#ifndef CHAT_H
#define CHAT_H

#include <string_view>

#define MESSAGES_CAPACITY 128

namespace chat {
    struct WStr {
        wchar_t *data;
        size_t len;
        static WStr from(const char *cstr);
    };

    struct Msg {
        std::int64_t id;
        WStr text; // copied from the message
        std::wstring_view author_name; // because we preload users
        bool is_mine;
        Msg *reply_to;
    };

    void init();
    void render(float bottom_margin);
    void push_msg(Msg msg);
    Msg *find_msg(std::int64_t msg_id);
    void select_prev_msg();
    void select_next_msg();
    Msg *get_selected_msg();
    Msg *get_msgs(size_t *count);
}

#endif
