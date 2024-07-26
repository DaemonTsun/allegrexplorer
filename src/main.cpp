
#include "backends/imgui_impl_vulkan.h"

#include "shl/format.hpp"
#include "shl/string.hpp"
#include "shl/assert.hpp"
#include "shl/print.hpp"

#include "allegrex/disassemble.hpp"

#include "allegrexplorer_info.hpp"
#include "allegrexplorer_context.hpp"
#include "allegrexplorer_settings.hpp"

#include "window/colorscheme.hpp"
#include "window/window_imgui_util.hpp"

#define U32_FORMAT   "%08x"
#define VADDR_FORMAT "0x%08x"

#define DISASM_LINE_FORMAT "%08x %08x"
#define DISASM_MNEMONIC_FORMAT "%-10s"

static void _imgui_menu_bar()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
            {
                // TODO: implement this
            }

            if (ImGui::MenuItem("Close", "Ctrl+W"))
                window_close(actx.window);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Toggle debug info", nullptr, &actx.show_debug_info);

            if (ImGui::MenuItem("Goto Address / Symbol", "Ctrl+G"))
            {
                // open_goto_popup();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::BeginMenu("Colorscheme"))
            {
                static const colorscheme *schemes = nullptr;
                static int count = 0;
                static int selection = 0;

                if (schemes == nullptr)
                    colorscheme_get_all(&schemes, &count);

                for (int i = 0; i < count; i++)
                {
                    if (schemes + i == colorscheme_get_current())
                        selection = i;

                    if (ImGui::RadioButton(schemes[i].name, &selection, i))
                    {
                        selection = i;
                        colorscheme_set(schemes + i);
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

static void _imgui_side_panel(ImGuiID dockspace_id)
{
    elf_psp_module *mod = &actx.disasm.psp_module;
    prx_sce_module_info *mod_info = &mod->module_info;

    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Module Info");

    ImGui::PushFont(actx.ui.fonts.mono);
    ImGui::InputText("Module Name", mod_info->name, PRX_MODULE_NAME_LEN, ImGuiInputTextFlags_ReadOnly);

    int attr = mod_info->attribute;
    ImGui::InputInt("Attributes", &attr, 0, 0, ImGuiInputTextFlags_ReadOnly
                                             | ImGuiInputTextFlags_CharsHexadecimal);


    u32 v1 = mod_info->version[0];
    ImGui::InputScalar("Version 1", ImGuiDataType_U32, &v1, nullptr, nullptr, U32_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);

    u32 v2 = mod_info->version[1];
    ImGui::InputScalar("Version 2", ImGuiDataType_U32, &v2, nullptr, nullptr, U32_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    
    ImGui::InputScalar("gp", ImGuiDataType_U32, &mod_info->gp, nullptr, nullptr, VADDR_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    
    ImGui::InputScalar("Export start", ImGuiDataType_U32, &mod_info->export_offset_start,
                       nullptr, nullptr, VADDR_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    ImGui::InputScalar("Export end", ImGuiDataType_U32, &mod_info->export_offset_end,
                       nullptr, nullptr, VADDR_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    
    ImGui::InputScalar("Import start", ImGuiDataType_U32, &mod_info->import_offset_start,
                       nullptr, nullptr, VADDR_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    ImGui::InputScalar("Import end", ImGuiDataType_U32, &mod_info->import_offset_end,
                       nullptr, nullptr, VADDR_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    ImGui::PopFont();

    /*
    ImGui::ColorPicker4("a", (float*)&section_color);
    ImGui::ColorPicker4("b", (float*)&function_color);
    */

    ImGui::End();
}

static void _main_panel(ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Disassembly"))
    {

    }

    ImGui::End();
}

static void _sections_panel(ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Sections"))
    {

    }

    ImGui::End();
}

static void _jump_history_panel(ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Jump history"))
    {
    }

    ImGui::End();
}

static void _debug_info_panel(ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Debug Info"))
    {
        ImGui::Text("debug text");
    }

    ImGui::End();
}

static void _show_popups(ImGuiID dockspace_id)
{
    // TODO: implement
}

static void _update(GLFWwindow *_, double dt)
{
    imgui_new_frame();

    int windowflags = ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoDecoration
                    | ImGuiWindowFlags_MenuBar
                    | ImGuiWindowFlags_NoBackground;

    imgui_set_next_window_full_size();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(allegrexplorer_NAME, nullptr, windowflags);
    ImGui::PopStyleVar();

    _imgui_menu_bar();

    ImGuiID dockspace_id = ImGui::GetID("main_dock");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    _imgui_side_panel(dockspace_id);
    _main_panel(dockspace_id);
    _sections_panel(dockspace_id);
    _jump_history_panel(dockspace_id);

    if (actx.show_debug_info)
        _debug_info_panel(dockspace_id);

    _show_popups(dockspace_id);

    ImGui::End();

    imgui_end_frame();
}

static bool _load_psp_elf(const char *path, error *err)
{
    free(&actx);
    init(&actx);

    if (!disassemble_psp_elf(path, &actx.disasm, err))
        return false;

    return true;
}


static void _key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    bool ctrl = (mods & 2) == 2;
    bool pressed = action == 1;

    if (pressed && ctrl && key == 'G')
    {
        // open_goto_popup();
    }
    else if (pressed && ctrl && key == 'W')
        window_close(window);

    // TODO: forward & backward
}

static void _setup()
{
    init(&actx.disasm);

    window_init();

    // the size is just a placeholder, since we don't load imgui settings (which hold
    // the window size, position, etc), before creating the window.
    actx.window = window_create(allegrexplorer_NAME, 1600, 900);
    window_set_keyboard_callback(actx.window, _key_callback);

    imgui_init(actx.window);

    // settings
    settings_init();

    ImGui::LoadIniSettingsFromDisk(ImGui::GetIO().IniFilename);

    allegrexplorer_settings *settings = settings_get();

    window_set_size(actx.window, settings->window.width, settings->window.height);

    if (settings->window.x != 0 && settings->window.y != 0)
        window_set_position(actx.window, settings->window.x, settings->window.y);

    if (settings->window.maximized)
        window_maximize(actx.window);

    // fonts
    float scale = window_get_scaling(actx.window);
    ui_load_fonts(&actx.ui, scale);
}

static void _cleanup()
{
    // save window size
    allegrexplorer_settings *settings = settings_get();
    
    window_get_size(actx.window, &settings->window.width, &settings->window.height);
    window_get_position(actx.window, &settings->window.x, &settings->window.y);
    settings->window.maximized = window_is_maximized(actx.window);

    imgui_exit(actx.window);
    window_destroy(actx.window);
    window_exit();

    free(&actx.disasm);
}

int main(int argc, const char *argv[])
{
    _setup();

    error err{};

    if (argc > 1)
        _load_psp_elf(argv[1], &err);

    window_event_loop(actx.window, _update);

    _cleanup();

    return 0;
}
