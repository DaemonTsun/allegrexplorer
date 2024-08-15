
#include "shl/format.hpp"
#include "allegrexplorer_context.hpp"

allegrexplorer_context actx;

void init(allegrexplorer_context *ctx)
{
    init(&ctx->disasm);
    init(&ctx->ui);

#ifndef NDEBUG
    ctx->show_debug_info = true;
#else
    ctx->show_debug_info = false;
#endif
}

void free(allegrexplorer_context *ctx)
{
    free(&ctx->ui);
    free(&ctx->disasm);
}

const char *address_name(u32 addr)
{
    // symbols
    elf_symbol *sym = ::search(&actx.disasm.psp_module.symbols, &addr);

    if (sym != nullptr)
        return sym->name;

    // imports
    function_import *fimp = ::search(&actx.disasm.psp_module.imports, &addr);

    if (fimp != nullptr)
        return fimp->function->name;

    return "";
}

static int _compare_only_address_ascending_p(const u32 *addr, const jump_destination *r)
{
    return compare_ascending(*addr, r->address);
}

const char *address_label(u32 addr)
{
    const char *aname = address_name(addr);

    if (aname != nullptr && aname[0] != '\0')
        return aname;

    constexpr const compare_function_p<u32, jump_destination> comp = _compare_only_address_ascending_p;

    binary_search_result res = nearest_index_of(&actx.disasm.all_jumps, addr, comp);

    if (res.last_comparison != 0)
        return "";

    jump_destination *dest = actx.disasm.all_jumps.data + res.index;

    const_string ret{};

    if (dest->type == jump_type::Jump)
        ret = tformat("func_%08x", addr);
    else
        ret = tformat(".L%08x", addr);

    return ret.c_str;
}

const char *address_label(jump_destination jmp)
{
    const char *aname = address_name(jmp.address);

    if (aname != nullptr && aname[0] != '\0')
        return aname;

    const_string ret{};

    if (jmp.type == jump_type::Jump)
        ret = tformat("func_%08x", jmp.address);
    else
        ret = tformat(".L%08x", jmp.address);

    return ret.c_str;
}

s64 instruction_index_by_vaddr(u32 vaddr)
{
    instruction *instrs = actx.disasm.all_instructions.data;
    s64 instr_count = actx.disasm.all_instructions.size;

    compare_function_p<u32, instruction> compare_instruction_vaddr =
        [](const u32 *l, const instruction *r)
        {
            return compare_ascending(*l, r->address);
        };

    auto res = binary_search(instrs, instr_count, &vaddr, compare_instruction_vaddr);

    if (res.last_comparison == 0)
        return res.index;
    else
        return -1;
}
