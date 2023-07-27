
#pragma once

#include "shl/endian.hpp"

#define COL(x) (force_big_endian(x))

namespace colors
{
extern ImColor section_bg_color;
extern ImColor section_text_color;
extern ImColor section_tooltip_color;
extern ImColor function_bg_color;
extern ImColor function_highlighted_bg_color;
}
