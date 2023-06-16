
#include "mg/mg.hpp"
#include "allegrex/disassemble.hpp"

#include "allegrexplorer_info.hpp"

struct allegrexplorer_context
{
    psp_disassembly disasm;
};

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

    ImGui::End();

    ui::end_frame();
}

void load_psp_elf(const char *path)
{
    free(&ctx.disasm);
    init(&ctx.disasm);

    disassemble_psp_elf(path, &ctx.disasm);
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
