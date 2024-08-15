
#include "shl/string.hpp"
#include "shl/format.hpp"
#include "allegrex/disassemble.hpp"

#include "allegrexplorer_context.hpp" // address_name / address_label
#include "allegrexplorer_settings.hpp"
#include "disassembly_window.hpp"

static struct _disassembly_goto_jump
{
    u32 address;
    bool do_jump;
} _disasm_jump;

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

void disassembly_window()
{
    allegrexplorer_settings *settings = settings_get();
    ImGui::PushFont(actx.ui.fonts.mono);

    if (ImGui::Begin("Disassembly"))
    {
        ImGuiStyle *style  = &ImGui::GetStyle();
        const float start_height = style->WindowPadding.y + style->FramePadding.y * 2;
        const float font_height  = actx.ui.fonts.mono->FontSize;
        const float line_height  = font_height + style->ItemSpacing.y;

        const float computed_disassembly_height = start_height
            + line_height * (actx.disasm.all_instructions.size)
            + font_height;

        // Set height upfront, so scrollbars are accurate
        ImGui::Dummy(ImVec2(0, computed_disassembly_height));
        ImGui::SetCursorPosY(start_height + font_height);

        if (_disasm_jump.do_jump)
        {
            _disasm_jump.do_jump = false;
            
            ImGui::SetScrollY(disassembly_address_to_offset(_disasm_jump.address) - ImGui::GetContentRegionAvail().y / 2);
        }

        // here we check which instructions we need to render given the current
        // scroll position and visible height, so we don't render _all_ instructions.
        float from_y = ImGui::GetScrollY();
        float to_y   = ImGui::GetScrollY() + ImGui::GetContentRegionAvail().y;
        
        if (from_y > to_y)
            to_y = from_y;

        s64 from_instr = (s64)((from_y - (start_height + font_height)) / line_height) - 4;
        s64 to_instr   = (s64)((to_y   - (start_height + font_height)) / line_height) + 4;

        from_instr = Clamp(from_instr, (s64)0, actx.disasm.all_instructions.size);
        to_instr   = Clamp(to_instr,   (s64)0, actx.disasm.all_instructions.size);

        ImGui::Text("%s%s%s%-32s",
                settings->disassembly.show_instruction_elf_offset ? "Offset   " : "",
                settings->disassembly.show_instruction_vaddr      ? "Vaddr    " : "",
                settings->disassembly.show_instruction_opcode     ? "Opcode   " : "",
                "Name/Symbol");

        string line{};

        array<instruction> *all_instructions = &actx.disasm.all_instructions;

        for (s64 i = from_instr; i < to_instr; ++i)
        {
            instruction *instr = all_instructions->data + i;
            ImGui::PushID(i);

            clear(&line);

            //if (settings->disassembly.show_instruction_elf_offset)
            //    format(&line, line.size, "%08x ", (u32)dsec->section->content_offset + i * (u32)sizeof(u32));

            if (settings->disassembly.show_instruction_vaddr)
                format(&line, line.size, "%08x ", instr->address);

            if (settings->disassembly.show_instruction_opcode)
                format(&line, line.size, "%08x ", instr->opcode);

            format(&line, line.size, "%-32s ", address_label(instr->address));

            jump_destination jmp{};
            jmp.address = max_value(u32);

            _format_instruction(&line, instr, &jmp);

            ImGui::SetCursorPosY(start_height + font_height + line_height * (i+1));
            ImGui::Text("%s", line.data);

            if (jmp.address != max_value(u32))
            {
                ImGui::SameLine(0, 0);

                if (ImGui::SmallButton(address_label(jmp.address)))
                    disassembly_goto_address(jmp.address);

                ImGui::SetItemTooltip("%08x", jmp.address);
            }

            ImGui::PopID();
        }

        free(&line);

        // break;
    }

    ImGui::End();
    ImGui::PopFont();
}

void disassembly_goto_address(u32 addr)
{
    _disasm_jump.address = addr;
    _disasm_jump.do_jump = true;
}

float disassembly_instruction_index_to_offset(s64 index)
{
    ImGuiStyle *style  = &ImGui::GetStyle();
    const float start_height = style->WindowPadding.y + style->FramePadding.y * 2;
    const float font_height  = actx.ui.fonts.mono->FontSize;
    const float line_height  = font_height + style->ItemSpacing.y;

    return start_height + font_height + line_height * (index+1);
}

s64   disassembly_offset_to_instruction_index(float offset)
{
    ImGuiStyle *style  = &ImGui::GetStyle();
    const float start_height = style->WindowPadding.y + style->FramePadding.y * 2;
    const float font_height  = actx.ui.fonts.mono->FontSize;
    const float line_height  = font_height + style->ItemSpacing.y;
    
    return (s64)((offset - (start_height + font_height)) / line_height) - 1;
}

float disassembly_address_to_offset(u32 address)
{
    s64 idx = instruction_index_by_vaddr(address);

    if (idx < 0)
        return -1.f;

    return disassembly_instruction_index_to_offset(idx);
}

u32   disassembly_offset_to_address(float offset)
{
    instruction *instrs = actx.disasm.all_instructions.data;
    s64 instr_count = actx.disasm.all_instructions.size;
    
    s64 idx = disassembly_offset_to_instruction_index(offset);

    if (idx < 0 || idx >= instr_count)
        return max_value(u32);

    return instrs[idx].address;
}
