/* Doom on the calculator: the keyboard.
 *
 * Each calculator key does one of Doom's actions (nw_actions), which it does as the key of Doom's PC
 * keyboard that does it would: pressed and released with the calculator's key. The keys can be given any
 * action in the options (Keys); they are kept in doom.cfg. As they come:
 *   arrows        walk and turn
 *   OK            fire
 *   Back          use: open doors, press switches
 *   EXE, backspace the menu (it pauses the game)
 *   shift         strafe with Left and Right, while held
 *   alpha         run, while held
 *   x,n,t  var    strafe left, strafe right
 *   toolbox       the automap; in it, + and - zoom, x follows, / shows the grid, 0 shows the whole map,
 *                 8 puts a mark, 9 takes them away
 *   1 to 7        the weapons; ( and ) the previous and next one you have
 *   + -           the screen bigger, smaller
 *   sqrt          brightness
 *   ln, log       quick save, quick load
 *   pi            pause
 *   Ans           type a cheat
 *   Home, On/Off  keep the moment and quit
 * In menus: the arrows, OK and EXE to choose, Back (or backspace) to go back. */
#include <string.h>

#include "doomtype.h"
#include "doomkeys.h"
#include "d_event.h"
#include "i_input.h"
#include "i_video.h"
#include "m_controls.h"
#include "doom/doomstat.h"
#include "doom/d_player.h"
#include "plat.h"
#include "nw.h"

int M_BackKey(void);
boolean M_WantsEnter(void);

boolean nw_home;
boolean nw_always_run;
/* the Doom key each held key pressed (released with the same one), or NW_SENT_NONE for one that pressed none */
#define NW_SENT_NONE 0x100
static uint16_t sent[NK_COUNT];

/* ------------------------------------------------------------------ actions */
const nw_action_t nw_actions[NA_COUNT] = {
    [NA_NONE] = {"NOTHING", 0},
    [NA_FORWARD] = {"WALK FORWARD", KEY_UPARROW},
    [NA_BACKWARD] = {"WALK BACK", KEY_DOWNARROW},
    [NA_TURNLEFT] = {"TURN LEFT", KEY_LEFTARROW},
    [NA_TURNRIGHT] = {"TURN RIGHT", KEY_RIGHTARROW},
    [NA_FIRE] = {"FIRE", KEY_RCTRL},
    [NA_USE] = {"USE: DOORS, SWITCHES", ' '},
    [NA_STRAFE] = {"STRAFE (HELD)", KEY_RALT},
    [NA_RUN] = {"RUN (HELD)", KEY_RSHIFT},
    [NA_STRAFELEFT] = {"STRAFE LEFT", ','},
    [NA_STRAFERIGHT] = {"STRAFE RIGHT", '.'},
    [NA_WEAPON1] = {"FIST, CHAINSAW", '1'},
    [NA_WEAPON2] = {"PISTOL", '2'},
    [NA_WEAPON3] = {"SHOTGUN", '3'},
    [NA_WEAPON4] = {"CHAINGUN", '4'},
    [NA_WEAPON5] = {"ROCKET LAUNCHER", '5'},
    [NA_WEAPON6] = {"PLASMA GUN", '6'},
    [NA_WEAPON7] = {"BFG 9000", '7'},
    [NA_AUTOMAP] = {"AUTOMAP", KEY_TAB},
    [NA_MENU] = {"MENU", KEY_ESCAPE},
    [NA_PAUSE] = {"PAUSE", KEY_PAUSE},
    [NA_BIGGER] = {"BIGGER; MAP: ZOOM IN", '='},
    [NA_SMALLER] = {"SMALLER; MAP: ZOOM OUT", '-'},
    [NA_FOLLOW] = {"MAP: FOLLOW", 'f'},
    [NA_GRID] = {"MAP: GRID", 'g'},
    [NA_WHOLEMAP] = {"MAP: WHOLE MAP", '0'},
    [NA_MARK] = {"MAP: MARK", 'm'},
    [NA_CLEARMARKS] = {"MAP: CLEAR MARKS", 'c'},
    [NA_GAMMA] = {"BRIGHTNESS", KEY_F11},
    [NA_QUICKSAVE] = {"QUICK SAVE", KEY_F6},
    [NA_QUICKLOAD] = {"QUICK LOAD", KEY_F9},
    [NA_MESSAGES] = {"MESSAGES ON, OFF", KEY_F8},
    [NA_DETAIL] = {"DETAIL HIGH, LOW", KEY_F5},
    [NA_CHEAT] = {"TYPE A CHEAT", 0},
    [NA_NEXTWEAPON] = {"NEXT WEAPON", ']'},
    [NA_PREVWEAPON] = {"PREVIOUS WEAPON", '['},
};

/* the calculator's keys, as the options and Read This! show them ("" for those that are not keys) */
static const char *const key_names[NK_COUNT] = {
    [NK_LEFT] = "LEFT", [NK_UP] = "UP", [NK_DOWN] = "DOWN", [NK_RIGHT] = "RIGHT", [NK_OK] = "OK",
    [NK_BACK] = "BACK", [NK_HOME] = "HOME", [NK_ONOFF] = "ON/OFF", [NK_SHIFT] = "SHIFT", [NK_ALPHA] = "ALPHA",
    [NK_XNT] = "X,N,T", [NK_VAR] = "VAR", [NK_TOOLBOX] = "TOOLBOX", [NK_BACKSPACE] = "BACKSPACE",
    [NK_EXP] = "E^X", [NK_LN] = "LN", [NK_LOG] = "LOG", [NK_IMAG] = "I", [NK_COMMA] = "COMMA",
    [NK_POWER] = "X^Y", [NK_SIN] = "SIN", [NK_COS] = "COS", [NK_TAN] = "TAN", [NK_PI] = "PI",
    [NK_SQRT] = "SQUARE ROOT", [NK_SQUARE] = "X^2", [NK_SEVEN] = "7", [NK_EIGHT] = "8", [NK_NINE] = "9",
    [NK_LPAREN] = "(", [NK_RPAREN] = ")", [NK_FOUR] = "4", [NK_FIVE] = "5", [NK_SIX] = "6", [NK_MUL] = "*",
    [NK_DIV] = "/", [NK_ONE] = "1", [NK_TWO] = "2", [NK_THREE] = "3", [NK_PLUS] = "+", [NK_MINUS] = "-",
    [NK_ZERO] = "0", [NK_DOT] = ".", [NK_EE] = "X10^X", [NK_ANS] = "ANS", [NK_EXE] = "EXE",
};

const char *nw_key_name(int k) { return k >= 0 && k < NK_COUNT && key_names[k] ? key_names[k] : ""; }
boolean nw_key_bindable(int k) { return k >= 0 && k < NK_COUNT && key_names[k] && k != NK_HOME && k != NK_ONOFF; }

uint8_t nw_binding[NK_COUNT];

void nw_bindings_default(void) {
    static const uint8_t def[][2] = {
        {NK_UP, NA_FORWARD}, {NK_DOWN, NA_BACKWARD}, {NK_LEFT, NA_TURNLEFT}, {NK_RIGHT, NA_TURNRIGHT},
        {NK_OK, NA_FIRE}, {NK_BACK, NA_USE}, {NK_EXE, NA_MENU}, {NK_BACKSPACE, NA_MENU},
        {NK_SHIFT, NA_STRAFE}, {NK_ALPHA, NA_RUN}, {NK_XNT, NA_STRAFELEFT}, {NK_VAR, NA_STRAFERIGHT},
        {NK_ONE, NA_WEAPON1}, {NK_TWO, NA_WEAPON2}, {NK_THREE, NA_WEAPON3}, {NK_FOUR, NA_WEAPON4},
        {NK_FIVE, NA_WEAPON5}, {NK_SIX, NA_WEAPON6}, {NK_SEVEN, NA_WEAPON7}, {NK_TOOLBOX, NA_AUTOMAP},
        {NK_PLUS, NA_BIGGER}, {NK_MINUS, NA_SMALLER}, {NK_MUL, NA_FOLLOW}, {NK_DIV, NA_GRID},
        {NK_ZERO, NA_WHOLEMAP}, {NK_EIGHT, NA_MARK}, {NK_NINE, NA_CLEARMARKS}, {NK_SQRT, NA_GAMMA},
        {NK_LN, NA_QUICKSAVE}, {NK_LOG, NA_QUICKLOAD}, {NK_PI, NA_PAUSE}, {NK_ANS, NA_CHEAT},
        {NK_LPAREN, NA_PREVWEAPON}, {NK_RPAREN, NA_NEXTWEAPON},
    };
    memset(nw_binding, NA_NONE, sizeof nw_binding);
    for (unsigned i = 0; i < sizeof def / sizeof def[0]; i++) nw_binding[def[i][0]] = def[i][1];
}

/* the keys that do an action ("" if none), as "OK, EXE" */
const char *nw_keys_of(int action, char *buf, int size) {
    int n = 0;
    buf[0] = 0;
    for (int k = 0; k < NK_COUNT && n < size - 1; k++) {
        if (nw_binding[k] != action || !nw_key_bindable(k)) continue;
        const char *name = key_names[k];
        int len = (int)strlen(name);
        if (n + len + (n ? 2 : 0) >= size) break;
        if (n) buf[n++] = ',', buf[n++] = ' ';
        memcpy(buf + n, name, len + 1);
        n += len;
    }
    return buf;
}

/* the options are choosing a key for an action: the next key pressed is it */
int nw_capture = -1;

/* ------------------------------------------------------------------ cheats
 * Doom's cheats are typed on its keyboard: here each key types the letter it types after alpha on the
 * calculator, and the numbers after IDCLEV; Back, OK or EXE stop */
boolean nw_typing;
static char typed[16];
extern boolean message_dontfuckwithme;

static int letter_of(int k) {
    static const char letters[NK_COUNT] = {
        [NK_EXP] = 'a', [NK_LN] = 'b', [NK_LOG] = 'c', [NK_IMAG] = 'd', [NK_COMMA] = 'e', [NK_POWER] = 'f',
        [NK_SIN] = 'g', [NK_COS] = 'h', [NK_TAN] = 'i', [NK_PI] = 'j', [NK_SQRT] = 'k', [NK_SQUARE] = 'l',
        [NK_SEVEN] = 'm', [NK_EIGHT] = 'n', [NK_NINE] = 'o', [NK_LPAREN] = 'p', [NK_RPAREN] = 'q',
        [NK_FOUR] = 'r', [NK_FIVE] = 's', [NK_SIX] = 't', [NK_MUL] = 'u', [NK_DIV] = 'v', [NK_ONE] = 'w',
        [NK_TWO] = 'x', [NK_THREE] = 'y', [NK_PLUS] = 'z',
    };
    static const char digits[NK_COUNT] = {
        [NK_ZERO] = '0', [NK_ONE] = '1', [NK_TWO] = '2', [NK_THREE] = '3', [NK_FOUR] = '4', [NK_FIVE] = '5',
        [NK_SIX] = '6', [NK_SEVEN] = '7', [NK_EIGHT] = '8', [NK_NINE] = '9',
    };
    /* (the numbers of IDCLEV and IDMUS: the episode and the map, the music) */
    int n = (int)strlen(typed);
    for (int i = n; i >= 0 && i >= n - 1; i--) {
        if (i < n && (typed[i] < '0' || typed[i] > '9')) break;
        if ((i >= 6 && !memcmp(typed + i - 6, "idclev", 6)) || (i >= 5 && !memcmp(typed + i - 5, "idmus", 5)))
            if (digits[k]) return digits[k];
    }
    return letters[k];
}

static void show_typed(void) {
    static char line[40];
    strcpy(line, "CHEAT: ");
    for (int i = 0; typed[i]; i++) line[7 + i] = typed[i] >= 'a' && typed[i] <= 'z' ? typed[i] - 32 : typed[i];
    line[7 + strlen(typed)] = '_';
    line[8 + strlen(typed)] = 0;
    players[consoleplayer].message = line;
    message_dontfuckwithme = true;
}

void nw_cheat_start(void) {
    nw_typing = true;
    typed[0] = 0;
    show_typed();
}

static void cheat_key(int k) {
    if (k == NK_BACK || k == NK_OK || k == NK_EXE) {
        nw_typing = false;
        return;
    }
    int c = letter_of(k);
    if (!c) return;
    int n = (int)strlen(typed);
    if (n == (int)sizeof typed - 1) memmove(typed, typed + 1, n--);
    typed[n] = (char)c;
    typed[n + 1] = 0;
    show_typed();
    /* the letter, as Doom's keyboard would send it: the cheats see it (and say so when one works) */
    event_t ev = {ev_keydown, c, c, 0, 0, 0};
    D_PostEvent(&ev);
    ev.type = ev_keyup;
    D_PostEvent(&ev);
}

/* ------------------------------------------------------------------ the keys */
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
    key_map_mark = 'm';
    key_map_clearmark = 'c';
    if (!nw_binding[NK_OK] && !nw_binding[NK_UP]) nw_bindings_default();
}

static int doom_key(int k) {
    if (M_WantsEnter()) {
        /* menus and questions: arrows, OK and EXE choose, Back and backspace go back */
        switch (k) {
            case NK_LEFT: return KEY_LEFTARROW;
            case NK_RIGHT: return KEY_RIGHTARROW;
            case NK_UP: return KEY_UPARROW;
            case NK_DOWN: return KEY_DOWNARROW;
            case NK_OK:
            case NK_EXE: return KEY_ENTER;
            case NK_BACK: return M_BackKey();
            case NK_BACKSPACE: return KEY_BACKSPACE;
            default: break;
        }
        if (nw_binding[k] == NA_MENU) return KEY_ESCAPE;
    }
    int a = nw_binding[k];
    return a < NA_COUNT ? nw_actions[a].key : 0;
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
            if (nw_capture >= 0) {
                /* the options: this key is given the action */
                if (nw_key_bindable(k)) {
                    nw_binding[k] = (uint8_t)nw_capture;
                    nw_capture = -1;
                }
                sent[k] = NW_SENT_NONE;
                continue;
            }
            if (nw_typing && !M_WantsEnter()) {
                cheat_key(k);
                sent[k] = NW_SENT_NONE;
                continue;
            }
            if (!M_WantsEnter() && nw_binding[k] == NA_CHEAT && gamestate == GS_LEVEL) {
                nw_cheat_start();
                sent[k] = NW_SENT_NONE;
                continue;
            }
            int key = doom_key(k);
            if (!key) continue;
            sent[k] = (uint16_t)key;
            event_t ev = {ev_keydown, key, key, 0, 0, 0};
            D_PostEvent(&ev);
        } else if (!down && sent[k]) {
            if (sent[k] != NW_SENT_NONE) {
                event_t ev = {ev_keyup, sent[k], 0, 0, 0, 0};
                D_PostEvent(&ev);
            }
            sent[k] = 0;
        }
    }
}

void I_StartTextInput(int x1, int y1, int x2, int y2) {}
void I_StopTextInput(void) {}
void I_BindInputVariables(void) {}
void I_InputInit(void) {}
