
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

            if (ImGui::BeginMenu("Disassembly"))
            {
                ImGui::MenuItem("Display instruction ELF offset", NULL, &settings->disassembly.show_instruction_elf_offset);
                ImGui::MenuItem("Display instruction Vaddr", NULL, &settings->disassembly.show_instruction_vaddr);
                ImGui::MenuItem("Display instruction Opcode", NULL, &settings->disassembly.show_instruction_opcode);
                
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

static void _format_instruction(string *out, instruction *instr, jump_destination *out_jump = nullptr)
{
    // format instruction mnemonic
    const char *instr_name = get_mnemonic_name(instr->mnemonic);

    if (requires_vfpu_suffix(instr->mnemonic))
    {
        vfpu_size sz = get_vfpu_size(instr->opcode);
        const char *suf = size_suffix(sz);
        auto fullname = tformat("%%"_cs, instr_name, suf);

        format(out, out->size, "%-10s", fullname);
    }
    else
        format(out, out->size, "%-10s", instr_name);

    // format instruction arguments
    bool first_arg = true;
    for (u32 i = 0; i < instr->argument_count; ++i)
    {
        instruction_argument *arg = instr->arguments + i;
        argument_type arg_type = instr->argument_types[i];

        if (!first_arg && arg_type != argument_type::Base_Register)
            format(out, out->size, ", ");

        first_arg = false;

        switch (arg_type)
        {
        case argument_type::Invalid:
            format(out, out->size, "[?invalid?]");
            break;

        case argument_type::MIPS_Register:
            format(out, out->size, "%s", register_name(arg->mips_register));
            break;

        case argument_type::MIPS_FPU_Register:
            format(out, out->size, "%s", register_name(arg->mips_fpu_register));
            break;

        case argument_type::VFPU_Register:
            format(out, out->size, "%s", register_name(arg->vfpu_register));
            break;

        case argument_type::VFPU_Matrix:
            format(out, out->size, "%s%s", matrix_name(arg->vfpu_matrix)
                                           , size_suffix(arg->vfpu_matrix.size));
            break;

        case argument_type::VFPU_Condition:
            format(out, out->size, "%s", vfpu_condition_name(arg->vfpu_condition));
            break;

        case argument_type::VFPU_Constant:
            format(out, out->size, "%s", vfpu_constant_name(arg->vfpu_constant));
            break;

        case argument_type::VFPU_Prefix_Array:
        {
            vfpu_prefix_array *arr = &arg->vfpu_prefix_array;
            format(out, out->size, "[%s,%s,%s,%s]", vfpu_prefix_name(arr->data[0])
                                       , vfpu_prefix_name(arr->data[1])
                                       , vfpu_prefix_name(arr->data[2])
                                       , vfpu_prefix_name(arr->data[3])
            );
            break;
        }

        case argument_type::VFPU_Destination_Prefix_Array:
        {
            vfpu_destination_prefix_array *arr = &arg->vfpu_destination_prefix_array;
            format(out, out->size, "[%s,%s,%s,%s]"
                                   , vfpu_destination_prefix_name(arr->data[0])
                                   , vfpu_destination_prefix_name(arr->data[1])
                                   , vfpu_destination_prefix_name(arr->data[2])
                                   , vfpu_destination_prefix_name(arr->data[3])
            );
            break;
        }

        case argument_type::VFPU_Rotation_Array:
        {
            vfpu_rotation_array *arr = &arg->vfpu_rotation_array;
            format(out, out->size, "[%s", vfpu_rotation_name(arr->data[0]));

            for (u32 j = 1; j < arr->size; ++j)
                format(out, out->size, ",%s", vfpu_rotation_name(arr->data[j]));

            format(out, out->size, "]");
            break;
        }

        case argument_type::PSP_Function_Pointer:
        {
            const psp_function *sc = arg->psp_function_pointer;
            format(out, out->size, "%s <0x%08x>", sc->name, sc->nid);
            break;
        }

#define ARG_TYPE_FORMAT(out, arg, ArgumentType, UnionMember, FMT) \
case argument_type::ArgumentType: \
    format(out, out->size, FMT, arg->UnionMember.data);\
    break;

        ARG_TYPE_FORMAT(out, arg, Shift, shift, "%#x");

        case argument_type::Coprocessor_Register:
        {
            coprocessor_register *reg = &arg->coprocessor_register;
            format(out, out->size, "[%u, %u]", reg->rd, reg->sel);
            break;
        }

        case argument_type::Base_Register:
            format(out, out->size, "(%s)", register_name(arg->base_register.data));
            break;

        case argument_type::Jump_Address:
            if (out_jump != nullptr)
                *out_jump = jump_destination{arg->jump_address.data, jump_type::Jump};
            else
                format(out, out->size, "%s", address_name(arg->jump_address.data));
            break;

        case argument_type::Branch_Address:
            if (out_jump != nullptr)
                *out_jump = jump_destination{arg->branch_address.data, jump_type::Branch};
            else
                format(out, out->size, "%s", address_name(arg->branch_address.data));
            break;

        case argument_type::Memory_Offset:
            format(out, out->size, "%#x", (u32)arg->memory_offset.data);
            break;
        ARG_TYPE_FORMAT(out, arg, Immediate_u32, immediate_u32, "%#x");
        case argument_type::Immediate_s32:
        {
            s32 d = arg->immediate_s32.data;

            if (d < 0)
                format(out, out->size, "-%#x", -d);
            else
                format(out, out->size, "%#x", d);

            break;
        }

        ARG_TYPE_FORMAT(out, arg, Immediate_u16, immediate_u16, "%#x");
        case argument_type::Immediate_s16:
        {
            s16 d = arg->immediate_s16.data;

            if (d < 0)
                format(out, out->size, "-%#x", -d);
            else
                format(out, out->size, "%#x", d);

            break;
        }

        ARG_TYPE_FORMAT(out, arg, Immediate_u8,    immediate_u8,    "%#x");
        ARG_TYPE_FORMAT(out, arg, Immediate_float, immediate_float, "%f");
        ARG_TYPE_FORMAT(out, arg, Condition_Code,  condition_code,  "(CC[%#x])");
        ARG_TYPE_FORMAT(out, arg, Bitfield_Pos,    bitfield_pos,    "%#x");
        ARG_TYPE_FORMAT(out, arg, Bitfield_Size,   bitfield_size,   "%#x");

        ARG_TYPE_FORMAT(out, arg, String, string_argument, "%s");

        case argument_type::Extra:
        case argument_type::MAX:
        default:
            break;
        }
    }
}

static void _disassembly_window()
{
    allegrexplorer_settings *settings = settings_get();
    ImGui::PushFont(actx.ui.fonts.mono);

    if (ImGui::Begin("Disassembly"))
    {
        auto *dsecs = &actx.disasm.disassembly_sections;

        ImGui::Text("%s%s%s%-32s",
                settings->disassembly.show_instruction_elf_offset ? "Offset   " : "",
                settings->disassembly.show_instruction_vaddr      ? "Vaddr    " : "",
                settings->disassembly.show_instruction_opcode     ? "Opcode   " : "",
                "Name/Symbol");

        for_array(_sec_i, dsec, dsecs)
        {
            ImGui::PushID(_sec_i);

            string line{};

            for (s32 i = 0; i < dsec->instruction_count; ++i)
            {
                instruction *instr = dsec->instructions + i;

                clear(&line);

                if (settings->disassembly.show_instruction_elf_offset)
                    format(&line, line.size, "%08x ", (u32)dsec->section->content_offset + i * (u32)sizeof(u32));

                if (settings->disassembly.show_instruction_vaddr)
                    format(&line, line.size, "%08x ", instr->address);

                if (settings->disassembly.show_instruction_opcode)
                    format(&line, line.size, "%08x ", instr->opcode);

                format(&line, line.size, "%-32s ", address_label(instr->address));

                jump_destination jmp{};
                jmp.address = max_value(u32);

                _format_instruction(&line, instr, &jmp);

                ImGui::Text("%s", line.data);

                if (jmp.address != max_value(u32))
                {
                    ImGui::SameLine(0, 0);

                    ImGui::SmallButton(address_label(jmp.address));
                    ImGui::SetItemTooltip("%08x", jmp.address);
                }
            }

            free(&line);

            // break;

            ImGui::PopID();
        }
    }

    ImGui::End();
    ImGui::PopFont();
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

                        // TODO: button to disassembly
                        ImGui::Text("0x%08x %s", jmp->address, address_label(*jmp));
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
    arena mem = _frame_memory;

    with_allocator(arena_allocator(&mem))
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

            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
            _disassembly_window();

            ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
            _sections_window();
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
