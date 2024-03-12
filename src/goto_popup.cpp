
#include "shl/string.hpp"
#include "shl/parse.hpp"
#include "shl/format.hpp"
#include "shl/murmur_hash.hpp"

#include "allegrexplorer_context.hpp"
#include "ui.hpp"
#include "goto_popup.hpp"

static bool _open_goto_popup = false;
static bool _first_opened = false;

const u32 _max_user_input = 128;
static char _user_input[_max_user_input];

struct goto_search_result
{
    u32 address;
    const_string name;
};

static array<goto_search_result> _search_results{};

bool _goto_popup_submit(const_string input)
{
    u32 out;

    if (parse_integer(input, &out))
    {
        ui_set_jump_target_address(out);
        return true;
    }
    else
    {
        ::clear(&_search_results);

        // TODO: use levenshtein distance or something instead
        for_hash_table(addr, sym, &ctx.disasm.psp_module.symbols)
        {
            const_string name = to_const_string(sym->name);

            if (::begins_with(name, input))
                ::add_at_end(&_search_results, goto_search_result{*addr, name});
        }

        ::sort(_search_results.data, _search_results.size,
               (compare_function_p<goto_search_result>)[](auto a, auto b){ return compare_ascending(a->address, b->address); });
    }

    return false;
}

void open_goto_popup()
{
    _open_goto_popup = true;
}

void close_goto_popup()
{
    _open_goto_popup = false;
    _first_opened = false;
    ImGui::CloseCurrentPopup();
}

void goto_popup()
{
#define POPUP_GOTO "Goto address or symbol##POPUP_GOTO"

    if (_open_goto_popup)
    {
        _open_goto_popup = false;
        _first_opened = true;
        ImGui::OpenPopup(POPUP_GOTO);
    }

    if (_first_opened)
        ImGui::SetNextWindowSize({500, 200});

    if (ImGui::BeginPopupModal(POPUP_GOTO))
    {
        ImGui::Text("Enter address or case-sensitive symbol name to look for:");

        if (_first_opened)
        {
            ImGui::SetKeyboardFocusHere();
            _first_opened = false;
        }

        if (ImGui::InputText("Target", _user_input, _max_user_input, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (_goto_popup_submit(to_const_string(_user_input)))
                close_goto_popup();
        }

        const_string fmt{};

        for_array(i, result, &_search_results)
        {
            fmt = tformat("%08x - %s", result->address, result->name.c_str);

            ImGui::PushID(__LINE_HASH__ + i);
            
            if (ImGui::Button(fmt.c_str))
            {
                ui_set_jump_target_address(result->address);
                close_goto_popup();
            }

            ImGui::PopID();
        }

        if (ImGui::Button("Go"))
            if (_goto_popup_submit(to_const_string(_user_input)))
                close_goto_popup();

        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
            close_goto_popup();

        ImGui::EndPopup();
    }
}
