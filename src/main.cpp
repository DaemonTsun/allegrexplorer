
#include "backends/imgui_impl_vulkan.h"

#include "shl/format.hpp"
#include "shl/string.hpp"
#include "shl/assert.hpp"
#include "shl/print.hpp"

#include "allegrex/disassemble.hpp"

#include "allegrexplorer_info.hpp"
#include "allegrexplorer_context.hpp"
#include "allegrexplorer_settings.hpp"

#include "psp_module_info_window.hpp"
#include "log_window.hpp"
#include "popups.hpp"

#include "window/colorscheme.hpp"
#include "window/window_imgui_util.hpp"
#include "fs-ui/filepicker.hpp"

struct glfw_key_input
{
    int key;
    int scancode;
    int action;
    int mods;
};

static array<glfw_key_input> _inputs_to_process{};

static bool _load_psp_elf(const char *path, error *err)
{
    free(&actx);
    init(&actx);

    if (!disassemble_psp_elf(path, &actx.disasm, err))
    {
        log_error(tformat("could not load psp elf from %s", path), err);
        return false;
    }

    log_message(tformat("loaded psp elf from %s", path));

    return true;
}

static void _menu_bar()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
                imgui_open_global_popup(POPUP_OPEN_ELF);

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

static void _show_popups()
{
    if_imgui_begin_global_modal_popup(POPUP_OPEN_ELF)
    {
        static char buf[4096] = {};

        if (FsUi::FileDialog(POPUP_OPEN_ELF, buf, 4095))
        {
            ImGui::CloseCurrentPopup();
            
            const_string path = to_const_string(buf);

            if (!is_blank(path))
            {
                error err{};
                _load_psp_elf(path.c_str, &err);
            }
        }

        ImGui::EndPopup();
    }
}

static void _process_inputs()
{
    // this exists so we can process inputs during an imgui frame

    for_array(input, &_inputs_to_process)
    {
        bool ctrl = (input->mods & 2) == 2;
        bool pressed = input->action == 1;

        if (pressed && ctrl)
        {
            switch (input->key)
            {
            case 'O': imgui_open_global_popup(POPUP_OPEN_ELF); break;
            case 'G': break; // TODO: goto
            case 'W': window_close(actx.window); break;
            }
        }

        // TODO: forward & backward
    }

    clear(&_inputs_to_process);
}

static void _update(GLFWwindow *_, double dt)
{
    imgui_new_frame();

    _process_inputs();

    int windowflags = ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoDecoration
                    | ImGuiWindowFlags_MenuBar
                    | ImGuiWindowFlags_NoBackground;

    imgui_set_next_window_full_size();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin(allegrexplorer_NAME, nullptr, windowflags);
    {
        ImGui::PopStyleVar();

        _menu_bar();

        ImGuiID dockspace_id = ImGui::GetID("main_dock");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
        psp_module_info_window();

        _main_panel(dockspace_id);
        _sections_panel(dockspace_id);
        _jump_history_panel(dockspace_id);

        ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
        log_window(actx.ui.fonts.mono);

        if (actx.show_debug_info)
            _debug_info_panel(dockspace_id);

        _show_popups();
    }
    ImGui::End();

    imgui_end_frame();
}

static void _key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)(window);
    add_at_end(&_inputs_to_process, glfw_key_input{key, scancode, action, mods});
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

    init(&actx);
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
