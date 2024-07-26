
#include <assert.h>
#include "backends/imgui_impl_vulkan.h"

#include "window/fonts.hpp"

#include "shl/format.hpp"
#include "shl/string.hpp"
#include "shl/array.hpp"
#include "shl/murmur_hash.hpp" // __LINE_HASH__

#include "allegrex/disassemble.hpp"

#include "allegrexplorer_info.hpp"
#include "allegrexplorer_context.hpp"
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
            /*
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
            {
                // TODO: implement this...
                // is there really no default file picker in imgui?
                // All the other ones I found were pretty bad...
            }
            */

            if (ImGui::MenuItem("Close", "Ctrl+W"))
                close_window(actx.window);

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
    ui_new_frame();

    int windowflags = ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoDecoration
                    | ImGuiWindowFlags_MenuBar
                    | ImGuiWindowFlags_NoBackground;

    ui_set_next_window_full_size();

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

    ui_end_frame();
}

static bool _load_psp_elf(const char *path, error *err)
{
    free(&actx);
    init(&actx);

    if (!disassemble_psp_elf(path, &actx.disasm, err))
        return false;

    return true;
}

#include "window/find_font.hpp"

static void _load_fonts()
{
    float scale = get_window_scaling(actx.window);
    int font_size = 20 * scale;

    ff_cache *fc = ff_load_font_cache();
    defer { ff_unload_font_cache(fc); };

    const char *font_paths_monospace_bold[font_paths_monospace_count * 2];

    for (int i = 0; i < font_paths_monospace_count; ++i)
    {
        font_paths_monospace_bold[i*2] = font_paths_monospace[i*2];
        font_paths_monospace_bold[i*2 + 1] = "Bold";
    }

    const char *ui_font_path = ff_find_first_font_path(fc, (const char**)font_paths_ui, font_paths_ui_count * 2, nullptr);
    const char *monospace_font_path = ff_find_first_font_path(fc, (const char**)font_paths_monospace, font_paths_monospace_count * 2, nullptr);
    const char *monospace_bold_font_path = ff_find_first_font_path(fc, (const char**)font_paths_monospace_bold, font_paths_monospace_count * 2, nullptr);

    assert(ui_font_path != nullptr);
    assert(monospace_font_path != nullptr);
    assert(monospace_bold_font_path != nullptr);

    ImGuiIO &io = ImGui::GetIO(); (void)io;
    actx.ui.fonts.ui = io.Fonts->AddFontFromFileTTF(ui_font_path, font_size);
    actx.ui.fonts.mono = io.Fonts->AddFontFromFileTTF(monospace_font_path, font_size);
    actx.ui.fonts.mono_bold = io.Fonts->AddFontFromFileTTF(monospace_bold_font_path, font_size);
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
        close_window(window);

    // TODO: forward & backward
}

int main(int argc, const char *argv[])
{
    error err{};

    init(&actx.disasm);

    window_init();

    actx.window = create_window(allegrexplorer_NAME, 1600, 900);
    set_window_keyboard_callback(actx.window, _key_callback);
    ui_init(actx.window);

    _load_fonts();

    if (argc > 1)
        _load_psp_elf(argv[1], &err);

    window_event_loop(actx.window, _update);

    ui_exit(actx.window);
    destroy_window(actx.window);
    window_exit();

    free(&actx.disasm);

    return 0;
}
