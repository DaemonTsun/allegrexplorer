
#pragma once

#include "allegrex/disassemble.hpp"

#include "ui.hpp"

struct GLFWwindow;

struct allegrexplorer_context
{
    psp_disassembly disasm;

    GLFWwindow *window;
    allegrexplorer_ui ui;

    bool show_debug_info;
#ifndef NDEBUG
    struct _debug
    {
    } debug;
#endif
};

void init(allegrexplorer_context *ctx);
void free(allegrexplorer_context *ctx);

// the global context
extern allegrexplorer_context actx;

// gets the name of the address from the context. stored in static storage,
// may be overwritten, so store the name elsewhere if you need it later.
const char *address_name(u32 vaddr);
// same thing as address_name, but gives unnamed functions and branches labels too
const char *address_label(u32 addr);
