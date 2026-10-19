/* The HUD, where Rebirth has it: the active item and its charge top left,
 * the hearts beside it, coins, bombs and keys down the left, the map top
 * right, the pill or card bottom right, the boss's health along the bottom,
 * and the banners for floors and items. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "res.h"

int16_t banner_t;
static char banner_title[40], banner_sub[48];

void hud_banner(const char *title, const char *sub) {
  strncpy(banner_title, title, sizeof banner_title - 1);
  banner_title[sizeof banner_title - 1] = 0;
  strncpy(banner_sub, sub, sizeof banner_sub - 1);
  banner_sub[sizeof banner_sub - 1] = 0;
  banner_t = 75;
}

static void number(int x, int y, int n) {
  char s[4];
  s[0] = (char)('0' + n / 10 % 10), s[1] = (char)('0' + n % 10), s[2] = 0;
  gfx_text(FONT_UPHEAVAL, x, y, s, C_WHITE, 1, true);
}

static void hearts(int x0, int y0) {
  if (fl.curse & CURSE_UNKNOWN) {
    gfx_text(FONT_UPHEAVAL, x0 + 2, y0 + 1, "?", C_WHITE, 2, true);
    return;
  }
  int i = 0;
  for (int c = 0; c < pl.containers; c++, i++) {
    int h = pl.red - c * 2;
    int f = h >= 2 ? 0 : h == 1 ? 1 : 2;
    gfx_sprite(SPR_HUD_HEART, f, x0 + (i % 6) * 11, y0 + (i / 6) * 11, 0, 32);
  }
  for (int s = 0; s < (pl.soul + 1) / 2; s++, i++) {
    bool black = pl.black & (1 << s), half = s * 2 + 1 == pl.soul;
    int f = black ? (half ? 6 : 5) : (half ? 4 : 3);
    gfx_sprite(SPR_HUD_HEART, f, x0 + (i % 6) * 11, y0 + (i / 6) * 11, 0, 32);
  }
  if (pl.eternal) {   /* the eternal heart: a white one over the last red */
    int k = pl.containers ? pl.containers - 1 : 0;
    gfx_sprite_scaled(SPR_HUD_HEART, 7, x0 + (k % 6) * 11 + 2, y0 + (k / 6) * 11 + 2, 160, 160, 0, 0, 0, 32);
  }
}

/* ---------------------------------------------------------------- the map */
static void room_cell(int x, int y, int w, int h, Room *r, int i, bool big) {
  uint16_t c;
  if (i == fl.cur) c = C_WHITE;
  else if (r->state & RS_VISITED) c = rgb565(0x9A9A9A);
  else c = rgb565(0x4A4A4A);
  gfx_rect(x, y, w, h, C_BLACK, 32);
  gfx_rect(x + 1, y + 1, w - 2, h - 2, c, 32);
  int icon = -1;
  switch (r->kind) {
    case ROOM_BOSS: icon = 0; break;
    case ROOM_TREASURE: icon = 1; break;
    case ROOM_SHOP: icon = 2; break;
    case ROOM_SECRET: case ROOM_SUPERSECRET: icon = 3; break;
    case ROOM_CURSE: icon = 4; break;
    case ROOM_ARCADE: icon = 5; break;
    case ROOM_SACRIFICE: icon = 6; break;
    case ROOM_LIBRARY: icon = 7; break;
    case ROOM_MINIBOSS: icon = 8; break;
    case ROOM_CHALLENGE: icon = 9; break;
    default: break;
  }
  if (icon >= 0 && ((r->state & (RS_VISITED | RS_FOUND)) || pl.has[21])) {
    static const uint32_t col[10] = {0xC02020, 0xE0C030, 0x40A040, 0x606060, 0x902020, 0xD050A0, 0xA03030, 0x806040, 0xC06020, 0xA02020};
    gfx_sprite_tint(SPR_MAP_ICON, icon, x + w / 2, y + h / 2, 0, rgb565(col[icon]), 32, 32);
    (void)big;
  }
}

static bool shown(Room *r) {
  return r->state & (RS_SEEN | RS_VISITED | RS_FOUND);
}

static void minimap(void) {
  if (fl.curse & CURSE_LOST) return;
  int cw = 9, ch = 7, vx = 250, vy = 3, vw = 66, vh = 43;
  gfx_rect(vx - 1, vy - 1, vw + 2, vh + 2, C_BLACK, 14);
  Room *cr = room_cur();
  int cmx = fl.cur == fl.devil_room ? fl.rooms[fl.boss_room].mx : cr->mx, cmy = fl.cur == fl.devil_room ? fl.rooms[fl.boss_room].my : cr->my;
  gfx_clip(vx, vy, vw, vh);
  for (int i = 0; i < fl.nrooms; i++) {
    Room *r = &fl.rooms[i];
    if (i == fl.devil_room || !shown(r)) continue;
    int x = vx + vw / 2 - cw / 2 + (r->mx - cmx) * cw, y = vy + vh / 2 - ch / 2 + (r->my - cmy) * ch;
    room_cell(x, y, cw, ch, r, i, false);
  }
  gfx_clip_all();
}

void map_draw_full(void) {
  gfx_rect(0, 0, SCREEN_W, SCREEN_H, C_BLACK, 20);
  gfx_text_c(FONT_UPHEAVAL, 160, 8, stage_name(fl.stage, fl.alt), C_WHITE, 2, true);
  int cw = 22, ch = 17;
  int minx = 99, maxx = -1, miny = 99, maxy = -1;
  for (int i = 0; i < fl.nrooms; i++) {
    Room *r = &fl.rooms[i];
    if (i == fl.devil_room || !shown(r)) continue;
    if (r->mx < minx) minx = r->mx;
    if (r->mx > maxx) maxx = r->mx;
    if (r->my < miny) miny = r->my;
    if (r->my > maxy) maxy = r->my;
  }
  if (maxx < 0) return;
  int ox = 160 - (minx + maxx + 1) * cw / 2, oy = 128 - (miny + maxy + 1) * ch / 2;
  for (int i = 0; i < fl.nrooms; i++) {
    Room *r = &fl.rooms[i];
    if (i == fl.devil_room || !shown(r)) continue;
    room_cell(ox + r->mx * cw, oy + r->my * ch, cw - 2, ch - 2, r, i, true);
  }
}

/* ---------------------------------------------------------------- all of it */
void hud_draw(void) {
  int x = 4;
  /* the active item and its charge */
  if (pl.active) {
    const ItemInfo *ii = item_info(pl.active);
    gfx_sprite(item_sprite(pl.active), 0, 16, 26, 0, 32);
    if (ii && ii->charge && ii->charge < 254) {
      int h = 22, n = ii->charge;
      gfx_rect(29, 4, 6, h + 2, C_BLACK, 32);
      int full = pl.active_charge > n ? n : pl.active_charge;
      gfx_rect(30, 5 + h - h * full / n, 4, h * full / n, pl.active_charge >= n ? rgb565(0xF0E040) : rgb565(0x60C040), 32);
      if (pl.active_charge > n) gfx_rect(30, 5 + h - h * (pl.active_charge - n) / n, 4, h * (pl.active_charge - n) / n, rgb565(0x40A0F0), 32);
      for (int k = 1; k < n && n <= 6; k++) gfx_rect(30, 5 + h * k / n, 4, 1, C_BLACK, 32);
    }
    x = 40;
  }
  hearts(x, 4);
  /* coins, bombs, keys */
  int cy = 30;
  gfx_sprite(SPR_HUD_ICON, 0, 3, cy, 0, 32), number(14, cy, pl.coins);
  gfx_sprite(SPR_HUD_ICON, 1, 3, cy + 11, 0, 32), number(14, cy + 11, pl.bombs);
  gfx_sprite(SPR_HUD_ICON, 2, 3, cy + 22, 0, 32), number(14, cy + 22, pl.keys);
  if (fl.golden_key) gfx_rect(26, cy + 25, 3, 3, rgb565(0xF0C030), 32);
  if (fl.golden_bomb) gfx_rect(26, cy + 14, 3, 3, rgb565(0xF0C030), 32);
  minimap();
  /* the pill or card */
  if (pl.pocket_kind) {
    int spr = pl.pocket_kind == 1 ? SPR_PILL : SPR_CARD;
    gfx_sprite(spr, pl.pocket - 1, 306, 234, 0, 32);
  }
  /* the boss's health */
  float max, hp = bosses_hp(&max);
  if (max > 0 && hp > 0) {
    int w = 150, f = (int)(w * hp / max);
    gfx_rect(85, 226, w + 4, 9, C_BLACK, 26);
    gfx_rect(87, 228, f, 5, rgb565(0xC01818), 32);
    gfx_rect(87, 228, f, 1, rgb565(0xF06060), 32);
  }
  /* banners: an item's name and what it does, or the floor's */
  if (banner_t > 0) {
    int a = banner_t > 8 ? 32 : banner_t * 4;
    int y = 62;
    int w = gfx_text_width(FONT_UPHEAVAL, banner_title, 2);
    int ws = gfx_text_width(FONT_UPHEAVAL, banner_sub, 1);
    if (ws > w) w = ws;
    w += 24;
    gfx_rect(160 - w / 2, y - 4, w, banner_sub[0] ? 38 : 26, rgb565(0xE8D8B8), a * 7 / 8);
    gfx_rect(160 - w / 2, y - 4, w, 2, rgb565(0x6A4A30), a);
    gfx_rect(160 - w / 2, y + (banner_sub[0] ? 32 : 20), w, 2, rgb565(0x6A4A30), a);
    if (a > 16) {
      gfx_text_c(FONT_UPHEAVAL, 160, y, banner_title, rgb565(0x302018), 2, false);
      if (banner_sub[0]) gfx_text_c(FONT_UPHEAVAL, 160, y + 22, banner_sub, rgb565(0x5A4030), 1, false);
    }
  }
}
