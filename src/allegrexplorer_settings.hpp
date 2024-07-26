
#pragma once

// These here are settings that are automatically saved on exit
// and loaded on start.

struct allegrexplorer_settings
{
    struct _window
    {
        int width;
        int height;
        int x;
        int y;
        bool maximized;
    } window;
};

void settings_init();

allegrexplorer_settings *settings_get();
