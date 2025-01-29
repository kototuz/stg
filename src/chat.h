#ifndef CHAT_H
#define CHAT_H

#include <string_view>

#include <raylib.h>

namespace chat {
    struct WStr {
        wchar_t *data;
        size_t len;
        static WStr from(const char *cstr);
        static WStr copy(WStr from);
    };

    struct MsgReplyData {
        std::int64_t id;
        WStr text;
        std::wstring_view sender_name;
        bool is_mine;
    };

    struct MsgData {
        std::int64_t id;
        WStr text;
        std::wstring_view sender_name;
        bool is_mine;
        bool has_reply_to;
        MsgReplyData reply_to;
    };

    void init();
    void render(float bottom_margin, float mouse_wheel_move);
    void push_msg(MsgData msg);
    MsgData *find_msg(std::int64_t msg_id);
    void select_prev_msg();
    void select_next_msg();
    MsgData *get_selected_msg();
}

#endif
