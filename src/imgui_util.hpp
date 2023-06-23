
#pragma once

#include "shl/endian.hpp"
#include "imgui.h"

namespace ui
{
struct padding
{
    float left;
    float right;
    float top;
    float bottom;
};

void begin_group(ImColor col = 0, ui::padding padding = {-1, -1, -1, -1});
void end_group();
}

#define COL(x) (force_big_endian(x))
