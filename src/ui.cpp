
#include <stdarg.h>
#include "shl/format.hpp"
#include "shl/string.hpp"

#include "allegrexplorer_context.hpp"
#include "colors.hpp"
#include "ui.hpp"

void init(ui_elf_section *sec)
{
    sec->computed_height = -1.f;
    init(&sec->header);
    init(&sec->functions);
}

void free(ui_elf_section *sec)
{
    free(&sec->header);
    free(&sec->functions);
}

void init(ui_context *ctx)
{
    ctx->computed_height = -1.f;
    init(&ctx->sections);
    init(&ctx->section_search_results);
}

void free(ui_context *ctx)
{
    free(&ctx->section_search_results);
    free<true>(&ctx->sections);
}

float _recompute_function_height(ui_allegrex_function *func, float y_padding, float font_size, float item_spacing)
{
    float total_height = 0;

    // top
    total_height += y_padding;

    total_height += func->instruction_count * font_size;

    if (func->instruction_count > 0)
        total_height += (func->instruction_count - 1) * item_spacing;
    
    // bottom
    total_height += y_padding;

    func->computed_height = total_height;

    return total_height;
}

float recompute_function_height(ui_allegrex_function *func)
{
    float y_padding = ui_style_disassembly_y_padding;
    float item_spacing = ui_style_disassembly_item_spacing;
    float font_size = ImGui::GetFontSize();
    
    return _recompute_function_height(func, y_padding, font_size, item_spacing);
}

float _recompute_section_height(ui_elf_section *sec, float y_padding, float font_size, float item_spacing)
{
    float total_height = 0;

    // header line
    total_height += y_padding;
    total_height += font_size;
    total_height += item_spacing;

    for_array(_i, func, &sec->functions)
    {
        func->computed_y_offset = total_height + _i * item_spacing;
        total_height += _recompute_function_height(func, y_padding, font_size, item_spacing);
    }

    total_height += sec->functions.size * item_spacing;
    
    // bottom
    total_height += y_padding;

    sec->computed_height = total_height;

    return total_height;
}

float recompute_section_height(ui_elf_section *sec)
{
    float y_padding = ui_style_disassembly_y_padding;
    float item_spacing = ui_style_disassembly_item_spacing;
    float font_size = ImGui::GetFontSize();
    
    return _recompute_section_height(sec, y_padding, font_size, item_spacing);
}

float recompute_total_disassembly_height(ui_context *ctx)
{
    float y_padding = ui_style_disassembly_y_padding;
    float item_spacing = ui_style_disassembly_item_spacing;
    float font_size = ImGui::GetFontSize();

    float total_height = 0;
    total_height += y_padding;

    for_array(_i, _uisec, &ctx->sections)
    {
        _uisec->computed_y_offset = total_height + _i * item_spacing;

        total_height += _recompute_section_height(_uisec, y_padding, font_size, item_spacing);

        for_array(_func, &_uisec->functions)
            _func->computed_y_offset += _uisec->computed_y_offset;
    }

    if (ctx->sections.size > 0)
        total_height += (ctx->sections.size) * item_spacing;

    ctx->computed_height = total_height;

    return total_height;
}

float get_y_offset_of_address(u32 vaddr)
{
    // we're assuming disassembly height has been computed
    float y_padding = ui_style_disassembly_y_padding;
    float item_spacing = ui_style_disassembly_item_spacing;
    float font_size = ImGui::GetFontSize();

    float ret = -1.f;

    for_array(uisec, &ctx.ui.sections)
    {
        u32 sec_vaddr = uisec->section->vaddr; 

        if (vaddr < sec_vaddr)
            break;

        u64 sec_max_vaddr = sec_vaddr;

        if (uisec->instruction_data->instructions.size > 0)
            sec_max_vaddr += (uisec->instruction_data->instructions.size - 1) * sizeof(u32);
        
        if (vaddr > sec_max_vaddr)
            continue;

        // got the section the vaddr is probably in, find function
        for_array(func, &uisec->functions)
        {
            if (vaddr < func->vaddr)
                break;

            u64 func_max_vaddr = func->vaddr;

            if (func->instruction_count > 0)
                func_max_vaddr += (func->instruction_count - 1) * sizeof(u32);

            if (vaddr > func_max_vaddr)
                continue;

            u32 nth_instr = (vaddr - func->vaddr) / sizeof(u32);

            ret = func->computed_y_offset + y_padding + nth_instr * (font_size + item_spacing);
            break;
        }

        break;
    }
    
    return ret;
}

void ui_instruction_name_text(const instruction *inst)
{
    const char *name = get_mnemonic_name(inst->mnemonic);

    if (requires_vfpu_suffix(inst->mnemonic))
    {
        vfpu_size sz = get_vfpu_size(inst->opcode);
        const char *suf = size_suffix(sz);
        auto fullname = tformat("%%\0"_cs, name, suf);

        ImGui::Text(DISASM_MNEMONIC_FORMAT, fullname.c_str);
    }
    else
        ImGui::Text(DISASM_MNEMONIC_FORMAT, name);
}

void ui_instruction_arguments(instruction *inst)
{
    bool first = true;

    for (u32 i = 0; i < inst->argument_count; ++i)
    {
        ImGui::SameLine();

        instruction_argument *arg = inst->arguments + i;
        argument_type arg_type = inst->argument_types[i];

        first = false;

        switch (arg_type)
        {
        case argument_type::Invalid:
            ImGui::Text("[?invalid?]");
            break;

        case argument_type::MIPS_Register:
            ImGui::Text("%s", register_name(arg->mips_register));
            break;

        case argument_type::MIPS_FPU_Register:
            ImGui::Text("%s", register_name(arg->mips_fpu_register));
            break;

        case argument_type::VFPU_Register:
            ImGui::Text("%s%s", register_name(arg->vfpu_register),
                                size_suffix(arg->vfpu_register.size));
            break;

        case argument_type::VFPU_Matrix:
            ImGui::Text("%s%s", matrix_name(arg->vfpu_matrix)
                              , size_suffix(arg->vfpu_matrix.size));
            break;

        case argument_type::VFPU_Condition:
            ImGui::Text("%s", vfpu_condition_name(arg->vfpu_condition));
            break;

        case argument_type::VFPU_Constant:
            ImGui::Text("%s", vfpu_constant_name(arg->vfpu_constant));
            break;

        case argument_type::VFPU_Prefix_Array:
        {
            vfpu_prefix_array *arr = &arg->vfpu_prefix_array;
            ImGui::Text("[%s,%s,%s,%s]", vfpu_prefix_name(arr->data[0])
                                       , vfpu_prefix_name(arr->data[1])
                                       , vfpu_prefix_name(arr->data[2])
                                       , vfpu_prefix_name(arr->data[3])
            );
            break;
        }

        case argument_type::VFPU_Destination_Prefix_Array:
        {
            vfpu_destination_prefix_array *arr = &arg->vfpu_destination_prefix_array;
            ImGui::Text("[%s,%s,%s,%s]", vfpu_destination_prefix_name(arr->data[0])
                                       , vfpu_destination_prefix_name(arr->data[1])
                                       , vfpu_destination_prefix_name(arr->data[2])
                                       , vfpu_destination_prefix_name(arr->data[3])
            );
            break;
        }

        case argument_type::VFPU_Rotation_Array:
        {
            vfpu_rotation_array *arr = &arg->vfpu_rotation_array;
            ImGui::Text("[%s", vfpu_rotation_name(arr->data[0]));

            for (int i = 1; i < arr->size; ++i)
                ImGui::Text(",%s", vfpu_rotation_name(arr->data[i]));

            ImGui::Text("]");
            break;
        }

        case argument_type::PSP_Function_Pointer:
        {
            const psp_function *sc = arg->psp_function_pointer;
            ImGui::Text("%s <0x%08x>", sc->name, sc->nid);
            break;
        }

#define ARG_TYPE_FORMAT(out, arg, ArgumentType, UnionMember, FMT) \
case argument_type::ArgumentType: \
    ImGui::Text(FMT, arg->UnionMember.data);\
    break;

        ARG_TYPE_FORMAT(out, arg, Shift, shift, "%#x");

        case argument_type::Coprocessor_Register:
        {
            coprocessor_register *reg = &arg->coprocessor_register;
            ImGui::Text("[%u, %u]", reg->rd, reg->sel);
            break;
        }

        case argument_type::Base_Register:
            ImGui::Text("(%s)", register_name(arg->base_register.data));
            break;

        case argument_type::Jump_Address:
        {
            u32 addr = arg->jump_address.data;
            const char *name = address_label(addr);

            ui_address_button(addr, name);

            break;
        }

        case argument_type::Branch_Address:
        {
            u32 addr = arg->branch_address.data;
            ui_address_button(addr, ".L%08x", addr);
            break;
        }

        ARG_TYPE_FORMAT(out, arg, Memory_Offset, memory_offset, "%#x");
        ARG_TYPE_FORMAT(out, arg, Immediate_u32, immediate_u32, "%#x");
        case argument_type::Immediate_s32:
        {
            s32 d = arg->immediate_s32.data;

            if (d < 0)
                ImGui::Text("-%#x", -d);
            else
                ImGui::Text("%#x", d);

            break;
        }

        ARG_TYPE_FORMAT(out, arg, Immediate_u16, immediate_u16, "%#x");
        case argument_type::Immediate_s16:
        {
            s16 d = arg->immediate_s16.data;

            if (d < 0)
                ImGui::Text("-%#x", -d);
            else
                ImGui::Text("%#x", d);

            break;
        }

        ARG_TYPE_FORMAT(out, arg, Immediate_u8,  immediate_u8,  "%#x");
        ARG_TYPE_FORMAT(out, arg, Immediate_float, immediate_float, "%f");
        ARG_TYPE_FORMAT(out, arg, Condition_Code, condition_code, "(CC[%#x])");
        ARG_TYPE_FORMAT(out, arg, Bitfield_Pos, bitfield_pos, "%#x");
        ARG_TYPE_FORMAT(out, arg, Bitfield_Size, bitfield_size, "%#x");

        // case argument_type::Extra:
        ARG_TYPE_FORMAT(out, arg, String, string_argument, "%s");

        default:
            break;
        }
    }
}

void ui_set_jump_target_address(u32 address)
{
    ctx.ui.do_jump = true;
    ctx.ui.jump_address = address;
    ctx.ui.jump_y_offset = get_y_offset_of_address(ctx.ui.jump_address);
}

void ui_do_jump_to_target_address()
{
    if (!ctx.ui.do_jump)
        return;

    auto wsize = ImGui::GetWindowSize();
    float total_height = ctx.ui.computed_height;

    ctx.ui.do_jump = false;
    
    float offset = ctx.ui.jump_y_offset;

    if (offset < 0.f)
        return;

    // offset -= wsize.y / 2;

    if (offset < 0.f)
        offset = 0;

    if (offset > total_height - wsize.y)
        offset = total_height - wsize.y;

    ImGui::SetScrollY(offset);
}

void ui_address_button(u32 target_address, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    static char _buf[256];
    const char *display_text = _buf;

    if (index_of(to_const_string(fmt), '%') == -1)
        display_text = fmt;
    else
    {
        int len = vsnprintf(_buf, 256, fmt, args);

        if (len < 0)
            return;
    }

    if (ImGui::SmallButton(display_text))
        ui_set_jump_target_address(target_address);

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, (u32)colors::section_tooltip_color);
        ImGui::SetTooltip("%08x", target_address);
        ImGui::PopStyleColor();
    }
}
