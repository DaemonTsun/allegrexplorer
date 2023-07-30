
#include "shl/format.hpp"
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

int compare_only_address_ascending_p(const u32 *addr, const jump_destination *r)
{
    return compare_ascending(*addr, r->address);
}

const char *address_label(u32 addr)
{
    const char *aname = address_name(addr);

    if (aname != "")
        return aname;

    constexpr const compare_function_p<u32, jump_destination> comp = compare_only_address_ascending_p;

    binary_search_result res = nearest_index_of(&ctx.disasm.jumps, addr, comp);

    if (res.last_comparison != 0)
        return "";

    jump_destination *dest = at(&ctx.disasm.jumps, res.index);

    static char _buf[14];

    if (dest->type == jump_type::Jump)
        snprintf(_buf, 14, "func_%08x", addr);
    else
        snprintf(_buf, 14, ".L%08x", addr);

    return _buf;
}
