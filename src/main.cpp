
#include "mg/mg.hpp"
#include "allegrex/disassemble.hpp"

#include "allegrexplorer_info.hpp"

void update(mg::window *window, double dt)
{
    ui::new_frame(window);
    static bool active = true;

    int windowflags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar;

#ifdef IMGUI_HAS_VIEWPORT
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
    ImGui::SetNextWindowViewport(viewport->ID);
#else 
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
#endif

    ImGui::Begin("My First Tool", nullptr, windowflags);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open...", "Ctrl+O"))
            {
                
            }

            if (ImGui::MenuItem("Close", "Ctrl+W"))  { active = false; }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End();

    ui::end_frame();
}

int main(int argc, const char *argv[])
{
    mg::window window;
    mg::create_window(&window, "a", 640, 480);

    mg::event_loop(&window, ::update);

    mg::destroy_window(&window);

    return 0;
}
