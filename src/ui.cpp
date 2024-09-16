
#include "shl/defer.hpp"

#include "window/find_font.hpp"
#include "window/find_font_fonts.hpp"
#include "ui.hpp"
#include "ui.hpp"

void init(allegrexplorer_ui *ctx)
{
}

void free(allegrexplorer_ui *ctx)
{
}

void ui_load_fonts(allegrexplorer_ui *ui, float scale)
{
    int font_size = (int)(20 * scale);

    ff_cache *fc = ff_load_font_cache();
    defer { ff_unload_font_cache(fc); };

    const char *font_names_monospace_bold[font_names_monospace_count * 2];

    for (int i = 0; i < font_names_monospace_count; ++i)
    {
        font_names_monospace_bold[i*2] = font_names_monospace[i*2];
        font_names_monospace_bold[i*2 + 1] = "Bold";
    }

    const char *ui_font_path = ff_find_first_font_path(fc, (const char**)font_names_ui, font_names_ui_count * 2, nullptr);
    const char *monospace_font_path = ff_find_first_font_path(fc, (const char**)font_names_monospace, font_names_monospace_count * 2, nullptr);
    const char *monospace_bold_font_path = ff_find_first_font_path(fc, (const char**)font_names_monospace_bold, font_names_monospace_count * 2, nullptr);

    assert(ui_font_path != nullptr);
    assert(monospace_font_path != nullptr);
    assert(monospace_bold_font_path != nullptr);

    ImGuiIO &io = ImGui::GetIO(); (void)io;
    ui->fonts.ui = io.Fonts->AddFontFromFileTTF(ui_font_path, (float)font_size);
    ui->fonts.mono = io.Fonts->AddFontFromFileTTF(monospace_font_path, (float)font_size);
    ui->fonts.mono_bold = io.Fonts->AddFontFromFileTTF(monospace_bold_font_path, (float)font_size);
}
