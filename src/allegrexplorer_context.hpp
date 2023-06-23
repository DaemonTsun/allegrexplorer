
#pragma once

#include "ui.hpp"

struct allegrexplorer_context
{
    psp_disassembly disasm;

    array<ui_elf_section> sections;
    array<ui_elf_section*> section_search_results;

    char file_offset_format[32];
    char address_name_format[32];
};

void init(allegrexplorer_context *ctx);
void free(allegrexplorer_context *ctx);
