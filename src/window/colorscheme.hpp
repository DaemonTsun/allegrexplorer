
#pragma once

struct ImGuiStyle;
typedef void (*colorscheme_apply_function)(ImGuiStyle *dst);

struct colorscheme
{
    const char *name;
    colorscheme_apply_function apply;
};

void colorscheme_init();
void colorscheme_free();
void colorscheme_get_all(const colorscheme **out, int *out_count);

const colorscheme *colorscheme_get_current();
void colorscheme_set(const colorscheme *scheme);

// sets to a colorscheme if none is set
void colorscheme_set_default();
