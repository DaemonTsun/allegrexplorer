
#pragma once

// These here are settings that are automatically saved on exit
// and loaded on start.

struct allegrexplorer_settings
{
    struct _window
    {
        int width;
        int height;
        int x;
        int y;
        bool maximized;
    } window;

    struct _disassembly
    {
        bool show_instruction_elf_offset;
        bool show_instruction_vaddr;
        bool show_instruction_opcode;
    } disassembly;
};

void settings_init();

allegrexplorer_settings *settings_get();
