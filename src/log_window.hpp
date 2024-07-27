
#pragma once

#include "shl/string.hpp"
#include "shl/error.hpp"

void log_message(const_string msg);
void log_error(const_string msg);
void log_error(const_string msg, error *err);

void log_clear();

struct ImFont;
void log_window(ImFont *monospace_font = nullptr);
