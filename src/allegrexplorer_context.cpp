
#include "allegrexplorer_context.hpp"

allegrexplorer_context ctx;

void init(allegrexplorer_context *ctx)
{
    init(&ctx->disasm);
    init(&ctx->ui);
}

void free(allegrexplorer_context *ctx)
{
    init(&ctx->ui);
    free(&ctx->disasm);
}

const char *address_name(u32 addr)
{
    // symbols
    elf_symbol *sym = ::search(&ctx.disasm.psp_module.symbols, &addr);

    if (sym != nullptr)
        return sym->name;

    // imports
    function_import *fimp = ::search(&ctx.disasm.psp_module.imports, &addr);

    if (fimp != nullptr)
        return fimp->function->name;

    /*
    // exports (unoptimized, but probably not too common)
    for_array(mod, conf->exported_modules)
    {
        for_array(func, &mod->functions)
            if (func->address == addr)
                return func->function->name;
    }
    */

    return "";
}
