#ifndef CHAT_H
#define CHAT_H

#include <string_view>

#include <raylib.h>

#define MAX_MSG_COUNT 128

namespace chat {
    void init();
    void render(float bottom_margin);
    void push_msg(std::wstring_view text, std::wstring_view author_name, bool is_mine = false);
}

#endif
