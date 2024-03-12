
#include <assert.h>
#include "backends/imgui_impl_vulkan.h"

#include "fonts/fonts.hpp"

#include "shl/format.hpp"
#include "shl/string.hpp"
#include "shl/array.hpp"
#include "shl/murmur_hash.hpp"

#include "allegrex/disassemble.hpp"

#include "allegrexplorer_info.hpp"
#include "allegrexplorer_context.hpp"
#include "window_imgui_util.hpp"
#include "imgui_util.hpp"
#include "colors.hpp"
#include "goto_popup.hpp"
#include "ui.hpp"

#ifndef NDEBUG
bool show_debug_info = true;
#else
bool show_debug_info = false;
#endif

void imgui_menu_bar()
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
                close_window(ctx.window);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Toggle debug info", nullptr, &show_debug_info);

            if (ImGui::MenuItem("Goto Address / Symbol", "Ctrl+G"))
                open_goto_popup();

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void imgui_side_panel(ImGuiID dockspace_id)
{
    elf_psp_module *mod = &ctx.disasm.psp_module;
    prx_sce_module_info *mod_info = &mod->module_info;

    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Module Info");

    ImGui::PushFont(ctx.ui.fonts.mono);
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

void main_panel(ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Disassembly");

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {ImGui::GetStyle().WindowPadding.x, ui_style_disassembly_y_padding});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ImGui::GetStyle().ItemSpacing.x, ui_style_disassembly_item_spacing});
    auto wsize = ImGui::GetWindowSize();
    auto wpadding = ImGui::GetStyle().WindowPadding;

    float y_padding = wpadding.y;

    float font_size = ImGui::GetFontSize();
    float item_spacing = ImGui::GetStyle().ItemSpacing.y;

    if (ctx.ui.computed_height < 0)
        recompute_total_disassembly_height(&ctx.ui);

    float total_height = ctx.ui.computed_height;
    ui_allegrex_function *highlighted_function = ctx.ui.jump.target_function;

    ui_do_jump_to_target_address();

    float view_min_y = ImGui::GetScrollY();
    float view_max_y = view_min_y + wsize.y;

    ctx.debug.view_min_y = view_min_y;
    ctx.debug.view_max_y = view_max_y;

    ImGui::BeginChild("content",
                      {wsize.x, total_height},
                      false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PushFont(ctx.ui.fonts.mono);
    ImGui::PushStyleColor(ImGuiCol_Text, (u32)colors::section_text_color);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, y_padding});

    float current_height = 0;

    for_array(_i, uisec, &ctx.ui.sections)
    {
        float sec_height = uisec->computed_height;

        if (current_height > view_max_y)
            break;

        if (current_height + sec_height < view_min_y)
        {
            ImGui::Dummy({0.f, sec_height});
            current_height += sec_height;
            current_height += item_spacing;
            continue;
        }

        float current_sec_height = current_height;

        current_sec_height += y_padding;
        current_sec_height += font_size;
        current_sec_height += item_spacing;

        current_height += sec_height;
        current_height += item_spacing;

        elf_section *sec = uisec->section;
        u32 pos = sec->content_offset;

        begin_group(colors::section_bg_color);

        ImGui::Text("Section %s", sec->name);

        for_array(_fi, func, &uisec->functions)
        {
            if (func->instruction_count == 0)
                continue;

            float func_height = func->computed_height;

            if (current_sec_height > view_max_y)
                break;

            if (current_sec_height + func_height < view_min_y)
            {
                ImGui::Dummy({0.f, func_height});
                current_sec_height += func_height;
                current_sec_height += item_spacing;
                continue;
            }

            current_sec_height += func_height;
            current_sec_height += item_spacing;

            auto func_col = colors::function_bg_color;

            if (func == highlighted_function)
                func_col = colors::function_highlighted_bg_color;

            begin_group(func_col);

            for (u64 i = 0; i < func->instruction_count; ++i)
            {
                instruction *inst = func->instructions + i;

                if (show_debug_info)
                {
                    ImGui::Text("%.0f", ImGui::GetCursorPosY());
                    ImGui::SameLine();
                }

                if (ctx.ui.jump.target_address == inst->address)
                    ImGui::PushFont(ctx.ui.fonts.mono_bold);

                ImGui::Text(ctx.address_name_format, address_label(inst->address));

                if (ctx.ui.jump.target_address == inst->address)
                    ImGui::PopFont();

                ImGui::SameLine();
                ImGui::Text(ctx.file_offset_format, pos);
                ImGui::SameLine();
                ImGui::Text(DISASM_LINE_FORMAT, inst->address, inst->opcode);
                ImGui::SameLine();
                ui_instruction_name_text(inst);
                ui_instruction_arguments(inst);

                pos += sizeof(u32);
            }

            end_group();
        }

        end_group();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    ImGui::End();
}

void sections_panel(ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Sections");

    const u32 max_sec_name_size = 256;
    static char search_buf[max_sec_name_size] = {0};

    if (ImGui::InputText("Search", search_buf, max_sec_name_size))
    {
        clear(&ctx.ui.section_search_results);

        if (!is_blank(search_buf))
        {
            for_array(usec, &ctx.ui.sections)
                if (index_of(usec->header, search_buf) >= 0)
                    add_at_end(&ctx.ui.section_search_results, usec);
        }
        else
        {
            for_array(usec, &ctx.ui.sections)
                add_at_end(&ctx.ui.section_search_results, usec);
        }
    }

    ImGui::PushFont(ctx.ui.fonts.mono);

    for_array(usec_, &ctx.ui.section_search_results)
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

void jump_history_panel(ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Jump history");

    ImGui::PushFont(ctx.ui.fonts.mono);
    ImGui::PushItemWidth(-1);

    if (ImGui::BeginListBox("##", ImVec2{-1, -1}))
    {
        for_array(_i, _, &ctx.ui.jump.history)
        {
            ImGui::PushID(__LINE_HASH__ + _i);
            auto *entry = ctx.ui.jump.history.data + ((ctx.ui.jump.history.size - 1) - _i);
            ui_address_button(entry->target_address, "%08x: %s", entry->target_address, (const char*)entry->function_name);
            ImGui::PopID();
        }

        ImGui::EndListBox();
    }

    ImGui::PopFont();

    ImGui::End();
}

void debug_info_panel(ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Debug Info");

    ImGui::Text("View range: %.0f - %.0f", ctx.debug.view_min_y, ctx.debug.view_max_y);
    ImGui::Text("Jump addr: %08x", ctx.ui.jump.target_address);
    ImGui::Text("Jump y: %.0f", ctx.ui.jump.y_offset);

    ImGui::End();
}

void show_popups(ImGuiID dockspace_id)
{
    goto_popup();
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

    imgui_menu_bar();

    ImGuiID dockspace_id = ImGui::GetID("main_dock");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    imgui_side_panel(dockspace_id);
    main_panel(dockspace_id);
    sections_panel(dockspace_id);
    jump_history_panel(dockspace_id);

    if (show_debug_info)
        debug_info_panel(dockspace_id);

    show_popups(dockspace_id);

    ImGui::End();

    ui_end_frame();
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

static void _prepare_disasm_ui_data()
{
    reserve(&ctx.ui.sections, ctx.disasm.psp_module.sections.size);

    jump_destinations *jumps = &ctx.disasm.jumps;
    u32 jmp_i = 0;

    for_array(i, sec, &ctx.disasm.psp_module.sections)
    {
        ui_elf_section *uisec = ::add_at_end(&ctx.ui.sections);
        init(uisec);

        uisec->header = copy_string(tformat(VADDR_FORMAT " %s", sec->vaddr, sec->name));
        uisec->section = sec;
        uisec->instruction_data = nullptr;
        
        uisec->instruction_data = ctx.disasm.instruction_datas.data + i;
        assert(uisec->instruction_data->section_index == i);
        uisec->max_vaddr = sec->vaddr;

        if (uisec->instruction_data->instructions.size > 0)
            uisec->max_vaddr += (uisec->instruction_data->instructions.size - 1) * sizeof(u32);

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
                // let's not split at branches
                if (jumps->data[jmp_i].type == jump_type::Jump)
                {
                    f->instruction_count = current_instruction_count;
                    f->max_vaddr = f->vaddr;

                    if (f->instruction_count != 0)
                    {
                        f->max_vaddr += (current_instruction_count - 1) * sizeof(u32);
                        current_instruction_count = 0;
                        f = add_at_end(&uisec->functions);
                        f->instruction_count = 0;
                    }

                    f->instructions = instr;
                    f->vaddr = instr->address;
                }

                jmp_i++;
            }

            current_instruction_count += 1;
        }

        if (current_instruction_count == 0)
            uisec->functions.size--;
        else
            f->instruction_count = current_instruction_count;
    }

    for_array(usec, &ctx.ui.sections)
        ::add_at_end(&ctx.ui.section_search_results, usec);

    // offset formatting, we only want as many digits as necessary, not more
    if (ctx.ui.sections.size > 0)
    {
        elf_section *last_section = (ctx.ui.sections.data + (ctx.ui.sections.size - 1))->section;
        u32 max_instruction_offset = last_section->content_offset + last_section->content_size;
        u32 pos_digits = hex_digits(max_instruction_offset);

        format(ctx.file_offset_format, 32, "\\%0%ux", pos_digits);
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

    format(ctx.address_name_format, 32, "\\%-%us", max_name_len);
}

static bool _load_psp_elf(const char *path, error *err)
{
    free(&ctx);
    init(&ctx);

    if (!disassemble_psp_elf(path, &ctx.disasm, err))
        return false;

    _prepare_disasm_ui_data();

    return true;
}

static void _load_fonts()
{
    ImGuiIO *io = &ImGui::GetIO();

    float scale = get_window_scaling(ctx.window);
    int font_size = 20 * scale;

    ImFontConfig font_conf = ImFontConfig();
    font_conf.FontDataOwnedByAtlas = false;

    ctx.ui.fonts.ui = io->Fonts->AddFontFromMemoryTTF(roboto_regular, size_roboto_regular, font_size, &font_conf);
    ctx.ui.fonts.mono = io->Fonts->AddFontFromMemoryTTF(iosevka_fixed_ss02_regular, size_iosevka_fixed_ss02_regular, font_size, &font_conf);
    ctx.ui.fonts.mono_bold = io->Fonts->AddFontFromMemoryTTF(iosevka_fixed_ss02_bold, size_iosevka_fixed_ss02_bold, font_size, &font_conf);
    ctx.ui.fonts.mono_italic = io->Fonts->AddFontFromMemoryTTF(iosevka_fixed_ss02_italic, size_iosevka_fixed_ss02_italic, font_size, &font_conf);
}

static void _key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    bool ctrl = (mods & 2) == 2;
    bool pressed = action == 1;

    if (pressed && ctrl && key == 'G')
    {
        open_goto_popup();
    }
    else if (pressed && ctrl && key == 'W')
        close_window(window);

    // TODO: forward & backward
}

int main(int argc, const char *argv[])
{
    error err{};

    init(&ctx.disasm);

    window_init();

    // TODO: remember window size
    ctx.window = create_window(allegrexplorer_NAME, 1600, 900);
    set_window_keyboard_callback(ctx.window, _key_callback);
    ui_init(ctx.window);

    _load_fonts();

    if (argc > 1)
        _load_psp_elf(argv[1], &err);

    window_event_loop(ctx.window, _update);

    ui_exit(ctx.window);
    destroy_window(ctx.window);
    window_exit();

    free(&ctx.disasm);

    return 0;
}
