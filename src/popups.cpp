
#include "imgui.h"

#include "shl/memory.hpp"
#include "shl/string.hpp"

#include "allegrexplorer_context.hpp"
#include "popups.hpp"

struct _goto_search_result
{
    u32 address;
    const char *name;
};

struct _goto_data
{
    char search_text[256] = {};

    array<_goto_search_result> search_results;
};

static void init(_goto_data *data)
{
    fill_memory(data, 0);
}

static void free(_goto_data *data)
{
    free(&data->search_results);
    fill_memory(data, 0);
}

bool popup_goto(u32 *out_addr)
{
    static _goto_data *goto_data = nullptr;

    if (goto_data == nullptr)
    {
        goto_data = alloc<_goto_data>();
        init(goto_data);
        ImGui::SetKeyboardFocusHere();
    }

#define goto_cleanup(Data)\
    {\
        free(Data);\
        dealloc(Data);\
        Data = nullptr;\
    }

    bool go = ImGui::InputText("##search", goto_data->search_text, 255, ImGuiInputTextFlags_EnterReturnsTrue);

    if (ImGui::IsItemFocused() && ImGui::IsItemEdited())
    {
        clear(&goto_data->search_results);

        if (!string_is_blank(goto_data->search_text))
        {
            // TODO: more sources of symbols
            for_hash_table(addr, sym, &actx.disasm.psp_module.symbols)
            {
                if (string_begins_with(sym->name, goto_data->search_text))
                    ::add_at_end(&goto_data->search_results, _goto_search_result{.address = *addr, .name = sym->name});
            }
        }

        compare_function_p<_goto_search_result> compare_search_results =
            [](const _goto_search_result *l, const _goto_search_result *r)
            {
                return compare_ascending(l->address, r->address);
            };

        ::sort(goto_data->search_results.data, goto_data->search_results.size, compare_search_results);
    }

    ImGui::SameLine();

    go |= ImGui::Button("Go");

    if (go)
    {
        if (string_is_blank(goto_data->search_text))
            return false;

        const_string end = to_const_string(goto_data->search_text);
        u32 ul = string_to_u32(goto_data->search_text, &end, 16);

        if (ul != 0ul || end.c_str != (char*)goto_data->search_text)
        {
            // input is an address
            *out_addr = ul;

            goto_cleanup(goto_data);
            return true;
        }
        else
        {
            // TODO: other sources of names
            // input is text
            for_hash_table(addr, sym, &actx.disasm.psp_module.symbols)
            {
                if (string_begins_with(sym->name, goto_data->search_text))
                {
                    *out_addr = *addr;

                    goto_cleanup(goto_data);
                    return true;
                }
            }
        }
    }

    ImGui::PushFont(actx.ui.fonts.mono);

    _goto_search_result *clicked_result = nullptr;
    for_array(sr, &goto_data->search_results)
    {
        ImGui::Text("%08x", sr->address);

        ImGui::SameLine();

        if (ImGui::Button(sr->name))
            clicked_result = sr;
    }
    ImGui::PopFont();

    if (clicked_result != nullptr)
    {
        *out_addr = clicked_result->address;

        goto_cleanup(goto_data);
        return true;
    }


    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        *out_addr = max_value(u32);

        goto_cleanup(goto_data);
        return true;
    }

#undef goto_cleanup

    return false;
}
