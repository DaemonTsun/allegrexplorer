
#pragma once

#include "allegrex/disassemble.hpp"
#include "ui.hpp"

struct GLFWwindow;

struct allegrexplorer_context
{
    psp_disassembly disasm;

    GLFWwindow *window;
    ui_context ui;

    char file_offset_format[32];
    char address_name_format[32];

#ifndef NDEBUG
    struct _debug
    {
        float view_min_y;
        float view_max_y;
    } debug;
#endif
};

void init(allegrexplorer_context *ctx);
void free(allegrexplorer_context *ctx);

// the global context
extern allegrexplorer_context ctx;

// gets the name of the address from the context. stored in static storage,
// may be overwritten, so store the name elsewhere if you need it later.
const char *address_name(u32 vaddr);
// same thing as address_name, but gives unnamed functions and branches labels too
const char *address_label(u32 addr);
