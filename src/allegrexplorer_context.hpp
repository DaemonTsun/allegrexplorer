
#pragma once

#include "allegrex/disassemble.hpp"
#include "ui.hpp"

struct allegrexplorer_context
{
    psp_disassembly disasm;

    ui_context ui;

    char file_offset_format[32];
    char address_name_format[32];
};

void init(allegrexplorer_context *ctx);
void free(allegrexplorer_context *ctx);

extern allegrexplorer_context ctx;

// gets the name of the address from the context
const char *address_name(u32 vaddr);
