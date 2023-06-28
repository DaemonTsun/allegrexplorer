
#pragma once

#include "allegrex/disassemble.hpp"
#include "shl/array.hpp"
#include "shl/string.hpp"

#define U32_FORMAT   "%08x"
#define VADDR_FORMAT "0x%08x"

#define DISASM_LINE_FORMAT "%08x %08x"
#define DISASM_MNEMONIC_FORMAT "%-10s"

struct ui_allegrex_function
{
    elf_section *section;
    u32 vaddr;
    u32 max_vaddr; // the last vaddr in this function
    instruction *instructions;
    u64 instruction_count;

    float computed_height;
};

struct ui_elf_section
{
    // vaddr - name
    string header;
    elf_section *section;

    instruction_parse_data* instruction_data;

    array<ui_allegrex_function> functions;

    float computed_height;
};

void init(ui_elf_section *sec);
void free(ui_elf_section *sec);

// call after setting up fonts / when DPI changes
float recompute_function_height(ui_allegrex_function *func);
float recompute_section_height(ui_elf_section *sec);
float recompute_total_disassembly_height(array<ui_elf_section> *sections);

float get_total_disassembly_height(array<ui_elf_section> *sections);
