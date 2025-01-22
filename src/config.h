#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_WIDTH  800
#define DEFAULT_HEIGHT 600

#define FONT_GLYPH_COUNT 30000

#define CHAT_VIEW_WIDTH 950.0f
const Color CHAT_BG_COLOR = {0x12, 0x12, 0x12, 0xff};

#define MSG_AUTHOR_NAME_FONT_PATH  "resources/Roboto-Regular.ttf"
#define MSG_AUTHOR_NAME_FONT_SIZE  28.0f
#define MSG_TEXT_FONT_PATH         "resources/Roboto-Regular.ttf"
#define MSG_TEXT_FONT_SIZE         30.0f
const Color MSG_BG_COLOR          = {0x21,0x21,0x21,0xff};
const Color MSG_MY_FG_COLOR       = WHITE;
const Color MSG_OTHERS_FG_COLOR   = WHITE;
const Color MSG_MY_BG_COLOR       = {0x35, 0x78, 0x94, 0xff};
const Color MSG_OTHERS_BG_COLOR   = {0x26, 0x26, 0x26, 0xff};
const Color MSG_MY_NAME_COLOR     = {0x26, 0xd9, 0x83, 0xff};
const Color MSG_OTHERS_NAME_COLOR = {0xff, 0xa3, 0xf4, 0xff};
#define MSG_FG_COLOR               WHITE
#define MSG_REC_ROUNDNESS          40
#define MSG_REC_SEGMENT_COUNT      40
#define MSG_TEXT_MARGIN_LEFT_RIGHT 20.0f
#define MSG_TEXT_PADDING           15.0f
#define MSG_DISTANCE               10.0f
const Color MSG_SELECTED_COLOR      = {0x08, 0x08, 0x08, 0xff};
#define MSG_REPLY_PADDING          10.0f
#define MSG_REPLY_MARGIN           8.0f
const Color MSG_REPLY_BG_COLOR      = {0xff, 0xff, 0xff, 0x20};
#define MY_MSG_COLOR               GREEN
#define NOT_MY_MSG_COLOR           RED

#define TED_FONT_PATH         "resources/Roboto-Regular.ttf"
#define TED_FONT_SIZE         30.0f
#define TED_BG_COLOR          MSG_BG_COLOR
#define TED_FG_COLOR          WHITE
#define TED_CURSOR_CODE       L'█'
#define TED_CURSOR_COLOR      WHITE
const Color TED_PLACEHOLDER_COLOR = {0x40, 0x40, 0x40, 0xff};
#define TED_PADDING           15.0f
#define TED_MARGIN            20.0f
#define TED_REC_ROUNDNESS     40
#define TED_REC_SEGMENT_COUNT 40

#define MY_COLOR    GREEN
#define THEIR_COLOR RED

#define COMMAND_START_SYMBOL ':'

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
#   define KEYMAP_NEW_LINE           (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_J))
#   define KEYMAP_SELECT_PREV        (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_P))
#   define KEYMAP_SELECT_NEXT        (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_N))
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
#   define KEYMAP_NEW_LINE           (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_ENTER))
#   define KEYMAP_SELECT_PREV        (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_UP))
#   define KEYMAP_SELECT_NEXT        (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DOWN))
#endif

#endif
