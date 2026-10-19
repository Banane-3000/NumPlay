/* Doom on the calculator: the keyboard.
 *
 * Each calculator key stands for a key of Doom's PC keyboard, pressed and released with it:
 *   arrows        the arrow keys: walk and turn
 *   OK            fire (Ctrl); Enter while a menu or a question is up
 *   Back          the menu (Escape), back one menu (Backspace)
 *   backspace     use: open doors, press switches (Space)
 *   EXE           use as well; Enter in menus
 *   shift         strafe with Left and Right, while held (Alt)
 *   alpha         run, while held (Shift)
 *   x,n,t  var    strafe left, strafe right (, and .)
 *   toolbox       the automap (Tab); in it, + and - zoom, x follows, / shows the grid, 0 zooms all the way out
 *   1 to 7        the weapons
 *   sqrt          brightness (F11)
 *   Home, On/Off  keep the moment and quit */
#include <string.h>

#include "doomtype.h"
#include "doomkeys.h"
#include "d_event.h"
#include "i_input.h"
#include "i_video.h"
#include "m_controls.h"
#include "doom/doomstat.h"
#include "plat.h"
#include "nw.h"

/* the calculator's keys (eadk_key_*) */
enum {
    NK_LEFT = 0, NK_UP = 1, NK_DOWN = 2, NK_RIGHT = 3, NK_OK = 4, NK_BACK = 5, NK_HOME = 6, NK_ONOFF = 8,
    NK_SHIFT = 12, NK_ALPHA = 13, NK_XNT = 14, NK_VAR = 15, NK_TOOLBOX = 16, NK_BACKSPACE = 17,
    NK_SQRT = 28, NK_SEVEN = 30, NK_EIGHT = 31, NK_NINE = 32, NK_FOUR = 36, NK_FIVE = 37, NK_SIX = 38,
    NK_MUL = 39, NK_DIV = 40, NK_ONE = 42, NK_TWO = 43, NK_THREE = 44, NK_PLUS = 45, NK_MINUS = 46,
    NK_ZERO = 48, NK_EXE = 52, NK_COUNT = 53
};

int M_BackKey(void);
boolean M_WantsEnter(void);

boolean nw_home;
static uint8_t sent[NK_COUNT]; /* the Doom key each held key pressed (released with the same one) */

void nw_input_init(void) {
    // the menus' questions: OK says yes, Back says no
    key_menu_confirm = KEY_ENTER;
    key_menu_abort = KEY_ESCAPE;
    // (and the automap's keys)
    key_map_zoomin = '=';
    key_map_zoomout = '-';
    key_map_follow = 'f';
    key_map_grid = 'g';
    key_map_maxzoom = '0';
}

static int doom_key(int k) {
    switch (k) {
        case NK_LEFT: return KEY_LEFTARROW;
        case NK_RIGHT: return KEY_RIGHTARROW;
        case NK_UP: return KEY_UPARROW;
        case NK_DOWN: return KEY_DOWNARROW;
        case NK_OK: return M_WantsEnter() ? KEY_ENTER : KEY_RCTRL;
        case NK_EXE: return M_WantsEnter() ? KEY_ENTER : ' ';
        case NK_BACK: return M_BackKey();
        case NK_BACKSPACE: return M_WantsEnter() ? KEY_BACKSPACE : ' ';
        case NK_SHIFT: return KEY_RALT;
        case NK_ALPHA: return KEY_RSHIFT;
        case NK_XNT: return ',';
        case NK_VAR: return '.';
        case NK_TOOLBOX: return KEY_TAB;
        case NK_SQRT: return KEY_F11;
        case NK_ONE: return '1';
        case NK_TWO: return '2';
        case NK_THREE: return '3';
        case NK_FOUR: return '4';
        case NK_FIVE: return '5';
        case NK_SIX: return '6';
        case NK_SEVEN: return '7';
        case NK_PLUS: return '=';
        case NK_MINUS: return '-';
        case NK_MUL: return 'f';
        case NK_DIV: return 'g';
        case NK_ZERO: return '0';
        default: return 0;
    }
}

void I_StartTic(void) {
    uint64_t keys = plat_keys();
    if (keys & ((uint64_t)1 << NK_HOME | (uint64_t)1 << NK_ONOFF)) {
        nw_home = nw_quit = true; /* (the main loop ends after this frame) */
        return;
    }
    for (int k = 0; k < NK_COUNT; k++) {
        boolean down = (keys >> k) & 1;
        if (down && !sent[k]) {
            int key = doom_key(k);
            if (!key) continue;
            sent[k] = (uint8_t)key;
            event_t ev = {ev_keydown, key, key, 0, 0, 0};
            D_PostEvent(&ev);
        } else if (!down && sent[k]) {
            event_t ev = {ev_keyup, sent[k], 0, 0, 0, 0};
            sent[k] = 0;
            D_PostEvent(&ev);
        }
    }
}

void I_StartTextInput(int x1, int y1, int x2, int y2) {}
void I_StopTextInput(void) {}
void I_BindInputVariables(void) {}
void I_InputInit(void) {}
