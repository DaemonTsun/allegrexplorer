
#include <time.h>
#include "imgui.h"
#include "shl/fixed_array.hpp"
#include "shl/format.hpp"
#include "log_window.hpp"

enum log_level
{
    LevelMessage,
    LevelError
};

struct log_message_t
{
    log_level level;
    struct tm timestamp;
    string  message;
};

#define LOG_MAX_MESSAGES 256
#define LOG_MESSAGE_MASK (LOG_MAX_MESSAGES - 1)

struct _log_data
{
    fixed_array<log_message_t, LOG_MAX_MESSAGES> messages;
    s64 message_index;
    s64 message_count;
};

static _log_data *_get_log()
{
    static _log_data *log_data = nullptr;

    if (log_data == nullptr)
    {
        log_data = alloc<_log_data>();
        fill_memory(log_data, 0);
    }

    return log_data;
}

static s64 _next_index(_log_data *log)
{
    log->message_index = (log->message_index + 1) & LOG_MESSAGE_MASK;
    
    if (log->message_count < LOG_MAX_MESSAGES)
        log->message_count += 1;

    return log->message_index;
}

void log_message(const_string msg)
{
    auto *log = _get_log();

    s64 index = _next_index(log);
    log_message_t *mt = log->messages.data + index;

    mt->level = LevelMessage;
    time_t t = time(nullptr);
    mt->timestamp = *localtime(&t);
    set_string(&mt->message, msg);
}

void log_error(const_string msg)
{
    auto *log = _get_log();

    s64 index = _next_index(log);
    log_message_t *mt = log->messages.data + index;

    mt->level = LevelError;
    time_t t = time(nullptr);
    mt->timestamp = *localtime(&t);
    set_string(&mt->message, msg);
}

void log_error(const_string msg, error *err)
{
    auto *log = _get_log();

    s64 index = _next_index(log);
    log_message_t *mt = log->messages.data + index;

    mt->level = LevelError;
    time_t t = time(nullptr);
    mt->timestamp = *localtime(&t);
#ifndef NDEBUG
    format(&mt->message, "[%:%] % error: %", err->file, (s64)err->line, msg, err->what);
#else
    format(&mt->message, "% error: %", msg, err->what);
#endif
}

void log_clear()
{
    auto *log = _get_log();
    
    for (int i = 0; i < LOG_MAX_MESSAGES; ++i)
    {
        free(&log->messages[i].message);
        init(&log->messages[i].message);
    }

    log->message_index = 0;
    log->message_count = 0;
}

void log_window(ImFont *monospace_font)
{
    static bool only_errors = false;
    auto *log = _get_log();

    if (ImGui::Begin("Log Messages"))
    {
        if (ImGui::Button("Clear"))
            log_clear();

        ImGui::SameLine();
        ImGui::Checkbox("Only display errors", &only_errors);

        if (monospace_font != nullptr)
            ImGui::PushFont(monospace_font);

        for (s64 i = 0; i < log->message_count; ++i)
        {
            s64 index = (log->message_index - i) & LOG_MESSAGE_MASK;
            auto *msg = log->messages.data + index;

            if (only_errors && msg->level != LevelError)
                continue;

            if (msg->level == LevelError)
                ImGui::PushStyleColor(ImGuiCol_Text, (u32)ImColor(0xff, 0x99, 0x99));

            const_string text = tformat("[%d-%02d-%02d %02d:%02d:%02d] %",
                msg->timestamp.tm_year + 1900,
                msg->timestamp.tm_mon  + 1,
                msg->timestamp.tm_mday,
                msg->timestamp.tm_hour,
                msg->timestamp.tm_min,
                msg->timestamp.tm_sec,
                msg->message);

            ImGui::PushID(i);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##", (char*)text.c_str, text.size, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopID();

            if (msg->level == LevelError)
                ImGui::PopStyleColor();
        }

        if (monospace_font != nullptr)
            ImGui::PopFont();
    }

    ImGui::End();
}
