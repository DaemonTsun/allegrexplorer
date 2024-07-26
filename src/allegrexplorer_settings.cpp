
#include "imgui_internal.h" // AddSettingsHandler

#include "shl/assert.hpp"
#include "shl/compare.hpp"
#include "shl/string.hpp"
#include "shl/memory.hpp"

#include "allegrexplorer_settings.hpp"

static allegrexplorer_settings _settings;

static void init(allegrexplorer_settings *settings)
{
    assert(settings != nullptr);
    fill_memory(settings, 0);

    settings->window.width = 1600;
    settings->window.height = 900;
    settings->window.x = 0;
    settings->window.y = 0;
};

static void free(allegrexplorer_settings *settings)
{
    assert(settings != nullptr);
};

static void _settings_ClearAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler)
{
    free(&_settings);
    init(&_settings);
}

static void *_settings_ReadOpenFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name)
{
    if (compare_strings(name, "Preferences") == 0)
        return &_settings;

    return (void*)nullptr;
}

static void _settings_ReadLineFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* _entry, const char* line)
{
    int x = 0;

    if (sscanf(line, "WindowWidth=%d",  &x) == 1) _settings.window.width = Max(x, 160);
    if (sscanf(line, "WindowHeight=%d", &x) == 1) _settings.window.height = Max(x, 90);
    if (sscanf(line, "WindowX=%d",  &x) == 1) _settings.window.x = x;
    if (sscanf(line, "WindowY=%d", &x) == 1) _settings.window.y = x;
    if (sscanf(line, "WindowMaximized=%d", &x) == 1) _settings.window.maximized = x == 1;
}

static void _settings_WriteAllFn(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    buf->appendf("[%s][Preferences]\n", handler->TypeName);
    buf->appendf("WindowWidth=%d\n", _settings.window.width);
    buf->appendf("WindowHeight=%d\n", _settings.window.height);
    buf->appendf("WindowX=%d\n", _settings.window.x);
    buf->appendf("WindowY=%d\n", _settings.window.y);
    buf->appendf("WindowMaximized=%d\n", _settings.window.maximized ? 1 : 0);

    buf->append("\n");
}

void settings_init()
{
    init(&_settings);

    ImGuiSettingsHandler ini_handler{};
    ini_handler.TypeName = "Allegrexplorer";
    ini_handler.TypeHash = ImHashStr("Allegrexplorer");
    ini_handler.ClearAllFn = _settings_ClearAllFn;
    ini_handler.ReadOpenFn = _settings_ReadOpenFn;
    ini_handler.ReadLineFn = _settings_ReadLineFn;
    ini_handler.WriteAllFn = _settings_WriteAllFn;
    ImGui::AddSettingsHandler(&ini_handler);
}

allegrexplorer_settings *settings_get()
{
    return &_settings;
}
