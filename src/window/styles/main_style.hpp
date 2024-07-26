
#pragma once

#include "imgui.h"

static void set_style(ImGuiStyle *dst = nullptr)
{
    ImGuiStyle* style = dst ? dst : &ImGui::GetStyle();

    style->WindowPadding = ImVec2(8.00f, 8.00f);
    style->FramePadding = ImVec2(12.00f, 6.00f);
    style->ItemSpacing = ImVec2(12.00f, 6.00f);
    style->ItemInnerSpacing = ImVec2(12.00f, 4.00f);
    style->TouchExtraPadding = ImVec2(0.00f, 0.00f);
    style->IndentSpacing = 22.00f;
    style->ScrollbarSize = 17.00f;
    style->GrabMinSize = 14.00f;
    style->WindowBorderSize = 2.00f;
    style->ChildBorderSize = 1.00f;
    style->PopupBorderSize = 1.00f;
    style->FrameBorderSize = 1.00f;
    style->TabBorderSize = 0.00f;
    style->TabBarBorderSize = 2.00f;
    style->WindowRounding = 0.00f;
    style->ChildRounding = 0.00f;
    style->FrameRounding = 4.00f;
    style->PopupRounding = 0.00f;
    style->ScrollbarRounding = 0.00f;
    style->GrabRounding = 3.00f;
    style->TabRounding = 4.00f;
    style->CellPadding = ImVec2(6.00f, 10.00f);
    style->TableAngledHeadersAngle = 0.61f;
    style->WindowTitleAlign = ImVec2(0.00f, 0.50f);
    style->WindowMenuButtonPosition = 0;
    style->ColorButtonPosition = 1;
    style->ButtonTextAlign = ImVec2(0.50f, 0.50f);
    style->SelectableTextAlign = ImVec2(0.00f, 0.00f);
    style->SeparatorTextBorderSize = 3.00f;
    style->SeparatorTextAlign = ImVec2(0.00f, 0.50f);
    style->SeparatorTextPadding = ImVec2(24.00f, 8.00f);
    style->LogSliderDeadzone = 4.00f;
    style->DockingSeparatorSize = 3.00f;
    style->DisplaySafeAreaPadding = ImVec2(3.00f, 3.00f);
}