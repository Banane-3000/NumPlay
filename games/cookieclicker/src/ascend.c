/* Ascending, as the game shows it: the big cookie shakes, bursts into crumbs in a flash of light;
 * then the heavens, stars and nebulae, with the heavenly upgrades' tree to buy from with heavenly
 * chips (half the game's size), the permanent upgrade slots, the challenge mode, and Reincarnate. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

#define MAP_SCALE 0.5f
#define ASCEND_DURATION (FPS * 5)
#define ASCEND_BREAKPOINT (ASCEND_DURATION / 2)
extern bool heavenly_can_be_purchased(int u);
extern bool heavenly_ghosted(int u);
extern int permanent_slot_of(int u);
extern bool can_be_permanent(int u);

/* ------------------------------------------------------------------ the crumbling cookie */
void draw_ascend_cookie(float cx, float cy) {
  float sc = 0.37f;
  float tb = (float)(G.ascend_timer - ASCEND_BREAKPOINT) / (ASCEND_DURATION - ASCEND_BREAKPOINT);
  if (tb < 0) tb = 0;
  float t = sqrtf(tb);
  float shake = G.ascend_timer < ASCEND_BREAKPOINT ? (float)G.ascend_timer / ASCEND_BREAKPOINT : 0;
  float x = cx + (float)(rnd() * 2 - 1) * 10 * shake * sc, y = cy + (float)(rnd() * 2 - 1) * 10 * shake * sc;
  gfx_fill(0, NEWS_H, LEFT_W, SCREEN_H - NEWS_H, BLACK);
  if (tb > 0) {
    gfx_blend(BLEND_ADD);
    float s = 0.5f + powf(tb, 0.6f), s2 = 600 * s * sc;
    gfx_image_rotated(IM_HEAVEN_RING1, x, y, s2, s2, G.real_t * 0.007f, (int)((1 - sqrtf(t)) * 256));
    s = 0.5f + powf(1 - tb, 0.4f), s2 = 600 * s * sc;
    gfx_image_rotated(IM_HEAVEN_RING2, x, y, s2, s2, G.real_t * 0.005f, (int)((1 - sqrtf(t)) * 256));
    gfx_blend(BLEND_NORMAL);
  }
  static const int chunks[10] = {7, 6, 3, 2, 8, 1, 9, 5, 0, 4};
  float s = 256 * (t / 2 + 1) * sc, rot = t * -0.1f * 2 * (float)M_PI;
  for (int i = 0; i < 10; i++) {
    float d = t * (80 + ((i + 2) % 3) * 40) * sc;
    float a = -(float)((chunks[i] + 4) % 10) / 10 * 2 * (float)M_PI;
    float lx = sinf(a) * d + (float)(rnd() * 2 - 1) * 5 * shake * sc, ly = cosf(a) * d + (float)(rnd() * 2 - 1) * 5 * shake * sc;
    float px = x + lx * cosf(rot) - ly * sinf(rot), py = y + lx * sinf(rot) + ly * cosf(rot);
    gfx_image_rotated(IM_BROKEN_COOKIE0 + chunks[i], px, py, s, s, rot, (int)((1 - t) * 256));
  }
  float halo = 1 - (t / (1.f / 3) < 1.f / 3 ? t / (1.f / 3) : 1.f / 3) * 3;
  if (G.ascend_timer < ASCEND_BREAKPOINT) halo = (float)G.ascend_timer / ASCEND_BREAKPOINT;
  gfx_image_rotated(IM_BROKEN_HALO, x, y, s * 1.5f, s * 1.5f, rot, (int)(halo * 256));
  /* the light's spokes before it bursts */
  if (G.ascend_timer < ASCEND_BREAKPOINT) {
    float k = (float)G.ascend_timer / ASCEND_BREAKPOINT;
    for (int i = 0; i < 9; i++) {
      float v = k / 3 * 9 * 3 + i * 2.7f;
      if ((int)floorf(v) % 2 == 0) continue;
      float t2 = powf(fmodf(v, 1), 1.5f);
      float sw = (1 - t2 * 0.5f) * 96 * sc, sh = (0.5f + t2 * 1.5f) * 96 * sc;
      float ang = i * 2 * (float)M_PI / 9, dist = (sh / 2 + 32 * sc + (1 - t2) * 256 * sc);
      gfx_image_rotated(IM_SHINE_SPOKE, x + sinf(ang) * dist, y - cosf(ang) * dist, sw, sh, ang,
                        (int)((1 - k) * (G.draw_t % 2 ? 128 : 256)));
    }
  }
  if (tb < 0.1f && tb > 0) gfx_fill_alpha(0, NEWS_H, LEFT_W, SCREEN_H - NEWS_H, WHITE, (int)((1 - tb / 0.1f) * 256));
  if (tb > 0.8f) gfx_fill_alpha(0, NEWS_H, LEFT_W, SCREEN_H - NEWS_H, BLACK, (int)((tb - 0.8f) / 0.2f * 256));
}

/* ------------------------------------------------------------------ the heavens */
static int visible[N_HEAVENLY], nvisible;
static bool ghost[N_HEAVENLY];
static void build(void) {
  nvisible = 0;
  for (int i = 0; i < N_HEAVENLY; i++) {
    int u = LIST(HEAVENLY)[i];
    bool can = heavenly_can_be_purchased(u), gh = !can && heavenly_ghosted(u);
    if (can || gh) ghost[nvisible] = gh, visible[nvisible++] = u;
  }
}
static float map_x(int u) { return (upgrade_rec(u)->posx + 24) * MAP_SCALE; }
static float map_y(int u) { return (upgrade_rec(u)->posy + 24) * MAP_SCALE; }
static int sel_u = -1;
static double chips_shown;

static void focus(void) {
  if (sel_u < 0) return;
  U.asc_tx = SCREEN_W / 2 - map_x(sel_u), U.asc_ty = (SCREEN_H + 28) / 2 - map_y(sel_u);
}

void ascend_enter(void) {
  build();
  sel_u = U_LEGACY;
  if (!bit_get(S.upgrades_bought, sel_u) && nvisible) sel_u = visible[0];
  focus();
  U.asc_x = U.asc_tx, U.asc_y = U.asc_ty;
  chips_shown = S.heavenly_chips;
  U.asc_mode = 0;
}

static void draw_link(int a, int b, int alpha) {
  float x0 = map_x(a) + U.asc_x, y0 = map_y(a) + U.asc_y, x1 = map_x(b) + U.asc_x, y1 = map_y(b) + U.asc_y;
  int minx = (int)fminf(x0, x1), miny = (int)fminf(y0, y1);
  if (!gfx_visible(minx - 2, miny - 2, (int)fabsf(x1 - x0) + 4, (int)fabsf(y1 - y0) + 4)) return;
  /* the pulse along the link */
  gfx_line(x0, y0, x1, y1, HEX(0xbfe9ff), alpha / 2);
  float k = fmodf(G.real_t * 0.03f, 1);
  gfx_disc(x0 + (x1 - x0) * k, y0 + (y1 - y0) * k, 1.5f, WHITE, alpha);
}

static void draw_heavens(void) {
  gfx_fill(0, 0, SCREEN_W, SCREEN_H, BLACK);
  float s = 1 + cosf(G.real_t * 0.0027f) * 0.05f;
  gfx_pattern(IM_STARS, 0, 0, SCREEN_W, SCREEN_H, 384 * s, 384 * s, SCREEN_W / 2 + U.asc_x * 0.25f * s,
              SCREEN_H / 2 + U.asc_y * 0.25f * s, 128);
  if (S.prefs.fancy) {
    s = 2 * (1 + sinf(G.real_t * 0.002f) * 0.07f);
    gfx_pattern(IM_STARS, 0, 0, SCREEN_W, SCREEN_H, 384 * s, 384 * s, SCREEN_W / 2 + U.asc_x * 0.25f * s,
                SCREEN_H / 2 + U.asc_y * 0.25f * s, (int)(128 * (0.5f + sinf(G.real_t * 0.02f) * 0.3f)));
    gfx_blend(BLEND_ADD);
    float cx = U.asc_x, cy = U.asc_y;
    float r1 = (600 + 150 * sinf(G.real_t * 0.007f)) * MAP_SCALE, r2 = (600 + 150 * sinf(G.real_t * 0.0037f)) * MAP_SCALE;
    gfx_image_rotated(IM_HEAVEN_RING1, cx, cy, r1, r1, G.real_t * 0.001f, 128);
    gfx_image_rotated(IM_HEAVEN_RING2, cx, cy, r2, r2, -G.real_t * 0.0007f, 128);
    gfx_blend(BLEND_NORMAL);
  }
  /* the links, then the upgrades */
  for (int i = 0; i < nvisible; i++) {
    const UpgradeRec *r = upgrade_rec(visible[i]);
    for (int k = 0; k < 6 && r->parents[k] != NONE; k++) draw_link(r->parents[k], visible[i], ghost[i] ? 26 : 128);
  }
  for (int i = 0; i < nvisible; i++) {
    int u = visible[i];
    int x = (int)(map_x(u) + U.asc_x), y = (int)(map_y(u) + U.asc_y);
    if (!gfx_visible(x - 20, y - 20, 40, 40)) continue;
    if (ghost[i]) {
      gfx_icon(upgrade_rec(u)->icon, x - 12, y - 12, 51);
      continue;
    }
    bool bought = bit_get(S.upgrades_bought, u);
    int frame = u == sel_u ? 1 : bought ? 0 : 2;
    gfx_image(IM_ASCEND_SLOT0 + frame, x - 20, y - 20, 256);
    gfx_icon(upgrade_rec(u)->icon, x - 12, y - 12 - (u == sel_u), bought ? 256 : 205);
    int slot = permanent_slot_of(u);
    if (slot >= 0 && bought && S.permanent[slot]) gfx_icon_scaled(upgrade_rec(S.permanent[slot] - 1)->icon, x + 2, y + 2, 12, 256);
  }
  gfx_image_scaled_fast(IM_BORDER_SOFT, 0, 0, SCREEN_W, SCREEN_H, 256);
  /* the top: prestige, the chips, Reincarnate */
  char a[48], t[64];
  beautify(S.prestige, 0, a);
  gfx_text_outline(F_SMALL, 6, 11, "Prestige level:", WHITE, 256);
  gfx_text_outline(F_NAME, 6, 24, a, WHITE, 256);
  beautify(round(chips_shown), 0, a);
  gfx_text_outline_center(F_SMALL, SCREEN_W - 50, 11, "Heavenly chips:", WHITE, 256);
  gfx_image(IM_HEAVENLY_MONEY, SCREEN_W - 50 - gfx_text_width(F_NAME, a) / 2 - 12, 15, 256);
  gfx_text_outline_center(F_NAME, SCREEN_W - 50 + 6, 24, a, HEX(0x99dfff), 256);
  gfx_round_rect(SCREEN_W / 2 - 46, 3, 92, 18, 9, BLACK, 128);
  gfx_text_outline_center(F_NAME, SCREEN_W / 2, 16, "Reincarnate", WHITE, 256);
  if (S.prestige > 0) {
    snprintf(t, sizeof t, "Challenge mode: %s", U.asc_mode == 1 ? "Born again" : "None");
    gfx_text_outline_center(F_SMALL, SCREEN_W / 2, 32, t, U.asc_mode ? HEX(0xffcc66) : RGB(200, 200, 200), 256);
  }
  gfx_text_outline_center(F_SMALL, SCREEN_W / 2, SCREEN_H - 5,
                          S.prestige > 0 ? "EXE buy   Back reincarnate   shift challenge mode" : "EXE buy   Back reincarnate",
                          RGB(170, 170, 170), 256);
  /* the tooltip of the upgrade chosen */
  if (sel_u >= 0) {
    int w = 200, pad = 5;
    int h = crate_tip(0, sel_u, CTX_ASCEND, 0, 0, w - 2 * pad, false) + 2 * pad;
    int slot = permanent_slot_of(sel_u);
    char extra[96] = "";
    if (slot >= 0 && bit_get(S.upgrades_bought, sel_u)) {
      snprintf(extra, sizeof extra, "Press EXE to choose an upgrade for this slot (%s).",
               S.permanent[slot] ? res_text(upgrade_rec(S.permanent[slot] - 1)->name) : "empty");
      h += gfx_para(&PS_SMALL, 0, 0, w - 2 * pad, extra, false) + 4;
    }
    int x = (int)(map_x(sel_u) + U.asc_x), y = (int)(map_y(sel_u) + U.asc_y);
    int ty = y + 22;
    if (ty + h > SCREEN_H - 12) ty = y - 22 - h;
    if (ty < 36) ty = 36;
    int tx = x - w / 2;
    if (tx < 4) tx = 4;
    if (tx + w > SCREEN_W - 4) tx = SCREEN_W - 4 - w;
    if (gfx_visible(tx, ty, w, h)) {
      draw_frame_box(tx, ty, w, h);
      int hh = crate_tip(0, sel_u, CTX_ASCEND, tx + pad, ty + pad, w - 2 * pad, true);
      if (extra[0]) gfx_para(&PS_SMALL, tx + pad, ty + pad + hh + 2, w - 2 * pad, extra, true);
    }
  }
}

/* ------------------------------------------------------------------ the permanent upgrade slots */
static uint16_t cands[N_UPGRADES];
static int ncands;
static void cands_build(void) {
  ncands = 0;
  for (int u = 0; u < N_UPGRADES; u++)
    if (can_be_permanent(u)) cands[ncands++] = (uint16_t)u;
}
#define PCOLS 10
static void draw_permanent(void) {
  cands_build();
  int w = 300, x = (SCREEN_W - w) / 2, top = 30;
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, BLACK, 140);
  draw_frame_box(x, top, w, SCREEN_H - top - 8);
  int slot = U.perm_slot;
  char t[96];
  snprintf(t, sizeof t, "Permanent upgrade slot %s", (const char *[]){"I", "II", "III", "IV", "V"}[slot]);
  gfx_text_outline_center(F_TITLE, SCREEN_W / 2, top + 16, t, WHITE, 256);
  ParaStyle st = PS_SMALL;
  st.align = 1;
  int y = top + 20;
  y += gfx_para(&st, x + 8, y, w - 16,
                "Choose an upgrade you bought in your last run: it will be yours from the start of every run.", true);
  if (!ncands) {
    gfx_para(&st, x + 8, y + 20, w - 16, "You don't have any upgrades that can be made permanent.", true);
    return;
  }
  int gx = x + (w - PCOLS * CRATE) / 2, rows_shown = (SCREEN_H - 8 - y - 60) / CRATE;
  int row_sel = U.perm_sel / PCOLS, first = row_sel - rows_shown + 1 > 0 ? row_sel - rows_shown + 1 : 0;
  for (int i = first * PCOLS; i < ncands && i < (first + rows_shown) * PCOLS; i++) {
    int cx = gx + (i % PCOLS) * CRATE, cy = y + 4 + (i / PCOLS - first) * CRATE;
    bool cur = S.permanent[slot] == cands[i] + 1;
    draw_crate(upgrade_rec(cands[i])->icon, cx, cy, U.perm_sel == i ? 1 : cur ? 0 : 0, false);
    if (cur) gfx_rect(cx + 1, cy + 1, CRATE - 2, CRATE - 2, HEX(0xffcc66), 256);
  }
  int sel = U.perm_sel < ncands ? cands[U.perm_sel] : -1;
  if (sel >= 0) {
    int ty = y + 8 + rows_shown * CRATE;
    tip_line(x + 8, ty, w - 16);
    gfx_text_outline_center(F_NAME, SCREEN_W / 2, ty + 14, res_text(upgrade_rec(sel)->name), WHITE, 256);
    gfx_text_center(F_SMALL, SCREEN_W / 2, ty + 28, "EXE choose   Back cancel", RGB(170, 170, 170), 256);
  }
}
static void permanent_keys(uint32_t p) {
  cands_build();
  if (p & K_BACK) {
    U.overlay = OV_NONE;
    return;
  }
  if (!ncands) return;
  if (p & K_LEFT && U.perm_sel > 0) U.perm_sel--;
  if (p & K_RIGHT && U.perm_sel < ncands - 1) U.perm_sel++;
  if (p & K_UP && U.perm_sel >= PCOLS) U.perm_sel -= PCOLS;
  if (p & K_DOWN && U.perm_sel + PCOLS < ncands) U.perm_sel += PCOLS;
  if (p & (K_OK | K_EXE)) {
    S.permanent[U.perm_slot] = (uint16_t)(cands[U.perm_sel] + 1);
    U.overlay = OV_NONE;
  }
}

/* ------------------------------------------------------------------ the frames and keys */
void draw_ascend(void) {
  draw_heavens();
  if (U.overlay == OV_PERMANENT) draw_permanent();
}

void ascend_tick(void) {
  U.asc_x += (U.asc_tx - U.asc_x) * 0.5f;
  U.asc_y += (U.asc_ty - U.asc_y) * 0.5f;
  chips_shown += (S.heavenly_chips - chips_shown) * 0.4;
}

void ascend_keys(uint32_t p) {
  if (U.overlay == OV_PERMANENT) {
    permanent_keys(p);
    return;
  }
  build();
  if (sel_u < 0 && nvisible) sel_u = visible[0];
  if (p & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) {
    float dx = p & K_LEFT ? -1 : p & K_RIGHT ? 1 : 0, dy = p & K_UP ? -1 : p & K_DOWN ? 1 : 0;
    float x0 = map_x(sel_u), y0 = map_y(sel_u), best = 1e9f;
    int found = -1;
    for (int i = 0; i < nvisible; i++) {
      int u = visible[i];
      if (u == sel_u || ghost[i]) continue;
      float vx = map_x(u) - x0, vy = map_y(u) - y0, d = sqrtf(vx * vx + vy * vy);
      float along = (vx * dx + vy * dy) / d;
      if (along < 0.5f) continue;   /* (within 60 degrees of the arrow) */
      float score = d * (2 - along);
      if (score < best) best = score, found = u;
    }
    if (found >= 0) sel_u = found, focus();
  }
  if (p & K_SHIFT && S.prestige > 0) U.asc_mode ^= 1;
  if (p & (K_EXE | K_OK) && sel_u >= 0) {
    if (!bit_get(S.upgrades_bought, sel_u)) {
      if (heavenly_can_be_purchased(sel_u) && upgrade_buy(sel_u, true)) build();
    } else if (permanent_slot_of(sel_u) >= 0) {
      U.perm_slot = permanent_slot_of(sel_u), U.perm_sel = 0, U.overlay = OV_PERMANENT;
    }
  }
  if (p & K_BACK) prompt_open(PROMPT_REINCARNATE, U.asc_mode);
}
