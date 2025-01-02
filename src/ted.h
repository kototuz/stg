#ifndef TED_H
#define TED_H

#include <string>

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

    void init();
    void render();
    void clear();
    void try_cursor_motion(Motion m);
    void insert_symbol(int s);
    void delete_symbols(size_t count);
    void delete_word();
    void delete_line();
    void set_placeholder(const wchar_t *text);
    void set_placeholder(const wchar_t *text, size_t len);
    float get_height();
    std::wstring get_text();
}

#endif
