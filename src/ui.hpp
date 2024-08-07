
#pragma once

#include "imgui.h"

#define U32_FORMAT   "%08x"
#define VADDR_FORMAT "0x%08x"

#define DISASM_LINE_FORMAT "%08x %08x"
#define DISASM_MNEMONIC_FORMAT "%-10s"

struct allegrexplorer_ui
{
    struct _fonts
    {
        ImFont *ui;
        ImFont *mono;
        ImFont *mono_bold;
        ImFont *mono_italic;
    } fonts;

    /*
    struct _jump
    {
        u32 target_address;
        ui_allegrex_function *target_function;
        float y_offset;
        bool do_jump;

        array<ui_jump_history_entry> history;
    } jump;
    */
};

void init(allegrexplorer_ui *ui);
void free(allegrexplorer_ui *ui);

void ui_load_fonts(allegrexplorer_ui *ui, float scale);

