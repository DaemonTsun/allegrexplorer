
#include <assert.h>
#include "backends/imgui_impl_vulkan.h"

#include "fonts/fonts.hpp"

#include "shl/format.hpp"
#include "shl/string.hpp"
#include "shl/array.hpp"

#include "mg/mg.hpp"
#include "mg/impl/context.hpp"
#include "allegrex/disassemble.hpp"

#include "allegrexplorer_info.hpp"
#include "allegrexplorer_context.hpp"
#include "imgui_util.hpp"
#include "ui.hpp"

const ImColor section_color = COL(0xffbf98ff);
const ImColor function_color = COL(0xff6060ff);
const ImColor section_text_color = COL(0x000000ff);
ImFont *ui_font;
ImFont *mono_font;
ImFont *mono_bold_font;
ImFont *mono_italic_font;

static allegrexplorer_context ctx;

const char *address_name(u32 addr)
{
    // symbols
    elf_symbol *sym = ::search(&ctx.disasm.psp_module.symbols, &addr);

    if (sym != nullptr)
        return sym->name;

    // imports
    function_import *fimp = ::search(&ctx.disasm.psp_module.imports, &addr);

    if (fimp != nullptr)
        return fimp->function->name;

    /*
    // exports (unoptimized, but probably not too common)
    for_array(mod, conf->exported_modules)
    {
        for_array(func, &mod->functions)
            if (func->address == addr)
                return func->function->name;
    }
    */

    return "";
}

void imgui_menu_bar(mg::window *window)
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
            {
                // TODO: implement this...
                // is there really no default file picker in imgui?
                // All the other ones I found were pretty bad...
            }

            if (ImGui::MenuItem("Close", "Ctrl+W"))
                mg::close_window(window);

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void imgui_side_panel(mg::window *window, ImGuiID dockspace_id)
{
    elf_psp_module *mod = &ctx.disasm.psp_module;
    prx_sce_module_info *mod_info = &mod->module_info;

    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Module Info");

    ImGui::PushFont(mono_font);
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

    ImGui::End();
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
            const char *name = address_name(addr);

            if (compare_strings(name, "") != 0)
                ImGui::Text("%s", name);
            else
                ImGui::Text("func_%08x", addr);

            break;
        }

        case argument_type::Branch_Address:
        {
            u32 addr = arg->branch_address.data;
            ImGui::Text(".L%08x", addr);
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

float get_function_height(ui_allegrex_function *func, float y_padding, float font_size, float item_spacing)
{
    float total_height = 0;

    // top
    total_height += y_padding;

    total_height += func->instruction_count * font_size;

    if (func->instruction_count > 0)
        total_height += (func->instruction_count - 1) * item_spacing;
    
    // bottom
    total_height += y_padding;

    return total_height;
}

float get_section_height(ui_elf_section *section, float y_padding, float font_size, float item_spacing)
{
    float total_height = 0;

    // header line
    total_height += y_padding;
    total_height += font_size;
    total_height += item_spacing;

    for_array(func, &section->functions)
        total_height += get_function_height(func, y_padding, font_size, item_spacing);

    total_height += section->functions.size * item_spacing;
    
    // bottom
    total_height += y_padding;

    return total_height;
}

float get_total_disassembly_height()
{
    auto wpadding = ImGui::GetStyle().WindowPadding;
    float font_size = ImGui::GetFontSize();
    auto item_spacing = ImGui::GetStyle().ItemSpacing;

    float total_height = 0;

    for_array(_uisec, &ctx.sections)
        total_height += get_section_height(_uisec, wpadding.y, font_size, item_spacing.y);

    if (ctx.sections.size > 0)
        total_height += (ctx.sections.size) * item_spacing.y;

    return total_height;
}

void main_panel(mg::window *window, ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Disassembly");

    auto wsize = ImGui::GetWindowSize();
    auto wpadding = ImGui::GetStyle().WindowPadding;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {wpadding.x, 6});
    wpadding.y = 6;
    ui::padding sec_padding;
    sec_padding.left = 0;
    sec_padding.right = wpadding.x;
    sec_padding.top = wpadding.y;
    sec_padding.bottom = wpadding.y;

    float font_size = ImGui::GetFontSize();
    auto item_spacing = ImGui::GetStyle().ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {item_spacing.x, 16});
    item_spacing.y = 16;

    float total_height = get_total_disassembly_height();

    float view_min_y = ImGui::GetScrollY();
    float view_max_y = view_min_y + wsize.y;

    ImGui::BeginChild("content",
                      {wsize.x, total_height},
                      false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PushFont(mono_font);
    ImGui::PushStyleColor(ImGuiCol_Text, (u32)section_text_color);

    float current_height = 0;

    // printf("%f - %f\n", view_min_y, view_max_y);

    for_array(_i, uisec, &ctx.sections)
    {
        float sec_height = get_section_height(uisec, wpadding.y, font_size, item_spacing.y);

        if (current_height > view_max_y)
            break;

        if (current_height + sec_height < view_min_y)
        {
            ImGui::Dummy({0.f, sec_height});
            current_height += sec_height;
            current_height += item_spacing.y;
            continue;
        }

        // TODO: precalculate all of these and just check if its in view
        float current_sec_height = current_height;

        current_sec_height += wpadding.y;
        current_sec_height += font_size;
        current_sec_height += item_spacing.y;

        current_height += sec_height;
        current_height += item_spacing.y;

        elf_section *sec = uisec->section;
        u32 pos = sec->content_offset;

        ui::begin_group(section_color, sec_padding);

        ImGui::Text("%.1f Section %s", ImGui::GetCursorPosY(), sec->name);

        for_array(func, &uisec->functions)
        {
            if (func->instruction_count == 0)
                continue;

            float func_height = get_function_height(func, wpadding.y, font_size, item_spacing.y);

            if (current_sec_height > view_max_y)
                break;

            if (current_sec_height + func_height < view_min_y)
            {
                ImGui::Dummy({0.f, func_height});
                current_sec_height += func_height;
                current_sec_height += item_spacing.y;
                continue;
            }

            current_sec_height += func_height;
            current_sec_height += item_spacing.y;

            ui::begin_group(function_color, sec_padding);

            for (u64 i = 0; i < func->instruction_count; ++i)
            {
                instruction *inst = func->instructions + i;

                ImGui::Text("%.1f", ImGui::GetCursorPosY());
                ImGui::SameLine();
                ImGui::Text(ctx.address_name_format, address_name(inst->address));
                ImGui::SameLine();
                ImGui::Text(ctx.file_offset_format, pos);
                ImGui::SameLine();
                ImGui::Text(DISASM_LINE_FORMAT, inst->address, inst->opcode);
                ImGui::SameLine();
                ui_instruction_name_text(inst);
                ui_instruction_arguments(inst);

                pos += sizeof(u32);
            }

            ui::end_group();
        }

        ui::end_group();
    }

    ImGui::PopStyleColor();

    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    ImGui::End();
}

void sections_panel(mg::window *window, ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Sections");

    const u32 max_sec_name_size = 256;
    static char search_buf[max_sec_name_size] = {0};

    if (ImGui::InputText("Search", search_buf, max_sec_name_size))
    {
        clear(&ctx.section_search_results);

        if (!is_blank(search_buf))
        {
            for_array(usec, &ctx.sections)
                if (index_of(usec->header, search_buf) >= 0)
                    add_at_end(&ctx.section_search_results, usec);
        }
        else
        {
            for_array(usec, &ctx.sections)
                add_at_end(&ctx.section_search_results, usec);
        }
    }

    ImGui::PushFont(mono_font);

    for_array(usec_, &ctx.section_search_results)
    {
        ui_elf_section *usec = *usec_;

        if (ImGui::TreeNode((const char*)usec->header))
        {
            elf_section *sec = usec->section;

            ImGui::InputText("Name", (char*)sec->name, max_sec_name_size, ImGuiInputTextFlags_ReadOnly);

            ImGui::InputScalar("Vaddr", ImGuiDataType_U32, &sec->vaddr,
                               nullptr, nullptr, VADDR_FORMAT,
                               ImGuiInputTextFlags_ReadOnly);

            ImGui::InputScalar("Elf offset", ImGuiDataType_U32, &sec->content_offset,
                               nullptr, nullptr, VADDR_FORMAT,
                               ImGuiInputTextFlags_ReadOnly);

            ImGui::InputScalar("Size", ImGuiDataType_U64, &sec->content_size,
                               nullptr, nullptr, U32_FORMAT,
                               ImGuiInputTextFlags_ReadOnly);
            
            ImGui::TreePop();
        }
    }

    ImGui::PopFont();

    ImGui::End();
}

void update(mg::window *window, double dt)
{
    ui::new_frame(window);

    int windowflags = ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoDecoration
                    | ImGuiWindowFlags_MenuBar
                    | ImGuiWindowFlags_NoBackground;

    ui::set_next_window_full_size();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(allegrexplorer_NAME, nullptr, windowflags);
    ImGui::PopStyleVar();

    imgui_menu_bar(window);

    ImGuiID dockspace_id = ImGui::GetID("main_dock");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    imgui_side_panel(window, dockspace_id);
    main_panel(window, dockspace_id);
    sections_panel(window, dockspace_id);

    ImGui::End();

    ui::end_frame();
}

template<typename T>
constexpr inline T hex_digits(T x)
{
    T i = 0;

    while (x > 0)
    {
        x = x >> 4;
        ++i;
    }

    return i;
}

void prepare_disasm_ui_data()
{
    reserve(&ctx.sections, ctx.disasm.psp_module.sections.size);

    jump_destinations *jumps = &ctx.disasm.jumps;
    u32 jmp_i = 0;

    for_array(i, sec, &ctx.disasm.psp_module.sections)
    {
        ui_elf_section *uisec = ::add_at_end(&ctx.sections);
        init(uisec);

        uisec->header = copy_string(tformat(VADDR_FORMAT " %s", sec->vaddr, sec->name));
        uisec->section = sec;
        uisec->instruction_data = nullptr;
        
        if (i < ctx.disasm.instruction_datas.size)
        {
            uisec->instruction_data = ctx.disasm.instruction_datas.data + i;
            assert(uisec->instruction_data->section_index == i);
        }

        ui_allegrex_function *f = add_at_end(&uisec->functions);
        f->instructions = uisec->instruction_data->instructions.data;
        f->vaddr = sec->vaddr;

        u64 current_instruction_count = 0;

        bool new_func = false;

        while (jmp_i < array_size(jumps) && jumps->data[jmp_i].address < f->vaddr)
            ++jmp_i;

        for (u64 idx = 0; idx < uisec->instruction_data->instructions.size; ++idx)
        {
            instruction *instr = uisec->instruction_data->instructions.data + idx;           

            new_func = (jmp_i < array_size(jumps)) && (jumps->data[jmp_i].address <= instr->address);

            if (new_func)
            {
                f->instruction_count = current_instruction_count;

                if (f->instruction_count != 0)
                {
                    current_instruction_count = 0;
                    f = add_at_end(&uisec->functions);
                    f->instruction_count = 0;
                }

                f->instructions = instr;
                f->vaddr = instr->address;
                jmp_i++;
            }

            current_instruction_count += 1;
        }

        if (current_instruction_count == 0)
            uisec->functions.size--;
        else
            f->instruction_count = current_instruction_count;
    }

    for_array(usec, &ctx.sections)
        ::add_at_end(&ctx.section_search_results, usec);

    // offset formatting, we only want as many digits as necessary, not more
    if (ctx.sections.size > 0)
    {
        elf_section *last_section = (ctx.sections.data + (ctx.sections.size - 1))->section;
        u32 max_instruction_offset = last_section->content_offset + last_section->content_size;
        u32 pos_digits = hex_digits(max_instruction_offset);

        sprintf(ctx.file_offset_format, "%%0%ux", pos_digits);
    }

    // name padding
    u32 max_name_len = 0;

    for_hash_table(sym, &ctx.disasm.psp_module.symbols)
    {
        u64 tmp = string_length(sym->name);
        if (tmp > max_name_len)
            max_name_len = tmp;
    }

    for_hash_table(fimp, &ctx.disasm.psp_module.imports)
    {
        u64 tmp = string_length(fimp->function->name);
        if (tmp > max_name_len)
            max_name_len = tmp;
    }

    if (max_name_len > 256)
        max_name_len = 256;

    sprintf(ctx.address_name_format, "%%-%us", max_name_len);
}

void load_psp_elf(const char *path)
{
    free(&ctx);
    init(&ctx);

    disassemble_psp_elf(path, &ctx.disasm);

    prepare_disasm_ui_data();
}

void load_fonts(mg::window *window)
{
    ImGuiIO *io = &ImGui::GetIO();
    float scale = mg::get_window_scaling(window);
    int font_size = 20 * scale;

    ImFontConfig font_conf = ImFontConfig();
    font_conf.FontDataOwnedByAtlas = false;

    ui_font = io->Fonts->AddFontFromMemoryTTF(roboto_regular, size_roboto_regular, font_size, &font_conf);
    mono_font = io->Fonts->AddFontFromMemoryTTF(iosevka_fixed_ss02_regular, size_iosevka_fixed_ss02_regular, font_size, &font_conf);
    mono_bold_font = io->Fonts->AddFontFromMemoryTTF(iosevka_fixed_ss02_bold, size_iosevka_fixed_ss02_bold, font_size, &font_conf);
    mono_italic_font = io->Fonts->AddFontFromMemoryTTF(iosevka_fixed_ss02_italic, size_iosevka_fixed_ss02_italic, font_size, &font_conf);
    ui::upload_fonts(window);
}

int main(int argc, const char *argv[])
{
    init(&ctx.disasm);

    mg::window window;
    // TODO: remember window size
    mg::create_window(&window, allegrexplorer_NAME, 1600, 900);

    load_fonts(&window);

    ImGuiIO *io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (argc > 1)
        load_psp_elf(argv[1]);

    mg::event_loop(&window, ::update);

    mg::destroy_window(&window);

    free(&ctx.disasm);

    return 0;
}
