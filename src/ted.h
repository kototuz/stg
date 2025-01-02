#ifndef TED_H
#define TED_H

#include <cstdlib>

#include "common.h"

#define MAX_MSG_LEN 4096

namespace ted {
    enum Motion {
        MOTION_FORWARD_WORD,
        MOTION_BACKWARD_WORD,
        MOTION_FORWARD,
        MOTION_BACKWARD,
        MOTION_UP,
        MOTION_DOWN,
        MOTION_END,
        MOTION_BEGIN,
    };

    struct Pos {
        size_t col;
        size_t row;
    };

    struct State {
        common::Lines lines;
        Pos cursor_pos;
        size_t max_line_len;
        int buffer[MAX_MSG_LEN];
    };

    // Global variable because we need only one text editor, nerdes
    extern State state;

    void init();
    void render();
    void clear();
    void move_cursor_to_ptr(int *text_ptr);
    void try_cursor_motion(Motion m);
    void insert_symbol(int s);
    void delete_symbols(size_t count);
    void delete_word();
    void delete_line();
}

#endif
