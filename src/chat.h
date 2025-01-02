#ifndef CHAT_H
#define CHAT_H

#define MAX_MSG_COUNT 3

namespace chat {
    void init();
    void render();
    void push_err(const int *err_msg);
    void push_msg(const int *msg, size_t msg_len, const int *author, size_t author_len);
}

#endif
