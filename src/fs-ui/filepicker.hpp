
#pragma once

#include "shl/string.hpp"

enum FsUi_FilepickerFlags_
{
    FsUi_FilepickerFlags_NoDirectories      = 1 << 0, // must not be used together with the next option
    FsUi_FilepickerFlags_NoFiles            = 1 << 1, // use if only directories should be selectable, also implies selection must exist
    FsUi_FilepickerFlags_SelectionMustExist = 1 << 2  // may only select existing files / directories
};

typedef int FsUi_FilepickerFlags;

namespace FsUi
{
// Filter docs:
// https://learn.microsoft.com/en-us/dotnet/api/microsoft.win32.filedialog.filter?view=windowsdesktop-8.0
// 
// Example:
// "Any files|*.*|Office files|*.doc;*.docx;*.xlsx|Specific file|EBOOT.BIN"
#define FsUi_DefaultDialogFilter "Any files|*.*"

void Init();
void Exit();

bool Filepicker(const char *label, char *buf, size_t buf_size, const char *filter = FsUi_DefaultDialogFilter, FsUi_FilepickerFlags flags = 0);

bool FileDialog(const char *label, char *out_filebuf, size_t filebuf_size, const char *filter = FsUi_DefaultDialogFilter, FsUi_FilepickerFlags flags = 0);
}

