#include "imgui_internal.h"

#include "shl/array.hpp"

#include "imgui_util.hpp"

struct group_stack_data
{
    ImColor color;
    ui::padding padding;
};

struct group_stack_render_data
{
    ImColor color;
    ui::padding padding;
    ImVec2 start;
    ImVec2 end;
};

static array<group_stack_data> group_stack{};
static array<group_stack_render_data> group_render_stack{};

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

void ui::begin_group(ImColor col, ui::padding padding)
{
    if (group_stack.size == 0)
        ImGui::GetWindowDrawList()->ChannelsSplit(2);

    ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);

    auto wpadding = ImGui::GetStyle().WindowPadding;

    if (padding.left < 0)  padding.left  = wpadding.x;
    if (padding.right < 0) padding.right = wpadding.x;
    if (padding.top < 0)    padding.top    = wpadding.y;
    if (padding.bottom < 0) padding.bottom = wpadding.y;

    ::add_at_end(&group_stack, group_stack_data{col, padding});

    auto *window = ImGui::GetCurrentWindow();
    auto boundsX = calculateSectionBoundsX(padding.left, padding.right);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding.top);
    ImGui::BeginGroup();

    if (padding.left > 0)
        ImGui::Indent(padding.left);

    ImGui::PushClipRect(ImVec2(boundsX.start, window->ClipRect.Min.y),
                        ImVec2(boundsX.end, window->ClipRect.Max.y), false);
}

void ui::end_group()
{
    assert(group_stack.size > 0);

    group_stack_data stack_data = group_stack[group_stack.size - 1];
    ::remove_from_end(&group_stack);

    auto padding = stack_data.padding;

    ImGui::PopClipRect();

    if (padding.left > 0)
        ImGui::Unindent(padding.left);

    ImGui::EndGroup();

    auto boundsX = calculateSectionBoundsX(0.0f, 0.0f);
    auto rect_min = ImGui::GetItemRectMin();
    auto rect_max = ImGui::GetItemRectMax();

    auto panel_min = ImVec2(rect_min.x, rect_min.y - padding.top);
    auto panel_max = ImVec2(rect_max.x + padding.right, rect_max.y + padding.bottom);

    ::add_at_end(&group_render_stack,
                 group_stack_render_data{stack_data.color, padding, panel_min, panel_max});

    if (group_stack.size == 0)
    {
        for (s64 i = group_render_stack.size - 1; i >= 0; i--)
        {
            group_stack_render_data *render = group_render_stack.data + i;
            auto end = render->end;
            end.x += i * render->padding.right;
            ImGui::GetWindowDrawList()->ChannelsSetCurrent(0);
            ImGui::GetWindowDrawList()->AddRectFilled(
                    render->start, end,
                    render->color,
                    0.0f);
        }

        ImGui::GetWindowDrawList()->ChannelsMerge();

        ::clear(&group_render_stack);
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding.bottom);

    ImGui::Spacing();
}
