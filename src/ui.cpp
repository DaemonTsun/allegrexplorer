
#include "imgui.h"
#include "ui.hpp"

void init(ui_elf_section *sec)
{
    init(&sec->header);
    init(&sec->functions);
}

void free(ui_elf_section *sec)
{
    free(&sec->header);
    free(&sec->functions);
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
    float y_padding = ImGui::GetStyle().WindowPadding.y;
    float font_size = ImGui::GetFontSize();
    float item_spacing = ImGui::GetStyle().ItemSpacing.y;
    
    return _recompute_function_height(func, y_padding, font_size, item_spacing);
}

float _recompute_section_height(ui_elf_section *sec, float y_padding, float font_size, float item_spacing)
{
    float total_height = 0;

    // header line
    total_height += y_padding;
    total_height += font_size;
    total_height += item_spacing;

    for_array(func, &sec->functions)
        total_height += _recompute_function_height(func, y_padding, font_size, item_spacing);

    total_height += sec->functions.size * item_spacing;
    
    // bottom
    total_height += y_padding;

    sec->computed_height = total_height;

    return total_height;
}

float recompute_section_height(ui_elf_section *sec)
{
    float y_padding = ImGui::GetStyle().WindowPadding.y;
    float font_size = ImGui::GetFontSize();
    float item_spacing = ImGui::GetStyle().ItemSpacing.y;
    
    return _recompute_section_height(sec, y_padding, font_size, item_spacing);
}

float recompute_total_disassembly_height(array<ui_elf_section> *sections)
{
    float y_padding = ImGui::GetStyle().WindowPadding.y;
    float font_size = ImGui::GetFontSize();
    float item_spacing = ImGui::GetStyle().ItemSpacing.y;

    float total_height = 0;

    for_array(_uisec, sections)
        total_height += _recompute_section_height(_uisec, y_padding, font_size, item_spacing);

    if (sections->size > 0)
        total_height += (sections->size) * item_spacing;

    return total_height;
}

float get_total_disassembly_height(array<ui_elf_section> *sections)
{
    float item_spacing = ImGui::GetStyle().ItemSpacing.y;

    float total_height = 0;

    for_array(_uisec, sections)
        total_height += _uisec->computed_height;

    if (sections->size > 0)
        total_height += (sections->size) * item_spacing;

    return total_height;
}
