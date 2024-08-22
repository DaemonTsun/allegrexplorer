
#include "imgui_internal.h"
#include "shl/assert.hpp"
#include "shl/fixed_array.hpp"
#include "shl/string.hpp"
#include "shl/memory.hpp"
#include "window/colorscheme.hpp"

// include all the colorschemes
#include "window/colorschemes/light.hpp"
#include "window/colorschemes/dark.hpp"

constexpr fixed_array _colorschemes
{
    colorscheme{colorscheme_light_name, (colorscheme_apply_function)colorscheme_light_apply},
    colorscheme{colorscheme_dark_name,  (colorscheme_apply_function)colorscheme_dark_apply}
};

static const colorscheme *_find_colorscheme_by_name(const_string name)
{
    for_array(sc, &_colorschemes)
        if (to_const_string(sc->name) == name)
            return sc;

    return nullptr;
}

struct colorscheme_settings
{
    string colorscheme;
};

static void init(colorscheme_settings *settings)
{
    assert(settings != nullptr);
    fill_memory(settings, 0);
};

static void free(colorscheme_settings *settings)
{
    assert(settings != nullptr);
    free(&settings->colorscheme);
};

static colorscheme_settings _ini_settings;

static void _colorscheme_ClearAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler)
{
    (void)ctx;
    (void)handler;
    free(&_ini_settings);
    init(&_ini_settings);
}

static void *_colorscheme_ReadOpenFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name)
{
    (void)ctx;
    (void)handler;
    if (compare_strings(name, "Preferences") == 0)
        return &_ini_settings;

    return (void*)nullptr;
}

static void _colorscheme_ReadLineFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* _entry, const char* _line)
{
    (void)ctx;
    (void)handler;
    (void)_entry;

    const_string line = to_const_string(_line);

    if (begins_with(line, "Name="_cs))
    {
        const_string schemename = line;
        schemename.c_str += 5;
        schemename.size  -= 5;

        if (is_blank(schemename))
            return;

        const colorscheme *sc = _find_colorscheme_by_name(schemename);

        if (sc == nullptr)
            sc = _colorschemes.data;

        colorscheme_set(sc);
    }
}

static void _colorscheme_WriteAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    (void)ctx;

    if (is_blank(&_ini_settings.colorscheme))
        return;

    buf->appendf("[%s][Preferences]\n", handler->TypeName);
    buf->appendf("Name=%s\n", _ini_settings.colorscheme.data);

    buf->append("\n");
}

void colorscheme_init()
{
    init(&_ini_settings);

    ImGuiSettingsHandler ini_handler{};
    ini_handler.TypeName = "Colorscheme";
    ini_handler.TypeHash = ImHashStr("Colorscheme");
    ini_handler.ClearAllFn = _colorscheme_ClearAllFn;
    ini_handler.ReadOpenFn = _colorscheme_ReadOpenFn;
    ini_handler.ReadLineFn = _colorscheme_ReadLineFn;
    ini_handler.WriteAllFn = _colorscheme_WriteAllFn;
    ImGui::AddSettingsHandler(&ini_handler);
}

void colorscheme_free()
{
    free(&_ini_settings);
}

void colorscheme_get_all(const colorscheme **out, int *out_count)
{
    assert(out != nullptr);
    assert(out_count != nullptr);
    *out       = _colorschemes.data;
    *out_count = _colorschemes.size;
}

static const colorscheme *_current_scheme = nullptr;

const colorscheme *colorscheme_get_current()
{
    return _current_scheme;
}

void colorscheme_set(const colorscheme *scheme)
{
    assert(scheme != nullptr);
    
    ImGuiStyle *st = &ImGui::GetStyle();
    scheme->apply(st);
    set_string(&_ini_settings.colorscheme, scheme->name);
    _current_scheme = scheme;
}

void colorscheme_set_default()
{
    if (_current_scheme != nullptr)
        return;

    const colorscheme *dark = _colorschemes.data + 1;
    colorscheme_set(dark);
}
