/* The Pantheon (the temples' minigame, minigamePantheon.js): eleven spirits, three slots (diamond,
 * ruby, jade) where they are worshipped, the diamond strongest; worship swaps to move them, which come
 * back with time. Its panel shows in the buildings' view: the slots and their gems, the spirits in the
 * game's frames, the swaps; EXE picks a spirit up and sets it in a slot. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

typedef struct {
  const char *name, *before, *desc[3], *after, *quote;
  int16_t icon;
} God;
static const God gods[N_GODS] = {
  {"Holobore, Spirit of Asceticism", NULL, {"+15% base CpS.", "+10% base CpS.", "+5% base CpS."},
   "\x07If a golden cookie is clicked, this spirit is unslotted and all worship swaps will be used up.",
   "An immortal life spent focusing on the inner self, away from the distractions of material wealth.", ICON(21, 18)},
  {"Vomitrax, Spirit of Decadence", NULL,
   {"Golden and wrath cookie effect duration +7%. \x07" "Buildings grant -7% CpS.",
    "Golden and wrath cookie effect duration +5%. \x07" "Buildings grant -5% CpS.",
    "Golden and wrath cookie effect duration +2%. \x07" "Buildings grant -2% CpS."},
   NULL, "This sleazy spirit revels in the lust for quick easy gain and contempt for the value of steady work.",
   ICON(22, 18)},
  {"Godzamok, Spirit of Ruin", "Selling buildings triggers a buff boosted by how many buildings were sold.",
   {"Buff boosts clicks by +1% for every building sold for 10 seconds.",
    "Buff boosts clicks by +0.5% for every building sold for 10 seconds.",
    "Buff boosts clicks by +0.25% for every building sold for 10 seconds."},
   NULL, "The embodiment of natural disasters. An impenetrable motive drives the devastation caused by this spirit.",
   ICON(23, 18)},
  {"Cyclius, Spirit of Ages", "\x08" "CpS bonus fluctuating between +15% and -15% over time.",
   {"\x08" "Effect cycles over 3 hours.", "\x08" "Effect cycles over 12 hours.", "\x08" "Effect cycles over 24 hours."},
   NULL, "This spirit knows about everything you'll ever do, and enjoys dispensing a harsh judgment.", ICON(24, 18)},
  {"Selebrak, Spirit of Festivities", "Some seasonal effects are boosted.",
   {"Large boost. \x07Switching seasons is 100% pricier.", "Medium boost. \x07Switching seasons is 50% pricier.",
    "Small boost. \x07Switching seasons is 25% pricier."},
   NULL, "This is the spirit of merry getaways and regretful Monday mornings.", ICON(25, 18)},
  {"Dotjeiess, Spirit of Creation", NULL,
   {"All buildings are \x01" "7% cheaper\x02. \x07Heavenly chips have 30% less effect.",
    "All buildings are \x01" "5% cheaper\x02. \x07Heavenly chips have 20% less effect.",
    "All buildings are \x01" "2% cheaper\x02. \x07Heavenly chips have 10% less effect."},
   NULL, "All things that be and ever will be were scripted long ago by this spirit's inscrutable tendrils.",
   ICON(26, 18)},
  {"Muridal, Spirit of Labor", NULL,
   {"Clicking is \x01" "15%\x02 more powerful. \x07" "Buildings produce 3% less.",
    "Clicking is \x01" "10%\x02 more powerful. \x07" "Buildings produce 2% less.",
    "Clicking is \x01" "5%\x02 more powerful. \x07" "Buildings produce 1% less."},
   NULL, "This spirit enjoys a good cheese after a day of hard work.", ICON(27, 18)},
  {"Jeremy, Spirit of Industry", NULL,
   {"Buildings produce 10% more. \x07Golden and wrath cookies appear 10% less.",
    "Buildings produce 6% more. \x07Golden and wrath cookies appear 6% less.",
    "Buildings produce 3% more. \x07Golden and wrath cookies appear 3% less."},
   NULL, "While this spirit has many regrets, helping you rule the world through constant industrialization is not one "
   "of them.", ICON(28, 18)},
  {"Mokalsium, Mother Spirit", NULL,
   {"Milk is \x01" "10% more powerful\x02. \x07Golden and wrath cookies appear 15% less.",
    "Milk is \x01" "5% more powerful\x02. \x07Golden and wrath cookies appear 10% less.",
    "Milk is \x01" "3% more powerful\x02. \x07Golden and wrath cookies appear 5% less."},
   NULL, "A caring spirit said to contain itself, inwards infinitely.", ICON(29, 18)},
  {"Skruuia, Spirit of Scorn", "\x07" "All golden cookies are wrath cookies with a greater chance of a negative effect.",
   {"Wrinklers appear 150% faster and digest 15% more cookies.",
    "Wrinklers appear 100% faster and digest 10% more cookies.",
    "Wrinklers appear 50% faster and digest 5% more cookies."},
   NULL, "This spirit enjoys poking foul beasts and watching them squirm, but has no love for its own family.",
   ICON(21, 19)},
  {"Rigidel, Spirit of Order", NULL,
   {"Sugar lumps ripen \x01" "1 hour\x02 sooner.", "Sugar lumps ripen \x01" "40 minutes\x02 sooner.",
    "Sugar lumps ripen \x01" "20 minutes\x02 sooner."},
   "\x07" "Effect is only active when your total amount of buildings ends with 0.",
   "You will find that life gets just a little bit sweeter if you can motivate this spirit with tidy numbers and "
   "properly-filled tax returns.", ICON(22, 19)},
};
static const char *const slot_names[3] = {"Diamond slot", "Ruby slot", "Jade slot"};

/* ------------------------------------------------------------------ the rules */
static int32_t swap_wait(void) { return FPS * 60 * 60 * (S.swaps == 0 ? 16 : S.swaps == 1 ? 4 : 1); }

void pantheon_logic(void) {
  if (S.swaps < 3 && ++S.swap_t >= swap_wait()) S.swaps++, S.swap_t = 0;
}
void pantheon_reset(bool hard) {
  S.swaps = 3, S.swap_t = 0;
  memset(S.gods, 0, sizeof S.gods);
  (void)hard;
}
/* Holobore leaves its slot when a golden cookie is clicked */
bool force_unslot_god(int god) {
  for (int i = 0; i < 3; i++)
    if (S.gods[i] == god + 1 && hasgod(god)) {
      S.gods[i] = 0;
      G.recalculate = true;
      return true;
    }
  return false;
}
void use_swap(int32_t n) {
  S.swaps = (uint8_t)(S.swaps > n ? S.swaps - n : 0);
  S.swap_t = 0;
}
static int slot_of(int god) {
  for (int i = 0; i < 3; i++)
    if (S.gods[i] == god + 1) return i;
  return -1;
}
/* a spirit in a slot (or out of them, slot -1); the one there takes its place */
static void slot_god(int god, int slot) {
  int from = slot_of(god);
  if (slot == from) return;
  if (slot >= 0 && S.gods[slot]) {
    if (from >= 0) S.gods[from] = S.gods[slot];
  } else if (from >= 0)
    S.gods[from] = 0;
  if (slot >= 0) S.gods[slot] = (uint8_t)(god + 1);
  G.recalculate = true, G.store_dirty = true;
}

/* Cyclius' bonus now (the game's clock: here, the time played) */
double cyclius_mult(void) {
  int g = hasgod(GOD_AGES);
  if (!g) return 1;
  double hours = g == 1 ? 3 : g == 2 ? 12 : 24;
  return 1 + 0.15 * sin(S.play_time / 1000 / (60 * 60 * hours) * M_PI * 2);
}

/* ------------------------------------------------------------------ its panel */
#define PX STORE_X
#define PW STORE_W
#define GW 30
#define GH 37
#define RW (GW - 2)   /* (frames a pixel narrower each side than the game's: six fit in a row) */
static int sel;           /* 0..2 the slots, 3..13 the spirits */
static int held = -1;     /* the spirit picked up, -1 none */

static void slot_pos(int i, int *x, int *y) { *x = PX + PW / 2 - RW / 2 - 40 + i * 40, *y = NEWS_H + 42; }
static void god_pos(int g, int *x, int *y) {
  int row = g < 6 ? 0 : 1, n = row ? 5 : 6, k = row ? g - 6 : g;
  *x = PX + (PW - n * RW) / 2 + k * RW, *y = NEWS_H + 98 + row * (GH + 3);
}

static void god_frame(int g, int x, int y, bool chosen, int alpha) {
  int col = (g + 3) % 4;
  gfx_fill_alpha(x + 2, y + 2, RW, GH, BLACK, alpha / 2);
  gfx_image_part(IM_SPELL_BG, col * GW + 1, (chosen ? 1 : 0) * GH, RW, GH, x, y - chosen, alpha);
  gfx_icon(gods[g].icon, x + 2, y + 6 - chosen, alpha);
}
static void gem(int slot, int x, int y) {
  int im = icon_index_image(ICON(23, 15));
  if (im >= 0) gfx_image_part(im, slot == 1 ? 12 : 0, slot == 2 ? 12 : 0, 12, 12, x, y, 256);
}

void pantheon_draw(void) {
  gfx_clip(PX, NEWS_H, PW, SCREEN_H - NEWS_H);
  gfx_pattern(IM_MG_BG_PANTHEON, PX, NEWS_H, PW, SCREEN_H - NEWS_H, 192, 48, PX, NEWS_H, 256);
  gfx_image_scaled_fast(IM_BORDER_SOFT, PX, NEWS_H, PW, SCREEN_H - NEWS_H, 256);
  gfx_text_outline_center(F_KAVOON, PX + PW / 2, NEWS_H + 20, "Pantheon", WHITE, 256);
  for (int i = 0; i < 3; i++) {
    int x, y;
    slot_pos(i, &x, &y);
    gem(i, x + GW / 2 - 6, y - 14);
    gfx_round_rect(x - 1, y - 1, RW + 2, GH + 2, 4, BLACK, 150);
    if (S.gods[i]) god_frame(S.gods[i] - 1, x, y, sel == i, 256);
    if (sel == i) gfx_rect(x - 2, y - 2, RW + 4, GH + 4, held >= 0 ? HEX(0xffcc66) : WHITE, 256);
  }
  for (int g = 0; g < N_GODS; g++) {
    int x, y;
    god_pos(g, &x, &y);
    if (slot_of(g) >= 0 || g == held) {   /* (its place in the roster, empty) */
      gfx_round_rect(x + 1, y, RW - 2, GH, 4, BLACK, 70);
      if (g == held) god_frame(g, x, y - 6, true, 200);
    } else
      god_frame(g, x, y, sel == g + 3, 256);
    if (sel == g + 3) gfx_rect(x - 1, y - 2, RW + 2, GH + 4, WHITE, 256);
  }
  /* the worship swaps, the lump refill */
  char t[96], w[48];
  int y = NEWS_H + 190;
  if (S.swaps < 3) {
    say_time(swap_wait() - S.swap_t + FPS, -1, w);
    snprintf(t, sizeof t, "Worship swaps: %d/3 (next in %s)", S.swaps, w);
  } else
    snprintf(t, sizeof t, "Worship swaps: %d/3", S.swaps);
  gfx_text_outline_center(F_SMALL, PX + PW / 2, y, t, S.swaps ? WHITE : RGB(204, 0, 0), 256);
  gfx_text_center(F_SMALL, PX + PW / 2, SCREEN_H - 6, held >= 0 ? "EXE set in a slot   Back put back" : "EXE pick up   Back close",
                  RGB(160, 160, 160), 256);
  gfx_unclip();
}

static void god_tip(int g, int active, int x, int *yy, int w, bool draw) {
  const God *d = &gods[g];
  if (draw) {
    gfx_icon(d->icon, x, *yy, 256);
    gfx_para(&(ParaStyle){F_NAME, F_NAME, F_NAME, WHITE, WHITE, WHITE, 12, 0}, x + 27, *yy + 1, w - 27, d->name, true);
  }
  int nh = gfx_para(&(ParaStyle){F_NAME, F_NAME, F_NAME, WHITE, WHITE, WHITE, 12, 0}, x + 27, 0, w - 27, d->name, false);
  *yy += (nh > 24 ? nh : 24) + 2;
  if (draw) tip_line(x, *yy, w);
  *yy += 4;
  char l[300];
  /* (\x07 starts the part in red, \x08 is neutral; the rest is green) */
  for (int k = -1; k < 4; k++) {
    const char *s = k < 0 ? d->before : k < 3 ? d->desc[k] : d->after;
    if (!s) continue;
    if (k == 3 && g == GOD_AGES && hasgod(GOD_AGES)) continue;
    int lx = x + (k >= 0 && k < 3 ? 14 : 0), lw = w - (lx - x);
    const char *red = strchr(s, 0x07);
    ParaStyle st = PS_SMALL;
    st.color = s[0] == 0x08 ? RGB(204, 204, 204) : s[0] == 0x07 ? RGB(255, 102, 102) : RGB(102, 255, 102);
    if (k == active - 1 && draw) gfx_fill_alpha(x - 2, *yy - 1, w + 4, 2 + gfx_para(&st, lx, 0, lw, s, false), WHITE, 30);
    if (draw && k >= 0 && k < 3) gem(k, x, *yy + 1);
    if (red && red != s) {   /* (green, then red) */
      snprintf(l, sizeof l, "%.*s", (int)(red - s), s);
      int h = gfx_para(&st, lx, *yy, lw, l, draw);
      st.color = RGB(255, 102, 102);
      *yy += h;
      *yy += gfx_para(&st, lx, *yy, lw, red + 1, draw) + 2;
    } else
      *yy += gfx_para(&st, lx, *yy, lw, s[0] == 0x07 || s[0] == 0x08 ? s + 1 : s, draw) + 2;
  }
  if (g == GOD_AGES && hasgod(GOD_AGES)) {
    double m = cyclius_mult() - 1;
    char b[32];
    beautify(fabs(m) * 100, 2, b);
    snprintf(l, sizeof l, "Current bonus: %s%s%%", m < 0 ? "-" : "+", b);
    *yy += gfx_para(&PS_SMALL, x, *yy, w, l, draw) + 2;
  }
  if (g == GOD_ORDER && hasgod(GOD_ORDER)) {
    snprintf(l, sizeof l, "Buildings owned: %d\nEffect is %s.", G.buildings_owned,
             G.buildings_owned % 10 == 0 ? "active" : "inactive");
    *yy += gfx_para(&PS_SMALL, x, *yy, w, l, draw) + 2;
  }
  snprintf(l, sizeof l, CH_LQUOTE "%s" CH_RQUOTE, d->quote);
  ParaStyle q = PS_QUOTE;
  q.align = 1;
  *yy += gfx_para(&q, x + 4, *yy + 2, w - 8, l, draw) + 4;
}

int pantheon_tip(int x, int y, int w, bool draw) {
  int yy = y;
  if (sel < 3) {
    if (draw) {
      gem(sel, x + w / 2 - 6, yy);
      gfx_text_outline_center(F_NAME, x + w / 2, yy + 24, slot_names[sel], WHITE, 256);
    }
    yy += 28;
    if (!S.gods[sel]) {
      yy += gfx_para(&PS_SMALL, x, yy + 2, w, held >= 0 ? "Press EXE to set the spirit here." :
                     "Pick a spirit up (EXE) to set it in this slot.", draw) + 4;
      return yy - y;
    }
    if (draw) tip_line(x, yy, w);
    yy += 4;
    int slot = sel;
    if (aura_mult(AURA_SUPREME_INTELLECT) > 0 && slot > 0) slot--;
    god_tip(S.gods[sel] - 1, slot + 1, x, &yy, w, draw);
    return yy - y;
  }
  god_tip(sel - 3, 0, x, &yy, w, draw);
  return yy - y;
}

void pantheon_keys(uint32_t p) {
  U.tip_t = FPS * 6;
  if (p & K_LEFT) {
    if (sel > 0 && sel != 3 && sel != 9) sel--;
  }
  if (p & K_RIGHT) {
    if (sel < 2 || (sel >= 3 && sel < 13 && sel != 8)) sel++;
  }
  if (p & K_UP) sel = sel >= 9 ? sel - 6 : sel >= 3 ? (sel - 3) * 3 / 6 : sel;
  if (p & K_DOWN) sel = sel < 3 ? 3 + sel * 2 : sel < 9 ? (sel + 6 < 14 ? sel + 6 : 13) : sel;
  if (p & K_BACK && held >= 0) {
    held = -1;
    return;
  }
  if (!(p & K_EXE)) return;
  if (held >= 0) {
    if (sel < 3) {
      int from = slot_of(held);
      if (from == sel) {
        held = -1;
        return;
      }
      if (S.swaps == 0) {
        popup("No worship swaps left!", LEFT_W / 2, COOKIE_Y - 30);
        return;
      }
      use_swap(1);
      slot_god(held, sel);
    } else
      slot_god(held, -1);   /* (back to the roster: free) */
    held = -1;
    return;
  }
  if (sel < 3) {
    if (S.gods[sel]) held = S.gods[sel] - 1;
  } else if (slot_of(sel - 3) < 0)
    held = sel - 3, sel = 0;
}
