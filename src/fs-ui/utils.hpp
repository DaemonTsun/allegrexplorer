
// included by filepicker.cpp
#pragma once

#include "shl/time.hpp"

// fs ui utils
static int lexicoraphical_compare(const char *s1, const char *s2)
{
    for (; *s1 && *s2; s1++, s2++)
    {
        if (*s1 < *s2) return -1;
        if (*s1 > *s2) return  1;
    }

    if (*s2 != '\0') return -1;
    if (*s1 != '\0') return  1;

    return 0;
}

static int timespan_compare(const timespan *lhs, const timespan *rhs)
{
    if (lhs->seconds     < rhs->seconds)     return -1;
    if (lhs->seconds     > rhs->seconds)     return  1;
    if (lhs->nanoseconds < rhs->nanoseconds) return -1;
    if (lhs->nanoseconds > rhs->nanoseconds) return  1;

    return 0;
}

static void TextSlice(const_string str)
{
    ImGui::TextEx(str.c_str, str.c_str + str.size, 0);
}

// why does ImGui not support this again...?
static bool ButtonSlice(const char* label, const char *label_end, const ImVec2& size_arg, ImGuiButtonFlags flags)
{
    using namespace ImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, label_end, true);

    ImVec2 pos = window->DC.CursorPos;
    if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
        pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    RenderNavHighlight(bb, id);
    RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

    if (g.LogEnabled)
        LogSetNextTextDecoration("[", "]");
    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, label_end, &label_size, style.ButtonTextAlign, &bb);
    
    return pressed;
}

// FILTERS
struct fs_ui_dialog_filter_item
{
    const_string filename;  // most likely "*"
    const_string extension; // maybe ".*"
};

struct fs_ui_dialog_filter
{
    const_string label;
    array<fs_ui_dialog_filter_item> items;
};

static void init(fs_ui_dialog_filter *f)
{
    init(&f->items);
}

static void free(fs_ui_dialog_filter *f)
{
    free(&f->items);
}

inline static s64 _skip_whitespace(const_string str, s64 i)
{
    while (i >= 0 && i < str.size && is_space(str[i]))
        i += 1;

    return i;
}

/*
https://learn.microsoft.com/en-us/dotnet/api/microsoft.win32.filedialog.filter?view=windowsdesktop-8.0
general filter structure is

<label>"|"<name+extensions>["|"<label>"|"<name+extensions>]*

<name+extensions> = <name>"."<extension>[";"<name>"."<extension>]*

<label>, <name> and <extension> are strings.
*/
static s64 fs_ui_parse_filter(const_string str, s64 i, array<fs_ui_dialog_filter> *out_filters)
{
    i = _skip_whitespace(str, i);

    s64 label_start = i;

    while (i >= 0 && i < str.size && str[i] != '|')
        i += 1;

    assert(i >= 0 && i < str.size && str[i] == '|' && "parse error, expected |, got end or invalid char");

    s64 label_end = i;

    fs_ui_dialog_filter *filter = add_at_end(out_filters);
    init(filter);
    filter->label.c_str = str.c_str + label_start;
    filter->label.size  = label_end - label_start;

    i += 1;

    // names and extensions
    while (i >= 0 && i < str.size && str[i] != '|')
    {
        i = _skip_whitespace(str, i);
        s64 name_start = i;
        s64 name_end = -1;
        s64 ext_start = -1;
        s64 ext_end = -1;
        char c = '\0';

        while (i >= 0 && i < str.size)
        {
            c = str[i];

            if (c == '.' || c == ';' || c == '|')
                break;

            i += 1;
        }

        assert(i >= 0 && (i >= str.size || (c == '.' || c == ';' || c == '|')) && "parse error, unexpected character");

        name_end = i;
        assert(name_start != name_end);

        fs_ui_dialog_filter_item *item = add_at_end(&filter->items);
        fill_memory(item, 0);
        item->filename.c_str = str.c_str + name_start;
        item->filename.size  = name_end - name_start;

        if (i >= str.size || c == '|')
            break;

        if (c == '.')
        {
            ext_start = i;
            i += 1;

            while (i >= 0 && i < str.size)
            {
                c = str[i];

                if (c == ';' || c == '|')
                    break;

                i += 1;
            }
            
            assert(i >= 0 && (i >= str.size || (c == ';' || c == '|')) && "parse error, unexpected character");

            ext_end = i;
        }

        if (ext_start != -1 && ext_end != -1)
        {
            assert(ext_start != ext_end);
            item->extension.c_str = str.c_str + ext_start;
            item->extension.size  = ext_end - ext_start;
        }

        if (c == '|')
            break;

        i += 1;
    }

    if (i < str.size)
        i += 1;

    return i;
}

static void fs_ui_parse_filters(const_string str, array<fs_ui_dialog_filter> *out_filters)
{
    assert(out_filters != nullptr);

    clear(out_filters);

    if (is_blank(str))
        str = to_const_string(FsUi_DefaultDialogFilter);

    s64 i = 0;

    while (i >= 0 && i < str.size)
        i = fs_ui_parse_filter(str, i, out_filters);

    assert(i >= str.size && "parse error, there is remaining input");
}

static bool fs_ui_matches_filter(const_string str, fs_ui_dialog_filter *f)
{
    const_string filename{};
    const_string extension{};

    s64 found = ::index_of(str, '.');

    if (found < 0)
        found = str.size;

    filename = str;
    filename.size = found;

    if (found < str.size)
    {
        extension.c_str = str.c_str + found;
        extension.size  = str.size - found;
    }

    for_array(i, &f->items)
    {
        bool any_filename = i->filename == "*"_cs;
        bool any_extension = (i->extension.size == 0 || i->extension == ".*"_cs);

        if (any_filename && any_extension)
            return true;

        if ((!any_filename) && filename != i->filename)
            continue;

        if ((!any_extension) && extension != i->extension)
            continue;

        return true;
    }
    
    return false;
}
