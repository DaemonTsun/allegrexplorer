
// header for imgui utility functions

#pragma once

#include "shl/endian.hpp"
#include "imgui.h"

namespace ui
{
void begin_group(ImColor col = 0);
void end_group();
}

#define COL(x) (force_big_endian(x))
