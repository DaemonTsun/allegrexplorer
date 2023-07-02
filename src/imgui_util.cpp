#include "imgui_internal.h"

#include "shl/array.hpp"

#include "imgui_util.hpp"

struct group_stack_data
{
    ImColor color;
    ImVec2 padding;
};

struct group_stack_render_data
{
    ImColor color;
    ImVec2 padding;
    ImVec2 start;
    ImVec2 end;
    u64 stack_depth;
};

static array<group_stack_data> group_stack{};
static array<group_stack_render_data> group_render_stack{};
static u64 render_stack_depth = 0;

struct XBounds
{
    float start;
    float end;
};

static XBounds calculateSectionBoundsX(float padding_left, float padding_right) {
    auto *window = ImGui::GetCurrentWindow();
    float windowStart = ImGui::GetWindowPos().x;

    return {windowStart + window->WindowPadding.x + padding_left,
            windowStart + ImGui::GetWindowWidth() - window->WindowPadding.x -
            padding_right};
}

void ui::begin_group(ImColor col)
{
    if (group_stack.size == 0)
        ImGui::GetWindowDrawList()->ChannelsSplit(2);

    ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);

    ImVec2 padding = ImGui::GetStyle().WindowPadding;

    ::add_at_end(&group_stack, group_stack_data{col, padding});

    auto *window = ImGui::GetCurrentWindow();
    auto boundsX = calculateSectionBoundsX(padding.x, padding.x);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding.y);
    ImGui::BeginGroup();

    if (padding.x > 0)
        ImGui::Indent(padding.x);

    ImGui::PushClipRect(ImVec2(boundsX.start, window->ClipRect.Min.y),
                        ImVec2(boundsX.end, window->ClipRect.Max.y), false);

    render_stack_depth++;
}

void ui::end_group()
{
    assert(group_stack.size > 0);

    group_stack_data stack_data = group_stack[group_stack.size - 1];
    ::remove_from_end(&group_stack);

    ImVec2 padding = stack_data.padding;

    ImGui::PopClipRect();

    if (padding.x > 0)
        ImGui::Unindent(padding.x);

    ImGui::EndGroup();

    render_stack_depth--;

    auto boundsX = calculateSectionBoundsX(0.0f, 0.0f);
    auto rect_min = ImGui::GetItemRectMin();
    auto rect_max = ImGui::GetItemRectMax();

    auto panel_min = ImVec2(rect_min.x, rect_min.y - padding.y);
    auto panel_max = ImVec2(rect_max.x + padding.x, rect_max.y + padding.y);

    ::add_at_end(&group_render_stack,
                 group_stack_render_data{stack_data.color, padding, panel_min, panel_max, render_stack_depth});

    if (group_stack.size == 0)
    {
        for (s64 i = group_render_stack.size - 1; i >= 0; i--)
        {
            group_stack_render_data *render = group_render_stack.data + i;
            auto end = render->end;
            ImGui::GetWindowDrawList()->ChannelsSetCurrent(0);
            ImGui::GetWindowDrawList()->AddRectFilled(
                    render->start, end,
                    render->color,
                    0.0f);
        }

        ImGui::GetWindowDrawList()->ChannelsMerge();

        ::clear(&group_render_stack);
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding.y);

    // ImGui::Spacing();
}
