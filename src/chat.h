#ifndef CHAT_H
#define CHAT_H

#include "common.h"

#define MAX_MSG_COUNT 3

namespace chat {
    struct Message {
        common::Lines lines;
        int *author_name;
        size_t author_name_len;

        void free();

        static Message create(const int *text, size_t text_len, const int *author_name, size_t author_name_len);
    };

    struct State {
        Message messages[MAX_MSG_COUNT];
        size_t message_count;
        size_t max_msg_line_len;
    };

    extern State state;

    void init();
    void render();
    void push_err(const int *err_msg);
    void push_msg(const int *msg, size_t msg_len, const int *author, size_t author_len);
}

#endif
