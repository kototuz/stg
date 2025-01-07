#include <cstdio>

#include <raylib.h>

#include "common.h"
#include "ted.h"
#include "config.h"
#include "chat.h"
#include "tgclient.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <api_id> <api_hash>\n", argv[0]);
        return 1;
    }

    // Disable 'raylib' logging
    SetTraceLogLevel(LOG_NONE);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Simple Telegram");

    common::init();
    ted::init();
    chat::init();
    tgclient::init(argv[1], argv[2]);

    while (!WindowShouldClose()) {
        ClearBackground(CHAT_BG_COLOR);
        if (KEYMAP_MOVE_FORWARD)            ted::try_cursor_motion(ted::MOTION_FORWARD);
        else if (KEYMAP_MOVE_BACKWARD)      ted::try_cursor_motion(ted::MOTION_BACKWARD);
        else if (KEYMAP_MOVE_FORWARD_WORD)  ted::try_cursor_motion(ted::MOTION_FORWARD_WORD);
        else if (KEYMAP_MOVE_BACKWARD_WORD) ted::try_cursor_motion(ted::MOTION_BACKWARD_WORD);
        else if (KEYMAP_MOVE_UP)            ted::try_cursor_motion(ted::MOTION_UP);
        else if (KEYMAP_MOVE_DOWN)          ted::try_cursor_motion(ted::MOTION_DOWN);
        else if (KEYMAP_MOVE_END)           ted::try_cursor_motion(ted::MOTION_END);
        else if (KEYMAP_MOVE_BEGIN)         ted::try_cursor_motion(ted::MOTION_BEGIN);
        else if (KEYMAP_DELETE_WORD)        ted::delete_word();
        else if (KEYMAP_DELETE_LINE)        ted::delete_line();
        else if (KEYMAP_DELETE)             ted::delete_symbols(1);
        else if (KEYMAP_SEND_MESSAGE)       tgclient::process_input();
        else if (KEYMAP_NEW_LINE)           ted::insert_symbol('\n');
        else { // just insert char
            int symbol = GetCharPressed();
            if (symbol != 0) ted::insert_symbol(symbol);
        }

        tgclient::update();

        BeginDrawing();
            ted::render();
            chat::render();
        EndDrawing();
    }
    CloseWindow();
}

// TODO: Reply messages
// TODO: Different font size (maybe also font) for the message author name
// TODO: Load chat history
// TODO: Trim new lines
// TODO: Panel that shows the current chat info: title, online (maybe)
// TODO: Panel where you can select chat
// TODO: Animations
// TODO: Add a way to select messages to operate on them
