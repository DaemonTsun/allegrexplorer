
#pragma once

// IDs for popups
#define POPUP_OPEN_ELF              "Open PSP ELF..."
#define POPUP_GOTO                  "Goto Address / Symbol"
#define POPUP_EXPORT_DECRYPTED_ELF  "Export decrypted ELF..."
#define POPUP_EXPORT_DISASSEMBLY    "Export disassembly..."
#define POPUP_ABOUT                 "About Allegrexplorer"

bool popup_goto(u32 *out_addr);
