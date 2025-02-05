#ifndef CONFIG_H
#define CONFIG_H

#include <raylib.h>

#define DEFAULT_WIDTH  800
#define DEFAULT_HEIGHT 600

#define FONT_GLYPH_COUNT 30000

#define CHAT_VIEW_WIDTH   950.0f
#define CHAT_BG_COLOR     CLITERAL(Color){0x12, 0x12, 0x12, 0xff}
#define CHAT_SCROLL_SPEED 100

// NOTE: Specify integer not float (e.g. X(28) not X(28.0f))
#define LIST_OF_EMOJI_SIZES \
    X(28) \
    X(25) \

#define LIST_OF_FONTS \
    X(ROBOTO_REGULAR_28, "resources/Roboto-Regular.ttf", 28.0f, EMOJI_SIZE_28) \
    X(ROBOTO_BOLD_28, "resources/Roboto-Bold.ttf", 28.0f, EMOJI_SIZE_28) \
    X(ROBOTO_REGULAR_25, "resources/Roboto-Regular.ttf", 25.0f, EMOJI_SIZE_25) \
    X(ROBOTO_BOLD_25, "resources/Roboto-Bold.ttf", 25.0f, EMOJI_SIZE_25) \

enum EmojiSize {
#define X(emoji_size) EMOJI_SIZE_ ## emoji_size,
    LIST_OF_EMOJI_SIZES
#undef X
    EMOJI_SIZE_COUNT,
};

enum FontId {
#define X(name, ...) FONT_ID_ ## name,
    LIST_OF_FONTS
#undef X
    FONT_ID_COUNT,
};

#define LIST_OF_BOX_MODELS \
    X(MSG, 2.0, 2.0, 0.0, 0.0, 10.0, 12.0, 11.0, 10.0)   \
    X(TED, 15.0, 25.0, 0.0, 0.0, 21.0, 21.0, 20.0, 20.0) \

struct BoxModel {
#define X(name, tm, bm, lm, rm, tp, bp, lp, rp)                 \
    static constexpr float name ## _TM = tm; /*Top margin*/     \
    static constexpr float name ## _BM = bm; /*Bottom margin*/  \
    static constexpr float name ## _LM = lm; /*Left margin*/    \
    static constexpr float name ## _RM = rm; /*Right margin*/   \
    static constexpr float name ## _TP = tp; /*Top padding*/    \
    static constexpr float name ## _BP = bp; /*Bottom padding*/ \
    static constexpr float name ## _LP = lp; /*Left padding*/   \
    static constexpr float name ## _RP = rp; /*Right padding*/
    LIST_OF_BOX_MODELS
#undef X
};

// 0 element is the palette for other messages
// 1 element is the palette for my messages
const struct {
    Color bg_color;
    Color fg_color;
    Color sender_name_color;
    Color reply_bg_color;
} msg_color_palette[2] = {
    {{0x21, 0x21, 0x21, 0xff}, WHITE, {0x40, 0x8a, 0xcf, 0xff}, {0x25, 0x2c, 0x33, 0xff}},
    {{0x76, 0x6a, 0xc8, 0xff}, WHITE, {0xf6, 0x81, 0x36, 0xff}, {0x37, 0x2b, 0x24, 0xff}},
};

#define MSG_REPLY_BG_COLOR_IN_MY_MSG  CLITERAL(Color){0x87, 0x75, 0xda, 0xff}
#define MSG_SENDER_NAME_FONT_ID       FONT_ID_ROBOTO_BOLD_28
#define MSG_TEXT_FONT_ID              FONT_ID_ROBOTO_REGULAR_28
#define MSG_REC_ROUNDNESS             40
#define MSG_REC_SEGMENT_COUNT         40
#define MSG_DISTANCE                  4.0f
#define MSG_SELECTED_COLOR            CLITERAL(Color){0x08, 0x08, 0x08, 0xff}
#define MSG_REPLY_TEXT_FONT_ID        FONT_ID_ROBOTO_REGULAR_25
#define MSG_REPLY_SENDER_NAME_FONT_ID FONT_ID_ROBOTO_BOLD_25
#define MSG_REPLY_PADDING             10.0f
#define MSG_REPLY_REC_ROUNDNESS       30
#define MSG_REPLY_REC_SEGMENT_COUNT   30
#define MSG_WIDGET_DISTANCE           6.0f

#define TED_FONT_ID           FONT_ID_ROBOTO_REGULAR_28
#define TED_BG_COLOR          CLITERAL(Color){0x21, 0x21, 0x21, 0xff}
#define TED_FG_COLOR          WHITE
#define TED_CURSOR_COLOR      WHITE
#define TED_PLACEHOLDER_COLOR CLITERAL(Color){0x40, 0x40, 0x40, 0xff}
#define TED_REC_ROUNDNESS     40
#define TED_REC_SEGMENT_COUNT 40
#define COMMAND_START_SYMBOL  ':'

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
