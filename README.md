# allegrexplorer

Work-in-progress disassembly tool for PSP (E)BOOT.BIN files.

![image](https://github.com/DaemonTsun/allegrexplorer/assets/96687758/cdba9f99-7731-45e2-9828-2efe1c3e27a4)

## Features

- Working
  - Disassembling of PSP (E)BOOT.BIN files, both encrypted and decrypted
  - Display of PSP module information
  - Elf section listing with search
  - Full display of the disassembly, grouped by sections and functions (jump targets)
- Planned (in no particular order)
  - Shortcuts to jump to specific addresses (by entering an address, by clicking on a jump target, ...)
  - Symbol map with search feature
  - Syntax highlighting for arguments, names, addresses, ...
  - Exporting of disassembly (can already be done with [psp-elfdump](https://github.com/DaemonTsun/liballegrex/tree/master/psp-elfdump))
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

