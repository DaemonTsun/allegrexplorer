
#include "shl/format.hpp"
#include "shl/string.hpp"

#include "mg/mg.hpp"
#include "allegrex/disassemble.hpp"

#include "allegrexplorer_info.hpp"
#include "imgui_util.hpp"

#define U32_FORMAT   "%08x"
#define VADDR_FORMAT "0x%08x"

struct ui_elf_section
{
    // vaddr - name
    string header;
    elf_section *section;

    // TODO: array of functions, sorted by vaddr
};

void free(ui_elf_section *sec)
{
    free(&sec->header);
}

struct allegrexplorer_context
{
    psp_disassembly disasm;

    array<ui_elf_section> sections;
    array<ui_elf_section*> section_search_results;
};

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

static allegrexplorer_context ctx;

void imgui_menu_bar(mg::window *window)
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
            {
                // TODO: implement this...
                // is there really no default file picker in imgui?
                // All the other ones I found were pretty bad...
            }

            if (ImGui::MenuItem("Close", "Ctrl+W"))
                mg::close_window(window);

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void imgui_side_panel(mg::window *window, ImGuiID dockspace_id)
{
    elf_psp_module *mod = &ctx.disasm.psp_module;
    prx_sce_module_info *mod_info = &mod->module_info;

    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Module Info");

    ImGui::InputText("Module Name", mod_info->name, PRX_MODULE_NAME_LEN, ImGuiInputTextFlags_ReadOnly);

    int attr = mod_info->attribute;
    ImGui::InputInt("Attributes", &attr, 0, 0, ImGuiInputTextFlags_ReadOnly
                                             | ImGuiInputTextFlags_CharsHexadecimal);


    u32 v1 = mod_info->version[0];
    ImGui::InputScalar("Version 1", ImGuiDataType_U32, &v1, nullptr, nullptr, U32_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);

    u32 v2 = mod_info->version[1];
    ImGui::InputScalar("Version 2", ImGuiDataType_U32, &v2, nullptr, nullptr, U32_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    
    ImGui::InputScalar("gp", ImGuiDataType_U32, &mod_info->gp, nullptr, nullptr, VADDR_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    
    ImGui::InputScalar("Export start", ImGuiDataType_U32, &mod_info->export_offset_start,
                       nullptr, nullptr, VADDR_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    ImGui::InputScalar("Export end", ImGuiDataType_U32, &mod_info->export_offset_end,
                       nullptr, nullptr, VADDR_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    
    ImGui::InputScalar("Import start", ImGuiDataType_U32, &mod_info->import_offset_start,
                       nullptr, nullptr, VADDR_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);
    ImGui::InputScalar("Import end", ImGuiDataType_U32, &mod_info->import_offset_end,
                       nullptr, nullptr, VADDR_FORMAT,
                       ImGuiInputTextFlags_ReadOnly);

    ImGui::End();
}

void main_panel(mg::window *window, ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Disassembly");

    auto wpadding = ImGui::GetStyle().WindowPadding;
    ui::padding sec_padding;
    sec_padding.left = 0;
    sec_padding.right = wpadding.x;
    sec_padding.top = wpadding.y;
    sec_padding.bottom = wpadding.y;

    ImColor sec_color{0xff, 0xcc, 0x22, 0xff};
    ImColor func_color{0xff, 0x33, 0x11, 0xff};

    ImGui::PushStyleColor(ImGuiCol_Text, {0, 0, 0, 0xff});

    for_array(sec, &ctx.disasm.psp_module.sections)
    {
        ui::begin_group(sec_color, sec_padding);

        ImGui::Text(sec->name);

        ui::begin_group(func_color, sec_padding);

        ui::end_group();
        ui::end_group();
    }

    ImGui::PopStyleColor();

    ImGui::End();
}

void sections_panel(mg::window *window, ImGuiID dockspace_id)
{
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin("Sections");

    const u32 max_sec_name_size = 256;
    static char search_buf[max_sec_name_size] = {0};

    if (ImGui::InputText("Search", search_buf, max_sec_name_size))
    {
        clear(&ctx.section_search_results);

        if (!is_blank(search_buf))
        {
            for_array(usec, &ctx.sections)
                if (index_of(usec->header, search_buf) >= 0)
                    add_at_end(&ctx.section_search_results, usec);
        }
        else
        {
            for_array(usec, &ctx.sections)
                add_at_end(&ctx.section_search_results, usec);
        }
    }

    for_array(usec_, &ctx.section_search_results)
    {
        ui_elf_section *usec = *usec_;

        if (ImGui::TreeNode((const char*)usec->header))
        {
            elf_section *sec = usec->section;

            ImGui::InputText("Name", (char*)sec->name, max_sec_name_size, ImGuiInputTextFlags_ReadOnly);

            ImGui::InputScalar("Vaddr", ImGuiDataType_U32, &sec->vaddr,
                               nullptr, nullptr, VADDR_FORMAT,
                               ImGuiInputTextFlags_ReadOnly);

            ImGui::InputScalar("Elf offset", ImGuiDataType_U32, &sec->content_offset,
                               nullptr, nullptr, VADDR_FORMAT,
                               ImGuiInputTextFlags_ReadOnly);

            ImGui::InputScalar("Size", ImGuiDataType_U64, &sec->content_size,
                               nullptr, nullptr, U32_FORMAT,
                               ImGuiInputTextFlags_ReadOnly);
            
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void update(mg::window *window, double dt)
{
    ui::new_frame(window);

    int windowflags = ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoDecoration
                    | ImGuiWindowFlags_MenuBar
                    | ImGuiWindowFlags_NoBackground;

    ui::set_next_window_full_size();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(allegrexplorer_NAME, nullptr, windowflags);
    ImGui::PopStyleVar();

    imgui_menu_bar(window);

    ImGuiID dockspace_id = ImGui::GetID("main_dock");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    imgui_side_panel(window, dockspace_id);
    main_panel(window, dockspace_id);
    sections_panel(window, dockspace_id);

    ImGui::End();

    ui::end_frame();
}

void prepare_disasm_ui_data()
{
    for_array(sec, &ctx.disasm.psp_module.sections)
    {
        ui_elf_section *uisec = ::add_at_end(&ctx.sections);

        uisec->header = copy_string(tformat(VADDR_FORMAT " %s", sec->vaddr, sec->name));
        uisec->section = sec;

    }

    for_array(usec, &ctx.sections)
        ::add_at_end(&ctx.section_search_results, usec);
}

void load_psp_elf(const char *path)
{
    free(&ctx);
    init(&ctx);

    disassemble_psp_elf(path, &ctx.disasm);

    prepare_disasm_ui_data();
}

int main(int argc, const char *argv[])
{
    init(&ctx.disasm);

    mg::window window;
    mg::create_window(&window, allegrexplorer_NAME, 1024, 786);

    ImGuiIO *io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (argc > 1)
        load_psp_elf(argv[1]);

    mg::event_loop(&window, ::update);

    mg::destroy_window(&window);

    free(&ctx.disasm);

    return 0;
}
