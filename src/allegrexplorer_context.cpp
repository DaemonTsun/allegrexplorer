
#include "allegrexplorer_context.hpp"

void init(allegrexplorer_context *ctx)
{
    init(&ctx->disasm);
    init(&ctx->sections);
    init(&ctx->section_search_results);
}

void free(allegrexplorer_context *ctx)
{
    free(&ctx->section_search_results);
    free<true>(&ctx->sections);
    free(&ctx->disasm);
}
