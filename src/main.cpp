#include "common.h"
#include "chat.h"
#include "tgclient.h"

int main()
{
    // Disable 'raylib' logging
    SetTraceLogLevel(LOG_NONE);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Simple Telegram");

    common::init();
    chat::init();
    tgclient::init();

    while (!WindowShouldClose()) {
        tgclient::update();
        chat::update();

        BeginDrawing();
            ClearBackground(CHAT_BG_COLOR);
            chat::render();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
