
#pragma once

// Window showing the entire disassembly

void disassembly_window();

void disassembly_goto_address(u32 addr);

// vertical offset
float disassembly_instruction_index_to_offset(s64 index);
s64   disassembly_offset_to_instruction_index(float offset);
float disassembly_address_to_offset(u32 address);
u32   disassembly_offset_to_address(float offset);
