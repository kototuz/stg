#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_WIDTH  800
#define DEFAULT_HEIGHT 600

#define FONT_PATH        "resources/Iosevka-Regular.ttf"
#define FONT_SIZE        48.0f
#define FONT_GLYPH_COUNT 9608

#define CHAT_BG_COLOR (Color){0x0f,0x0f,0x0f,0xff}

#define MSG_BG_COLOR          (Color){0x21,0x21,0x21,0xff}
#define MSG_FG_COLOR          WHITE
#define MSG_REC_ROUNDNESS     20
#define MSG_REC_SEGMENT_COUNT 20
#define MSG_TEXT_MARGIN       10.0f
#define MSG_TEXT_PADDING      15.0f
#define MSG_AUTHOR_NAME_COLOR GREEN

#define TED_BG_COLOR     MSG_BG_COLOR
#define TED_FG_COLOR     WHITE
#define TED_CURSOR_CODE  L'▏'
#define TED_CURSOR_COLOR WHITE

#define EMACS_KEYMAP

// mappings
#define KEY(k) (IsKeyPressed(KEY_ ## k) || IsKeyPressedRepeat(KEY_ ## k))
#ifdef EMACS_KEYMAP
#   define KEYMAP_MOVE_FORWARD       (IsKeyDown(KEY_LEFT_CONTROL) && KEY(F))
#   define KEYMAP_MOVE_BACKWARD      (IsKeyDown(KEY_LEFT_CONTROL) && KEY(B))
#   define KEYMAP_MOVE_FORWARD_WORD  (IsKeyDown(KEY_LEFT_ALT)     && KEY(F))
#   define KEYMAP_MOVE_BACKWARD_WORD (IsKeyDown(KEY_LEFT_ALT)     && KEY(B))
#   define KEYMAP_DELETE             (IsKeyDown(KEY_LEFT_CONTROL) && KEY(H))
#   define KEYMAP_MOVE_UP            (IsKeyDown(KEY_LEFT_CONTROL) && KEY(P))
#   define KEYMAP_MOVE_DOWN          (IsKeyDown(KEY_LEFT_CONTROL) && KEY(N))
#   define KEYMAP_MOVE_END           (IsKeyDown(KEY_LEFT_CONTROL) && KEY(E))
#   define KEYMAP_MOVE_BEGIN         (IsKeyDown(KEY_LEFT_CONTROL) && KEY(A))
#   define KEYMAP_DELETE_WORD        (IsKeyDown(KEY_LEFT_CONTROL) && KEY(W))
#   define KEYMAP_DELETE_LINE        (IsKeyDown(KEY_LEFT_CONTROL) && KEY(U))
#   define KEYMAP_SEND_MESSAGE       (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_J))
#else
#   define KEYMAP_MOVE_FORWARD       (KEY(RIGHT))
#   define KEYMAP_MOVE_BACKWARD      (KEY(LEFT))
#   define KEYMAP_MOVE_FORWARD_WORD  (IsKeyDown(KEY_LEFT_CONTROL) && KEY(LEFT))
#   define KEYMAP_MOVE_BACKWARD_WORD (IsKeyDown(KEY_LEFT_CONTROL) && KEY(RIGHT))
#   define KEYMAP_DELETE             (KEY(BACKSPACE))
#   define KEYMAP_MOVE_UP            (KEY(UP))
#   define KEYMAP_MOVE_DOWN          (KEY(DOWN))
#   define KEYMAP_MOVE_END           (KEY(HOME))
#   define KEYMAP_MOVE_BEGIN         (KEY(END))
#   define KEYMAP_DELETE_WORD        (IsKeyDown(KEY_LEFT_CONTROL) && KEY(BACKSPACE))
#   define KEYMAP_DELETE_LINE        (false)
#   define KEYMAP_SEND_MESSAGE       (IsKeyPressed(KEY_ENTER))
#endif

#endif
