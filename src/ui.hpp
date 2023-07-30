
#pragma once

#include "imgui.h"

#include "allegrex/disassemble.hpp"
#include "shl/array.hpp"
#include "shl/string.hpp"

#define U32_FORMAT   "%08x"
#define VADDR_FORMAT "0x%08x"

#define DISASM_LINE_FORMAT "%08x %08x"
#define DISASM_MNEMONIC_FORMAT "%-10s"

const float ui_style_disassembly_y_padding = 2;
const float ui_style_disassembly_item_spacing = 6;

struct ui_allegrex_function
{
    elf_section *section;
    u32 vaddr;
    u32 max_vaddr; // vaddr + (instruction_count - 1) * sizeof(u32)
    instruction *instructions;
    u64 instruction_count;

    float computed_height;
    float computed_y_offset;
};

struct ui_elf_section
{
    // vaddr - name
    string header;
    elf_section *section;

    instruction_parse_data* instruction_data;

    u32 max_vaddr; // section->vaddr + (instruction_data.size - 1) * sizeof(u32)

    array<ui_allegrex_function> functions;

    float computed_height;
    float computed_y_offset;
};

void init(ui_elf_section *sec);
void free(ui_elf_section *sec);

struct ui_context
{
    array<ui_elf_section> sections;
    array<ui_elf_section*> section_search_results;
    float computed_height;

    struct _fonts
    {
        ImFont *ui;
        ImFont *mono;
        ImFont *mono_bold;
        ImFont *mono_italic;
    } fonts;

    u32 jump_address;
    float jump_y_offset;
    bool do_jump;
};

void init(ui_context *ctx);
void free(ui_context *ctx);

// call after setting up fonts / when DPI changes
float recompute_function_height(ui_allegrex_function *func);
float recompute_section_height(ui_elf_section *sec);
float recompute_total_disassembly_height(ui_context *ctx);

float get_y_offset_of_address(u32 vaddr);
ui_elf_section *get_section_containing_vaddr(u32 vaddr);
ui_allegrex_function *get_function_containing_vaddr(u32 vaddr);

void ui_instruction_name_text(const instruction *inst);
void ui_instruction_arguments(instruction *inst);
void ui_set_jump_target_address(u32 address);
void ui_do_jump_to_target_address();

void ui_address_button(u32 target_address, const char *fmt, ...);
