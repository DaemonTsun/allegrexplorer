
#include "backends/imgui_impl_vulkan.h"

#include "shl/format.hpp"
#include "shl/string.hpp"
#include "shl/assert.hpp"
#include "shl/allocator_arena.hpp"
#include "shl/print.hpp"

#include "allegrex/disassemble.hpp"

#include "allegrexplorer_info.hpp"
#include "allegrexplorer_context.hpp"
#include "allegrexplorer_settings.hpp"

#include "psp_module_info_window.hpp"
#include "disassembly_window.hpp"
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
#define FRAME_RAM (1 << 20) // 1 MB
static arena _frame_memory{};

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
    allegrexplorer_settings *settings = settings_get();

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
                imgui_open_global_popup(POPUP_OPEN_ELF);

            if (ImGui::MenuItem("Export decrypted ELF...", "Ctrl+Shift+E", nullptr, actx.disasm.psp_module.elf_size > 0))
                imgui_open_global_popup(POPUP_EXPORT_DECRYPTED_ELF);

            if (ImGui::MenuItem("Export disassembly...", "", nullptr, actx.disasm.psp_module.elf_size > 0))
                imgui_open_global_popup(POPUP_EXPORT_DISASSEMBLY);

            ImGui::Separator();

            if (ImGui::MenuItem("Close", "Ctrl+W"))
                window_close(actx.window);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Back", "Alt+Left", nullptr, history_can_go_back()))
                history_go_back();

            if (ImGui::MenuItem("Forward", "Alt+Right", nullptr, history_can_go_forward()))
                history_go_forward();

            if (ImGui::MenuItem("Goto Address / Symbol", "Ctrl+G"))
                imgui_open_global_popup(POPUP_GOTO);

            ImGui::Separator();

            if (ImGui::BeginMenu("Disassembly"))
            {
                // ImGui::MenuItem("Display instruction ELF offset", NULL, &settings->disassembly.show_instruction_elf_offset);
                ImGui::MenuItem("Display instruction Vaddr", NULL, &settings->disassembly.show_instruction_vaddr);
                ImGui::MenuItem("Display instruction Opcode", NULL, &settings->disassembly.show_instruction_opcode);
                
                ImGui::EndMenu();
            }

            ImGui::Separator();

            ImGui::MenuItem("Display debug info", nullptr, &actx.show_debug_info);

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

static void _sections_window()
{
    ImGui::PushFont(actx.ui.fonts.mono);
    
    if (ImGui::Begin("Sections"))
    {
        auto *dsecs = &actx.disasm.disassembly_sections;

        for_array(_sec_i, dsec, dsecs)
        {
            ImGui::PushID(_sec_i);

            if (ImGui::TreeNode("Section", "0x%08x %s", dsec->section->vaddr, dsec->section->name))
            {
                float char_width = actx.ui.fonts.mono->Glyphs['x'].AdvanceX;
                ImGui::PushItemWidth(12 * char_width);
                ImGui::InputScalar("ELF Offset", ImGuiDataType_U32, &dsec->section->content_offset,
                                                 nullptr, nullptr, U32_FORMAT, ImGuiInputTextFlags_ReadOnly);
                ImGui::InputScalar("ELF Size",   ImGuiDataType_U32, &dsec->section->content_size,
                                                 nullptr, nullptr, U32_FORMAT, ImGuiInputTextFlags_ReadOnly);
                ImGui::InputScalar("Vaddr", ImGuiDataType_U32, &dsec->section->vaddr,
                                            nullptr, nullptr, VADDR_FORMAT, ImGuiInputTextFlags_ReadOnly);
                ImGui::InputScalar("End Vaddr", ImGuiDataType_U32, &dsec->vaddr_end,
                                            nullptr, nullptr, VADDR_FORMAT, ImGuiInputTextFlags_ReadOnly);
                ImGui::InputScalar("# Instructions", ImGuiDataType_U32, &dsec->instruction_count,
                                            nullptr, nullptr, "%d", ImGuiInputTextFlags_ReadOnly);
                ImGui::InputScalar("# Jumps", ImGuiDataType_U32, &dsec->jump_count,
                                            nullptr, nullptr, "%d", ImGuiInputTextFlags_ReadOnly);
                ImGui::InputScalar("# Branches", ImGuiDataType_U32, &dsec->branch_count,
                                            nullptr, nullptr, "%d", ImGuiInputTextFlags_ReadOnly);
                ImGui::InputScalar("# Functions", ImGuiDataType_U32, &dsec->function_count,
                                            nullptr, nullptr, "%d", ImGuiInputTextFlags_ReadOnly);
                ImGui::PopItemWidth();

                if (ImGui::TreeNode("Functions"))
                {
                    for (s32 i = 0; i < dsec->jump_count; ++i)
                    {
                        jump_destination *jmp = dsec->jumps + i;

                        if (jmp->type != jump_type::Jump)
                            continue;

                        ImGui::Text("0x%08x", jmp->address);
                        ImGui::SameLine();

                        if (ImGui::SmallButton(address_label(*jmp)))
                            goto_address(jmp->address);
                    }

                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }
    ImGui::End();

    ImGui::PopFont();
}

/*
static void _jump_history_panel(ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Jump history"))
    {
    }

    ImGui::End();
}
*/

static void _debug_info_window()
{
    if (ImGui::Begin("Debug Info"))
    {
        ImGui::Text("debug text");
    }

    ImGui::End();
}


// disassembly_window.cpp
void format_instruction(string *out, instruction *instr, jump_destination *out_jump);

static void _show_popups()
{
    static char filebuf[4096] = {};

    if_imgui_begin_global_modal_popup(POPUP_OPEN_ELF)
    {
        if (FsUi::FileDialog(POPUP_OPEN_ELF, filebuf, 4095, FsUi_DefaultDialogFilter,
                    FsUi_FilepickerFlags_NoDirectories | FsUi_FilepickerFlags_SelectionMustExist))
        {
            ImGui::CloseCurrentPopup();
            
            const_string path = to_const_string(filebuf);

            if (!is_blank(path))
            {
                error err{};
                _load_psp_elf(path.c_str, &err);
            }
        }

        ImGui::EndPopup();
    }

    if_imgui_begin_global_modal_popup(POPUP_GOTO)
    {
        u32 goto_addr = 0;

        if (popup_goto(&goto_addr))
        {
            if (goto_addr != max_value(u32))
                goto_address(goto_addr);

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if_imgui_begin_global_modal_popup(POPUP_EXPORT_DECRYPTED_ELF)
    {
        if (FsUi::FileDialog(POPUP_EXPORT_DECRYPTED_ELF, filebuf, 4095,
                    "PSP Elf (bin)|*.bin|Any file|*.*",
                    FsUi_FilepickerFlags_NoDirectories))
        {
            ImGui::CloseCurrentPopup();
            
            const_string path = to_const_string(filebuf);

            if (!is_blank(path))
            {
                error err{};
                io_handle f = io_open(path.c_str, open_mode::WriteTrunc, &err);

                if (f == INVALID_IO_HANDLE)
                    log_error(tformat("could not open file to export decrypted elf %s", path), &err);
                else
                {
                    defer { io_close(f); };

                    elf_psp_module *mod = &actx.disasm.psp_module;

                    if (io_write(f, mod->elf_data, mod->elf_size, &err) < 0)
                        log_error(tformat("could not write decrypted elf to %s", path), &err);
                    else
                        log_message(tformat("successfully exported decrypted elf to %s", path));
                }
            }
        }

        ImGui::EndPopup();
    }

    if_imgui_begin_global_modal_popup(POPUP_EXPORT_DISASSEMBLY)
    {
        if (FsUi::FileDialog(POPUP_EXPORT_DISASSEMBLY, filebuf, 4095,
                    "Disassembly (.txt, .asm, .s)|*.txt;*.asm;*.s|Any file|*.*",
                    FsUi_FilepickerFlags_NoDirectories))
        {
            ImGui::CloseCurrentPopup();
            
            const_string path = to_const_string(filebuf);

            if (!is_blank(path))
            {
                error err{};
                io_handle f = io_open(path.c_str, open_mode::WriteTrunc, &err);

                if (f == INVALID_IO_HANDLE)
                    log_error(tformat("could not open file to export decrypted elf %s", path), &err);
                else
                {
                    defer { io_close(f); };

                    string line{};
                    defer { free(&line); };

                    for_array(dsec, &actx.disasm.disassembly_sections)
                    {
                        clear(&line);
                        
                        format(&line, line.size, "\n// section %s\n", dsec->section->name);
                        io_write(f, line.data, line.size);

                        for (s64 i = 0; i < dsec->instruction_count; ++i)
                        {
                            instruction *instr = dsec->instructions + i;

                            clear(&line);

                            format(&line, line.size, "/* %08x %08x %08x %-32s */ ",
                                    (u32)dsec->section->content_offset + i * (u32)sizeof(u32),
                                    instr->address,
                                    instr->opcode,
                                    address_label(instr->address));

                            format_instruction(&line, instr, nullptr);

                            format(&line, line.size, "\n");
                            io_write(f, line.data, line.size);
                        }
                    }

                    log_message(tformat("successfully exported disassembly to %s", path));
                }
            }
        }

        ImGui::EndPopup();
    }
}

static void _process_inputs()
{
    // this exists so we can process inputs during an imgui frame
#define KEY_RIGHT              262
#define KEY_LEFT               263

    for_array(input, &_inputs_to_process)
    {
        bool shift = (input->mods & 1) == 1;
        bool ctrl  = (input->mods & 2) == 2;
        bool alt   = (input->mods & 4) == 4;
        bool pressed = input->action == 1;

        // Ctrl
        if (pressed && ctrl && !alt && !shift)
        {
            switch (input->key)
            {
            case 'O': imgui_open_global_popup(POPUP_OPEN_ELF); break;
            case 'G': imgui_open_global_popup(POPUP_GOTO); break;
            case 'W': window_close(actx.window); break;
            }
        }

        // Alt
        if (pressed && alt && !ctrl && !shift)
        {
            switch (input->key)
            {
            case KEY_LEFT:  history_go_back(); break;
            case KEY_RIGHT: history_go_forward(); break;
            }
        }

        // Ctrl+Shift
        if (pressed && ctrl && shift && !alt)
        {
            switch (input->key)
            {
            case 'E':  if (actx.disasm.psp_module.elf_size > 0) imgui_open_global_popup(POPUP_EXPORT_DECRYPTED_ELF); break;
            }
        }
    }

    clear(&_inputs_to_process);
}

static void _update(GLFWwindow *_, double dt)
{
    arena mem = _frame_memory;
    actx.global_alloc = get_context_pointer()->allocator;
    actx.frame_alloc = arena_allocator(&mem);

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

        with_allocator(actx.frame_alloc)
        {
            _menu_bar();

            ImGuiID dockspace_id = ImGui::GetID("main_dock");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
            psp_module_info_window();

            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
            disassembly_window();

            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
            _sections_window();

            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
            log_window(actx.ui.fonts.mono);

            if (actx.show_debug_info)
            {
                ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
                _debug_info_window();
#ifndef NDEBUG
                ImGui::ShowDemoWindow();
#endif
            }
        }

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
    actx.disasm = {};

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
    init(&_frame_memory, FRAME_RAM);
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
    log_clear();

    free(&_frame_memory);
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
