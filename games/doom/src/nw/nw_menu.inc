// Doom on the calculator: what its Options add to vanilla's (included by m_menu.c): Brightness, the keys, the
// cheats (as GBADoom has them) and a page of more choices. Their words are in Doom's small font, as vanilla's
// own messages are.

#include "nw/nw.h"
#include "am_map.h"

extern menu_t OptionsDef;
extern int nw_scan_right_to_left, nw_sync_off;
void AM_NwCheat(void);

static void M_NwBrightness(int choice);
static void M_NwKeys(int choice);
static void M_NwCheats(int choice);
static void M_NwMore(int choice);

// a line of small words in a menu, where its big ones would be
#define NW_TEXT_DY 4
static int nw_value_x = 150;
static void M_NwLine(const menu_t *m, int i, const char *label, const char *value)
{
    M_WriteText(m->x, m->y + LINEHEIGHT * i + NW_TEXT_DY, label);
    if (value)
        M_WriteText(m->x + nw_value_x, m->y + LINEHEIGHT * i + NW_TEXT_DY, value);
}

static void M_NwTitle(const char *title)
{
    int w = M_StringWidth(title);
    M_WriteText(w < SCREENWIDTH ? (SCREENWIDTH - w) / 2 : 0, 16, title);
}

// ---------------------------------------------------------------- brightness (vanilla's gamma, its five levels)
static void M_NwBrightness(int choice)
{
    if (choice == 0 && usegamma > 0)
        usegamma--;
    else if (choice == 1 && usegamma < 4)
        usegamma++;
    players[consoleplayer].message = DEH_String(gammamsg[usegamma]);
    I_SetPaletteNum(0);
}

// ---------------------------------------------------------------- the keys
// the actions a page at a time: choose one, then press the key for it
#define NW_KEY_ROWS 6
static int nw_keys_page;
static const int nw_keys_pages = (NA_COUNT - 1 + NW_KEY_ROWS - 1) / NW_KEY_ROWS;

static void M_NwKeyItem(int choice)
{
    if (choice == NW_KEY_ROWS)
    {
        nw_bindings_default();
        players[consoleplayer].message = "THE KEYS ARE AS THEY CAME";
        return;
    }
    int action = nw_keys_page * NW_KEY_ROWS + choice + 1;
    if (action < NA_COUNT)
        nw_capture = action;
}

static const menuitem_t KeysMenu[NW_KEY_ROWS + 1] = {
    {1, VPATCH_NAME_INVALID, 0, M_NwKeyItem}, {1, VPATCH_NAME_INVALID, 0, M_NwKeyItem},
    {1, VPATCH_NAME_INVALID, 0, M_NwKeyItem}, {1, VPATCH_NAME_INVALID, 0, M_NwKeyItem},
    {1, VPATCH_NAME_INVALID, 0, M_NwKeyItem}, {1, VPATCH_NAME_INVALID, 0, M_NwKeyItem},
    {1, VPATCH_NAME_INVALID, 'd', M_NwKeyItem},
};

static void M_DrawNwKeys(void);
menu_t KeysDef = {NW_KEY_ROWS + 1, &OptionsDef, KeysMenu, M_DrawNwKeys, 40, 34, 0};

static void M_DrawNwKeys(void)
{
    char buf[64], page[32];
    nw_value_x = 150;
    M_NwTitle("KEYS: OK, THEN THE KEY");
    for (int i = 0; i < NW_KEY_ROWS; i++)
    {
        int action = nw_keys_page * NW_KEY_ROWS + i + 1;
        if (action >= NA_COUNT)
            continue;
        const char *keys = nw_capture == action ? "PRESS A KEY" : nw_keys_of(action, buf, 24);
        M_NwLine(&KeysDef, i, nw_actions[action].name, keys[0] ? keys : "-");
    }
    M_NwLine(&KeysDef, NW_KEY_ROWS, "THE KEYS AS THEY CAME", NULL);
    M_snprintf(page, sizeof page, "PAGE %d OF %d: LEFT, RIGHT", nw_keys_page + 1, nw_keys_pages);
    M_WriteText((SCREENWIDTH - M_StringWidth(page)) / 2, KeysDef.y + LINEHEIGHT * (NW_KEY_ROWS + 1) + 2, page);
}

static void M_NwKeys(int choice)
{
    nw_keys_page = 0;
    M_SetupNextMenu(&KeysDef);
}

// ---------------------------------------------------------------- cheats
// each does what typing its code does (the code is sent to the game as typed, so vanilla's rules and words
// apply: none in Nightmare)
static int nw_warp_map = 1, nw_power;
static const char nw_power_codes[] = "vsiral";
static const char *const nw_power_names[] = {"INVULNERABLE", "BERSERK", "INVISIBLE", "RADIATION SUIT",
                                             "COMPUTER MAP", "LIGHT GOGGLES"};

static void M_NwType(const char *code)
{
    M_ClearMenus();
    for (; *code; code++)
    {
        event_t ev = {ev_keydown, *code, *code, 0, 0, 0};
        D_PostEvent(&ev);
        ev.type = ev_keyup;
        D_PostEvent(&ev);
    }
}

enum { ch_god, ch_kfa, ch_fa, ch_clip, ch_warp, ch_power, ch_map, ch_chainsaw, ch_type, ch_end };

static void M_NwCheatItem(int choice)
{
    if (gamestate != GS_LEVEL || demoplayback)
    {
        players[consoleplayer].message = "CHEATS ARE FOR A GAME";
        return;
    }
    char code[16];
    switch (choice)
    {
        case ch_god: M_NwType("iddqd"); break;
        case ch_kfa: M_NwType("idkfa"); break;
        case ch_fa: M_NwType("idfa"); break;
        case ch_clip: M_NwType("idclip"); break;
        case ch_warp:
            M_snprintf(code, sizeof code, "idclev%d%d", gamemode == commercial ? nw_warp_map / 10 : gameepisode,
                       nw_warp_map % 10);
            M_NwType(code);
            break;
        case ch_power:
            M_snprintf(code, sizeof code, "idbehold%c", nw_power_codes[nw_power]);
            M_NwType(code);
            break;
        case ch_map:
            M_ClearMenus();
            AM_NwCheat();
            break;
        case ch_chainsaw: M_NwType("idchoppers"); break;
        case ch_type:
            M_ClearMenus();
            nw_cheat_start();
            break;
    }
}

// (left and right choose the level and the power)
static boolean M_NwCheatSide(int right)
{
    int maps = gamemode == commercial ? 32 : 9;
    if (itemOn == ch_warp)
        nw_warp_map = (nw_warp_map + (right ? 1 : maps - 1) - 1) % maps + 1;
    else if (itemOn == ch_power)
        nw_power = (nw_power + (right ? 1 : 5)) % 6;
    else
        return false;
    return true;
}

static const menuitem_t CheatsMenu[] = {
    {1, VPATCH_NAME_INVALID, 'g', M_NwCheatItem}, {1, VPATCH_NAME_INVALID, 'k', M_NwCheatItem},
    {1, VPATCH_NAME_INVALID, 'f', M_NwCheatItem}, {1, VPATCH_NAME_INVALID, 'n', M_NwCheatItem},
    {1, VPATCH_NAME_INVALID, 'w', M_NwCheatItem}, {1, VPATCH_NAME_INVALID, 'p', M_NwCheatItem},
    {1, VPATCH_NAME_INVALID, 'm', M_NwCheatItem}, {1, VPATCH_NAME_INVALID, 'c', M_NwCheatItem},
    {1, VPATCH_NAME_INVALID, 't', M_NwCheatItem},
};

static void M_DrawNwCheats(void);
menu_t CheatsDef = {ch_end, &OptionsDef, CheatsMenu, M_DrawNwCheats, 36, 26, 0};

static void M_DrawNwCheats(void)
{
    char warp[16];
    player_t *p = &players[consoleplayer];
    boolean on = gamestate == GS_LEVEL;
    nw_value_x = 184;
    M_NwTitle("CHEATS (LEFT, RIGHT: THE LEVEL, THE POWER)");
    M_NwLine(&CheatsDef, ch_god, "GOD MODE (IDDQD)", on && (p->cheats & CF_GODMODE) ? "ON" : "OFF");
    M_NwLine(&CheatsDef, ch_kfa, "ALL WEAPONS, KEYS (IDKFA)", NULL);
    M_NwLine(&CheatsDef, ch_fa, "WEAPONS, AMMO (IDFA)", NULL);
    M_NwLine(&CheatsDef, ch_clip, "NO CLIPPING (IDCLIP)", on && (p->cheats & CF_NOCLIP) ? "ON" : "OFF");
    if (gamemode == commercial)
        M_snprintf(warp, sizeof warp, "< MAP%02d >", nw_warp_map);
    else
        M_snprintf(warp, sizeof warp, "< E%dM%d >", gameepisode ? gameepisode : 1, nw_warp_map);
    M_NwLine(&CheatsDef, ch_warp, "WARP (IDCLEV)", warp);
    M_NwLine(&CheatsDef, ch_power, "POWER (IDBEHOLD)", nw_power_names[nw_power]);
    M_NwLine(&CheatsDef, ch_map, "THE WHOLE MAP (IDDT)", NULL);
    M_NwLine(&CheatsDef, ch_chainsaw, "CHAINSAW (IDCHOPPERS)", NULL);
    M_NwLine(&CheatsDef, ch_type, "TYPE A CODE: IDMYPOS...", NULL);
}

static void M_NwCheats(int choice)
{
    if (gamestate == GS_LEVEL && gameepisode && gamemode != commercial)
        nw_warp_map = gamemap;
    M_SetupNextMenu(&CheatsDef);
}

// ---------------------------------------------------------------- more
enum { more_bright, more_overlay, more_secrets, more_sync, more_end };

// (the settings loaded: the screen size and detail they have)
void M_NwViewLoaded(void)
{
    screenSize = screenblocks - 3;
    R_SetViewSize(screenblocks, detailLevel);
}

static void M_NwMoreItem(int choice)
{
    switch (itemOn)
    {
        case more_bright: M_NwBrightness(choice); break;
        case more_overlay: nw_map_overlay = !nw_map_overlay; break;
        case more_secrets: nw_map_secrets = !nw_map_secrets; break;
        case more_sync:
        {
            // left to right, right to left, none
            int s = nw_sync_off ? 2 : nw_scan_right_to_left;
            s = (s + (choice == 0 ? 2 : 1)) % 3;
            nw_sync_off = s == 2;
            nw_scan_right_to_left = s == 1;
            break;
        }
    }
}

static const menuitem_t MoreMenu[] = {
    {2, VPATCH_NAME_INVALID, 'b', M_NwMoreItem},
    {2, VPATCH_NAME_INVALID, 'm', M_NwMoreItem},
    {2, VPATCH_NAME_INVALID, 's', M_NwMoreItem},
    {2, VPATCH_NAME_INVALID, 'r', M_NwMoreItem},
};

static void M_DrawNwMore(void);
menu_t MoreDef = {more_end, &OptionsDef, MoreMenu, M_DrawNwMore, 40, 40, 0};

static void M_DrawNwMore(void)
{
    static const char *const sync[] = {"LEFT TO RIGHT", "RIGHT TO LEFT", "NONE"};
    static const char *const levels[] = {"OFF", "1", "2", "3", "4"};
    nw_value_x = 150;
    M_NwTitle("MORE OPTIONS");
    M_NwLine(&MoreDef, more_bright, "BRIGHTNESS", levels[usegamma]);
    M_NwLine(&MoreDef, more_overlay, "MAP OVER THE VIEW", nw_map_overlay ? "ON" : "OFF");
    M_NwLine(&MoreDef, more_secrets, "SECRETS ON THE MAP", nw_map_secrets ? "PURPLE" : "OFF");
    M_NwLine(&MoreDef, more_sync, "SCREEN REFRESH", sync[nw_sync_off ? 2 : nw_scan_right_to_left]);
    static const char *const notes[] = {
        "THE MAP KEY GOES FROM THE VIEW TO THE MAP,", "TO THE MAP OVER THE VIEW, THEN BACK.", "",
        "THE PICTURE IS SENT AS THE CALCULATOR'S", "SCREEN REFRESHES: IF IT TEARS, TRY THE", "OTHER WAY ROUND.",
    };
    for (unsigned i = 0; i < sizeof notes / sizeof notes[0]; i++)
        M_WriteText(16, MoreDef.y + LINEHEIGHT * more_end + 8 + 10 * i, notes[i]);
}

static void M_NwMore(int choice)
{
    M_SetupNextMenu(&MoreDef);
}

// left and right in the calculator's menus that vanilla's have no use for
static boolean M_NwSide(int key)
{
    if (key != key_menu_left && key != key_menu_right)
        return false;
    if (currentMenu == &KeysDef)
    {
        nw_keys_page = (nw_keys_page + (key == key_menu_right ? 1 : nw_keys_pages - 1)) % nw_keys_pages;
        S_StartUnpositionedSound(sfx_pstop);
        return true;
    }
    if (currentMenu == &CheatsDef && M_NwCheatSide(key == key_menu_right))
    {
        S_StartUnpositionedSound(sfx_pstop);
        return true;
    }
    return false;
}
