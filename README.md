# allegrexplorer

Work-in-progress disassembly tool for PSP (E)BOOT.BIN files.

![preview](https://github.com/user-attachments/assets/064068ff-e502-404c-9fe0-35e8dd13a629)

## Features

- Working
  - Disassembling of PSP (E)BOOT.BIN files, both encrypted and decrypted
  - Display of PSP module information
  - Elf section listing with function overview
  - Disassembly display
  - Shortcuts to jump to specific addresses (by entering an address, by clicking on a jump target, ...)
  - Exporting of disassembly (for more disassembly options, use [psp-elfdump](https://github.com/DaemonTsun/liballegrex/tree/master/psp-elfdump))
  - Dumping decrypted PSP Elf files
- Planned (in no particular order)
  - Symbol map with search feature
  - Syntax highlighting for arguments, names, addresses, ...
  - Control flow tree / overview
  - Annotation of addresses (for adding names to unnamed symbols, will be useful for decompilation)

## Building

```sh
$ mkdir bin
$ cd bin
$ cmake ..
$ make
```

## Usage

`$ ./allegrexplorer path-to-eboot.bin`

