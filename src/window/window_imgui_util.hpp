
#pragma once

struct GLFWwindow;

void window_init();
void window_exit();

GLFWwindow *create_window(const char *title, int width, int height);
void close_window(GLFWwindow *window);
void destroy_window(GLFWwindow *window);

float get_window_scaling(GLFWwindow *window);

// events
typedef void (*keyboard_callback)(GLFWwindow *window, int key, int scancode, int action, int mods);
void set_window_keyboard_callback(GLFWwindow *window, keyboard_callback cb);

typedef void (*event_loop_update_callback)(GLFWwindow *, double);
typedef void (*event_loop_render_callback)(GLFWwindow *, double);

// basically just renders the UI
void default_render_function(GLFWwindow *window, double dt);

void window_event_loop(GLFWwindow *window
                     , event_loop_update_callback update
                     , event_loop_render_callback render = default_render_function
                     , double min_fps = -1.0);

// UI
void ui_init(GLFWwindow *window);
void ui_exit(GLFWwindow *window);

void ui_new_frame();
void ui_end_frame();
void ui_set_next_window_full_size();
