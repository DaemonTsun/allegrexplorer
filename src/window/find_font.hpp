
#pragma once

// find_font.hpp

extern "C"
{
struct ff_cache;

// See shl/allocator.hpp for how an allocator looks like.
// If allocator is nullptr, uses a default allocator.
ff_cache *ff_load_font_cache(void *allocator = nullptr);
void      ff_unload_font_cache(ff_cache *cache);

// Tries to find a font by name and style, and returns the path to the font file.
// font_name and style_name must be _exact_.
// style_name may be nullptr or an empty string for the default style.
const char *ff_find_font_path(ff_cache *cache, const char *font_name, const char *style_name);

// Much slower, but easier to use.
// Names are checked using "begins_with", i.e. only start must match.
// NOTE: if there's multiple matches, it is not guaranteed which font path is
// returned. Use ff_find_font_path instead if you need an exact match.
const char *ff_find_font_path_vague(ff_cache *cache, const char *font_name, const char *style_name);


// The following functions try to find a font path by the provided font names
// and styles and return the path to the first one found in the provided order, or
// nullptr if no font was found.
// font_names_and_styles must point to pairs of strings, in the following form:
// name, style, name, style, name, style, ...
//
// count is the total number of strings, not pairs.
// If found_index is not nullptr, it's set to the index within font_names_and_styles of
// the found font.

// names and styles must be exact
const char *ff_find_first_font_path(ff_cache *cache, const char **font_names_and_styles, int count, int *found_index);

// names don't have to be exact here
const char *ff_find_first_font_path_vague(ff_cache *cache, const char **font_names_and_styles, int count, int *found_index);
}
