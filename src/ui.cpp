
#include "ui.hpp"

void init(ui_elf_section *sec)
{
    init(&sec->header);
    init(&sec->functions);
}

void free(ui_elf_section *sec)
{
    free(&sec->header);
    free(&sec->functions);
}

