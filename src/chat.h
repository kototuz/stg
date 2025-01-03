#ifndef CHAT_H
#define CHAT_H

#define MAX_MSG_COUNT 3

namespace chat {
    void init();
    void render();
    void push_msg(const wchar_t *msg, size_t msg_len, const wchar_t *author, size_t author_len);
}

#endif
