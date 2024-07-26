
/* Implementation of find_font.

On Linux, uses the fontconfig cache to find fonts.
On Windows, uses the Registry to find fonts.

See below for implementation-specific documentation.
*/
#include "shl/print.hpp"

#include "shl/defer.hpp"
#include "shl/array.hpp"
#include "shl/fixed_array.hpp"
#include "shl/string.hpp"
#include "shl/memory.hpp"
#include "shl/platform.hpp"
#include "shl/allocator.hpp"
#include "shl/hash_table.hpp"

#include "window/find_font.hpp"

#if Windows
#include <windows.h>

// there is no "Regular" style as far as I can tell, at least not for system fonts
#define FF_DEFAULT_STYLE ""

/* REGISTRY DOCUMENTATION

This one is simpler than fontconfig, fonts are registered in two Registry keys,
one for the system and one for the current user:

HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts
HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts

The former is the key for the installed system fonts, the latter
is the key for the user-installed fonts, which may be different per user.

The names of the values of those keys have the following schema:

"<Font Family> [Style] [(Type)]"
Font Family is always present.
Style is not present for "default" or "regular" styles, e.g. "Arial (TrueType)" does
not have a style name.
The (Type) is the type of font, although this is not used here, so we truncate
this information.

The value of the keys contain the filenames in the case of system fonts, or
full paths in the case of user-installed fonts.
System fonts are most likely stored in C:\Windows\Fonts, so that path may
be prepended to the filenames of system font registry values.
*/
#define REGISTRY_FONTS_KEY R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts)"
#define FONTS_SYSTEM_PATH  R"(C:\Windows\Fonts\)"

struct ff_cache
{
    hash_table<string, string> entries;
    string name_buffer;
    ::allocator allocator;
};

static void init(ff_cache *cache)
{
    init(&cache->entries);
    init(&cache->name_buffer);
}

static void free(ff_cache *cache)
{
    free<true, true>(&cache->entries);
    free(&cache->name_buffer);
}

static bool _load_registry_fonts_base(ff_cache *c, HKEY hkey, const char *base_path)
{
    HKEY fonts_key;
    LSTATUS ret = RegOpenKeyExA(hkey, REGISTRY_FONTS_KEY,
                                      0,
                                      KEY_READ | KEY_ENUMERATE_SUB_KEYS,
                                      &fonts_key);

    if (ret != ERROR_SUCCESS)
        return false;

    defer { RegCloseKey(fonts_key); };

    DWORD value_count = 0;

    ret = RegQueryInfoKeyA(fonts_key,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           &value_count,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr);

    if (ret != ERROR_SUCCESS)
        return false;

    if (value_count <= 0)
        return true;

#define MAX_VALUE_NAME 4096
#define MAX_VALUE_DATA 4096
    char value_name[MAX_VALUE_NAME] = {0};
    char value_data[MAX_VALUE_DATA] = {0};
    DWORD value_name_length = MAX_VALUE_NAME;
    DWORD value_data_length = MAX_VALUE_DATA;
    DWORD value_type = 0;

    for (DWORD i = 0; i < value_count; ++i)
    {
        value_name_length = MAX_VALUE_NAME;
        ret = RegEnumValueA(fonts_key,
                            i,
                            value_name,
                            &value_name_length,
                            nullptr,
                            nullptr,
                            nullptr,
                            nullptr);

        if (ret != ERROR_SUCCESS)
            return false;

        value_data_length = MAX_VALUE_DATA;

        ret = RegGetValueA(fonts_key,
                           nullptr,
                           value_name,
                           RRF_RT_REG_SZ,
                           &value_type,
                           value_data,
                           &value_data_length);

        if (ret != ERROR_SUCCESS)
            return false;

        const_string name = to_const_string(value_name, value_name_length);

        s64 idx = last_index_of(name, " (");

        if (idx != -1)
            name = to_const_string(value_name, idx);

        const_string value = to_const_string(value_data, value_data_length);

        string *path = search_by_hash(&c->entries, hash(name));

        if (path != nullptr)
            continue;

        string key = copy_string(name);
        path = add_element_by_key(&c->entries, &key);
        *path = copy_string(base_path);
        append_string(path, value);
    }

    return true;
}

static bool _load_registry_fonts(ff_cache *c)
{
    if (c == nullptr)
        return false;

    bool ret = _load_registry_fonts_base(c, HKEY_LOCAL_MACHINE, FONTS_SYSTEM_PATH);
    ret = ret && _load_registry_fonts_base(c, HKEY_CURRENT_USER, "");
    return ret;
}

#elif Linux
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "shl/streams.hpp"

#define FF_DEFAULT_STYLE "Regular"

struct ff_cache_entry_style
{
    string style_name;
    string path;
};

static void free(ff_cache_entry_style *entry_style)
{
    free(&entry_style->style_name);
    free(&entry_style->path);
}

struct ff_cache_entry
{
    array<ff_cache_entry_style> styles;
};

static void free(ff_cache_entry *entry)
{
    free<true>(&entry->styles);
}

struct ff_cache
{
    hash_table<string, ff_cache_entry> entries;
    ::allocator allocator;
};

static void init(ff_cache *cache)
{
    init(&cache->entries);
}

static void free(ff_cache *cache)
{
    free<true, true>(&cache->entries);
}

static ff_cache_entry_style *_ff_find_style(ff_cache_entry *e, const_string style)
{
    for_array(st, &e->styles)
        if (to_const_string(st->style_name) == style)
            return st;

    return nullptr;
}

/* FONTCONFIG DOCUMENTATION

Personally, I think the fontconfig code is unnecessarily confusing, mostly
because of the abundant macro use in C and support for what I believe are
legacy versions, so here's a basic implementation that only reads the
fontconfig cache (version 9), and only reads the names, styles and filepaths
of fonts.
*/

/* According to fontconfig there could be a lot more fontconfig cache
directories, but I'm not going to parse config files to find out where
cache files are.
These two paths are the simplest cache directories I could find.
Probably won't work on something like NixOS though.

Cache files have strange names which look like

<hash>-<endianness><wordsize>.cache-<fontconfig version>

so, for instance, fe547fea3a41b43a38975d292a2b19c7-le64.cache-9.
*/
constexpr fixed_array _fontconfig_cache_dirs = {
    "$HOME/.cache/fontconfig",
    "/var/cache/fontconfig"
};

/*
https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/src/fcint.h#L416
The Cache file header, which should appear at the start of a cache file.
magic is 0xFC02FC04 or 0xFC02FC05. One of those is for "MMAP" caches, which
I can only assume are directly mapped to memory, which means all the addresses
stored here (fontset_offset, ...) are not offsets, but memory addresses within
the file.
Needless to say, MMAP caches are not supported here (and I couldn't find any
cache files that used it either), but if you find any cache files with absurd
offsets, you'll know why.

It should also be noted that caches also differentiate between MMAP and offset
caches by having odd numbers as offsets, in which case they're offsets, see
https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/src/fcint.h#L152

so all offsets have to be modified with a (offset & ~1), to get rid of the odd offset.

Version is 9 here, but you will probably find caches of versions 7 and 8
floating around as well.

The other members are mostly irrelevant for find_font, unless you care very deeply
about security and need to verify filesize, checksums and directories.

The last relevant member is fontset_offset, which is the offset (relative to where
this structure is within the file, i.e. the beginning) to the set of fonts inside
the file.
*/
struct fontconfig_cache
{
    u32 magic;
    s32 version;
    s64 filesize;
    sys_int dir_name_offset;
    sys_int subdir_offset;
    s32 subdir_count;
    sys_int fontset_offset;
    s32 checksum;
    s64 checksum_nano;
};

/* 
https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/fontconfig/fontconfig.h#L280
(why is this in fontconfig.h and not fcint.h?)

Contains the number of "patterns" (set of attributes on a single font) as well
as the offset to said patterns.
It should be noted that all offsets within these structs is relative
_to the structure_, so if a fontset structure is at e.g. 0x0080 within the file
and the patterns_offset member of this struct has the value 0x0100, then the
offset points to the offset 0x0180 within the file.
 */
struct fontconfig_fontset
{
    int pattern_count;
    int sfont;
    sys_int patterns_offset;
};

/*
https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/fontconfig/fontconfig.h#L225

Again, a "pattern" (FcPattern) is a set of attributes of a font.
A pattern contains "elt"s (don't even ask me what that could mean), which are
the attributes of a font.
elts_offset points to the first elt, elts_count is the number of elts.
*/
struct fontconfig_pattern
{
    s32 elts_count;
    s32 size;
    sys_int elts_offset;
    int ref;
};

/*
https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/fontconfig/fontconfig.h#L218

Again, an elt is an attribute of a font.
Each elt has an object_id (an integer that denotes what kind of attribute
it is, e.g. the font family name, the font style, the font path, ...),
and an offset to a "value list".

We only use object_ids FC_FAMILY_OBJECT (1), FC_STYLE_OBJECT (3) and
FC_FILE_OBJECT (21), which are the font family name, font style name and
font file path respectively.
The rest can be found here:

https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/src/fcobjs.h

which is included inside an enum in fcint.h here:

https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/src/fcint.h#L1047
*/
struct fontconfig_elt
{
    int object_id;
    sys_int value_list_offset;
};

/*
https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/fontconfig/fontconfig.h#L204

The data type of a single value within a value list of an elt of a pattern.
We only use FcTypeString, but the rest are here for reference.
*/
enum fc_value_type
{
    FcTypeUnknown = -1,
    FcTypeVoid,
    FcTypeInteger,
    FcTypeDouble,
    FcTypeString,
    FcTypeBool,
    FcTypeMatrix,
    FcTypeCharSet,
    FcTypeFTFace,
    FcTypeLangSet,
    FcTypeRange
};

/*
https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/fontconfig/fontconfig.h#L265

A single value of a value list of an elt of a pattern.
The value member here is just a sys int (platform wordsize integer), in the
fontconfig source (see link above), this is a union of a bunch of different
types, however we only use string, which is an offset, so we just use value
as an offset to a string inside the cache file.
*/
struct fontconfig_value
{
    fc_value_type type;
    sys_int value;
};

/*
https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/src/fcint.h#L195

The list of values of an elt of a pattern.
next_offset points to the next value_list entry (of type FcValueList), relative
to the current FcValueList entry.
 */
struct fontconfig_value_list
{
    sys_int next_offset;
    fontconfig_value value;
    int binding;
};

/*
https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/src/fcint.h#L510

The constants for magic numbers so you know the file you're reading is hopefully
a fontconfig cache. Should appear at the very beginning of a file.
 */
#define FC_CACHE_MAGIC_MMAP     0xFC02FC04
#define FC_CACHE_MAGIC_ALLOC    0xFC02FC05
#define FC_CACHE_VERSION_NUMBER 9

/*
https://gitlab.freedesktop.org/fontconfig/fontconfig/-/blob/e3563fa20d3b04f6033eddf062c2d624036777f5/src/fcobjs.h

The object_ids that we use in this implementation.
 */
#define FC_FAMILY_OBJECT    1
#define FC_STYLE_OBJECT     3
#define FC_FILE_OBJECT      21

static string _find_fontconfig_cache_dir()
{
    string ret{};
    struct statx st{};

    for_array(ppath, &_fontconfig_cache_dirs)
    {
        set_string(&ret, *ppath);
        resolve_environment_variables(&ret, true);
        fill_memory(ret.data + ret.size, 0, ret.reserved_size - ret.size);

        if (statx(AT_FDCWD, ret.data, 0, STATX_TYPE, &st) != 0)
            continue;

        if (!S_ISDIR(st.stx_mode))
            continue;

        return ret;
    }

    set_string(&ret, "");

    return ret;
}

static void _find_cache_files(const_string dir, array<string> *cache_files)
{
    DIR *d = opendir(dir.c_str);

    if (d == nullptr)
        return;

    for (dirent *ent = readdir(d); ent != nullptr; ent = readdir(d))
    {
        const_string filename = to_const_string((const char*)ent->d_name);

        if (filename == "."_cs || filename == ".."_cs)
            continue;

        if (!contains(filename, ".cache"))
            continue;

        string *added_filename = add_at_end(cache_files);
        init(added_filename, dir);
        append_string(added_filename, '/');
        append_string(added_filename, filename);
    }

    closedir(d);
}

static int _fontconfig_find_pattern_object_index(const fontconfig_pattern *p, int object_id)
{
    fontconfig_elt *elts = (fontconfig_elt*)((char*)p + (p->elts_offset & ~1));

    int low = 0;
    int high = p->elts_count - 1;
    int c = 1;
    int mid = 0;

    while (low <= high)
    {
        mid = (low + high) >> 1;
        c = elts[mid].object_id - object_id;

        if (c == 0)
            return mid;

        if (c < 0)
            low = mid + 1;

        else
            high = mid - 1;
    }

    if (c < 0)
        mid++;

    return -(mid + 1);
}

static const char *_fontconfig_find_pattern_string_object(const fontconfig_pattern *p, int object_id)
{
    fontconfig_elt *elts = (fontconfig_elt*)((char*)p + (p->elts_offset & ~1));
    
    int index = _fontconfig_find_pattern_object_index(p, object_id);

    if (index < 0)
        return nullptr;

    fontconfig_elt *elt = elts + index;

    if (elt->value_list_offset == 0)
        return nullptr;

    fontconfig_value_list *list = (fontconfig_value_list*)((char*)elt + (elt->value_list_offset & ~1));
    fontconfig_value *val = nullptr;

    while (true)
    {
        val = &list->value;

        if (val->type == FcTypeString)
            return (char*)val + (val->value & ~1);

        if (list->next_offset == 0)
            break;

        list = (fontconfig_value_list*)((char*)list + (list->next_offset & ~1));
    }

    return nullptr;
}

static void _parse_fontconfig_cache_file(ff_cache *c, const_string filepath, string *buffer)
{
    if (!read_entire_file(filepath.c_str, buffer))
    {
        tprint("  could not read %\n", filepath);
        return;
    }

    memory_stream contents{};
    contents.data = buffer->data;
    contents.size = buffer->size - 1;
    contents.position = 0;
    contents.allocator = buffer->allocator;

    if (contents.size < (s64)sizeof(fontconfig_cache))
    {
        tprint("  invalid fontconfig cache: %\n", filepath);
        return;
    }

    fontconfig_cache fc_cache{};
    read(&contents, &fc_cache);

    if (fc_cache.magic != FC_CACHE_MAGIC_MMAP && fc_cache.magic != FC_CACHE_MAGIC_ALLOC)
    {
        tprint("  invalid magic number % in file: %\n", fc_cache.magic, filepath);
        return;
    }

    if (fc_cache.version != FC_CACHE_VERSION_NUMBER)
    {
        // tprint("  skipping version % in file: %\n", fc_cache.version, filepath);
        return;
    }

    fontconfig_fontset *fs = (fontconfig_fontset*)(contents.data + fc_cache.fontset_offset);

    if (fs == nullptr || fs->pattern_count <= 0)
        return;

    fs->patterns_offset &= ~1;
    s64 *font_offsets = (s64*)((char*)fs + fs->patterns_offset);

    for (int i = 0; i < fs->pattern_count; ++i)
    {
        s64 font_offset = font_offsets[i];
        font_offset &= ~1;

        auto *font = (fontconfig_pattern*)((char*)fs + (font_offset & ~1));

        fontconfig_elt *elts = (fontconfig_elt*)((char*)font + (font->elts_offset & ~1));
        (void)elts;
        
        const_string name  = to_const_string(_fontconfig_find_pattern_string_object(font, FC_FAMILY_OBJECT));
        const_string style = to_const_string(_fontconfig_find_pattern_string_object(font, FC_STYLE_OBJECT));
        const_string path  = to_const_string(_fontconfig_find_pattern_string_object(font, FC_FILE_OBJECT));

        ff_cache_entry *e = search_by_hash(&c->entries, hash(name));

        if (e == nullptr)
        {
            string key = copy_string(name);
            e = add_element_by_key(&c->entries, &key);
            init(&e->styles);
        }

        ff_cache_entry_style *st = _ff_find_style(e, style);

        if (st != nullptr)
        {
            /*
            if (st->path != path)
                tprint("  Warning: path mismatch in font %, style %: %, %\n", name, style, path, st->path);
            */
        }
        else
        {
            ff_cache_entry_style *newstyle = add_at_end(&e->styles);
            newstyle->style_name = copy_string(style);
            newstyle->path = copy_string(path);
        }
    }
}

static bool _load_fontconfig_cache(ff_cache *c)
{
    string fc_path = _find_fontconfig_cache_dir();
    defer { free(&fc_path); };

    if (is_empty(fc_path))
    {
        tprint("Error: no fontconfig cache path found\n");
        return false;
    }

    // tprint("found cache path: %\n", fc_path);

    array<string> cache_files{};
    defer { free<true>(&cache_files); };

    _find_cache_files(to_const_string(fc_path), &cache_files);

    if (cache_files.size == 0)
    {
        tprint("Error: no files found in %\n", fc_path);
        return false;
    }

    // buffer used for storing cache file contents.
    // is reused between files.
    string buf{};

    for_array(file, &cache_files)
        _parse_fontconfig_cache_file(c, *file, &buf);

    free(&buf);
    
    return true;
}
#endif

extern "C" ff_cache *ff_load_font_cache(void *_alloc)
{
    allocator a = default_allocator;

    if (_alloc != nullptr)
        a = *(allocator*)_alloc;

    ff_cache *ret = allocator_alloc_T(a, ff_cache);
    ret->allocator = a;

    with_allocator(a)
    {
#if Windows
        init(ret);

        if (!_load_registry_fonts(ret))
        {
            ff_unload_font_cache(ret);
            return nullptr;
        }
#elif Linux
        init(ret);

        if (!_load_fontconfig_cache(ret))
        {
            ff_unload_font_cache(ret);
            return nullptr;
        }

        /*
        put("\n\n");

        for_hash_table(font_name, entry, &ret->entries)
        {
            tprint("\n%:\n", font_name);

            for_array(st, &entry->styles)
                tprint("  %: %\n", st->style_name, st->path);
        }

        put("\n\n\n");
        */
#endif
    }

    return ret;
}

extern "C" void ff_unload_font_cache(ff_cache *cache)
{
    if (cache == nullptr)
        return;

    allocator a = cache->allocator;

    free(cache);
    allocator_dealloc_T(a, cache, ff_cache);
}

extern "C" const char *ff_find_font_path(ff_cache *cache, const char *font_name, const char *_style_name)
{
    if (cache == nullptr || font_name == nullptr)
        return nullptr;

    if (_style_name == nullptr || string_length(_style_name) == 0)
        _style_name = FF_DEFAULT_STYLE;

    const_string style_name = to_const_string(_style_name);

#if Windows
    hash_t h = 0;

    if (style_name.size > 0)
    {
        string *buf = &cache->name_buffer;
        set_string(buf, font_name);
        append_string(buf, ' ');
        append_string(buf, style_name);
        h = hash(buf);
    }
    else
        h = hash(font_name);

    string *ret = search_by_hash(&cache->entries, h);

    if (ret == nullptr)
        return nullptr;

    return ret->data;

#elif Linux
    ff_cache_entry *e = search_by_hash(&cache->entries, hash(font_name));

    if (e == nullptr)
        return nullptr;

    ff_cache_entry_style *st = _ff_find_style(e, style_name);

    if (st == nullptr)
        return nullptr;

    return st->path.data;
#else
    return nullptr;
#endif
}

extern "C" const char *ff_find_font_path_vague(ff_cache *cache, const char *_font_name, const char *_style_name)
{
    if (cache == nullptr || _font_name == nullptr)
        return nullptr;

    if (_style_name == nullptr || string_length(_style_name) == 0)
        _style_name = FF_DEFAULT_STYLE;

    const_string font_name  = to_const_string(_font_name);
    const_string style_name = to_const_string(_style_name);

#if Windows
    string *ret = nullptr;

    for_array(hentry, &cache->entries.data)
    {
        if (hentry->hash <= FIRST_HASH)
            continue;

        if (!begins_with(hentry->key, font_name))
            continue;

        if (!contains(hentry->key, style_name))
            continue;

        ret = &hentry->value;
        break;
    }

    if (ret == nullptr)
        return nullptr;

    return ret->data;
#elif Linux
    ff_cache_entry *e = nullptr;

    for_array(hentry, &cache->entries.data)
    {
        if (hentry->hash <= FIRST_HASH)
            continue;

        if (begins_with(hentry->key, font_name))
        {
            e = &hentry->value;
            break;
        }
    }

    if (e == nullptr)
        return nullptr;

    ff_cache_entry_style *st = nullptr;

    for_array(_st, &e->styles)
        if (begins_with(_st->style_name, style_name))
            st = _st;

    if (st == nullptr)
        return nullptr;

    return st->path.data;
#else
    return nullptr;
#endif
}

extern "C" const char *ff_find_first_font_path(ff_cache *cache, const char **font_names_and_styles, int count, int *found_index)
{
    if (font_names_and_styles == nullptr)
        return nullptr;

    // count must be even
    if (count <= 0 || ((count & 1) != 0))
        return nullptr;

#if Windows
    const char *ret = nullptr;

    for (int i = 0; i < count; i += 2)
    {
        const char *_font_name  = font_names_and_styles[i];
        const char *_style_name = font_names_and_styles[i + 1];

        ret = ff_find_font_path(cache, _font_name, _style_name);

        if (ret != nullptr)
        {
            if (found_index != nullptr)
                *found_index = i;

            break;
        }
    }

    return ret;
#elif Linux
    const_string style_name{};
    ff_cache_entry *e = nullptr;
    ff_cache_entry_style *st = nullptr;

    for (int i = 0; i < count; i += 2)
    {
        const char *_font_name  = font_names_and_styles[i];
        const char *_style_name = font_names_and_styles[i + 1];

        if (_style_name == nullptr || string_length(_style_name) == 0)
            _style_name = FF_DEFAULT_STYLE;

        e = search_by_hash(&cache->entries, hash(_font_name));

        if (e == nullptr)
            continue;

        style_name = to_const_string(_style_name);
        st = _ff_find_style(e, style_name);

        if (st == nullptr)
            continue;

        if (found_index != nullptr)
            *found_index = i;

        return st->path.data;
    }

    return nullptr;
#else
    return nullptr;
#endif
}

extern "C" const char *ff_find_first_font_path_vague(ff_cache *cache, const char **font_names_and_styles, int count, int *found_index)
{
    if (font_names_and_styles == nullptr)
        return nullptr;

    // count must be even
    if (count <= 0 || ((count & 1) != 0))
        return nullptr;

#if Windows
    const char *ret = nullptr;

    for (int i = 0; i < count; i += 2)
    {
        const char *_font_name  = font_names_and_styles[i];
        const char *_style_name = font_names_and_styles[i + 1];

        ret = ff_find_font_path_vague(cache, _font_name, _style_name);

        if (ret != nullptr)
        {
            *found_index = i;
            break;
        }
    }

    return ret;
#elif Linux
    const char *ret = nullptr;

    for (int i = 0; i < count; i += 2)
    {
        const char *_font_name  = font_names_and_styles[i];
        const char *_style_name = font_names_and_styles[i + 1];

        if (_style_name == nullptr || string_length(_style_name) == 0)
            _style_name = FF_DEFAULT_STYLE;

        ret = ff_find_font_path_vague(cache, _font_name, _style_name);

        if (ret != nullptr)
        {
            *found_index = i;
            break;
        }
    }

    return ret;
#else
    return nullptr;
#endif
}
