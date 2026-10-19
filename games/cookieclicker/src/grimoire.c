/* The Grimoire (the wizard towers' minigame, minigameGrimoire.js): magic that refills by itself, the
 * slower the emptier it is, and nine spells to spend it on; each may backfire. Its panel shows in
 * the buildings' view: the spells in the game's frames, the magic meter, the spells cast. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

enum { SP_CONJURE, SP_FATE, SP_STRETCH, SP_EDIFICE, SP_HAGGLER, SP_PIXIES, SP_GAMBLER, SP_RESURRECT, SP_DIMINISH, N_SPELLS };
typedef struct {
  const char *name, *desc, *fail_desc;
  int16_t icon;
  uint8_t cost_min;
  float cost_percent;
} Spell;
static const Spell spells[N_SPELLS] = {
  {"Conjure Baked Goods", "Summon half an hour worth of your CpS, capped at 15% of your cookies owned.",
   "Trigger a 15-minute clot and lose 15 minutes of CpS.", ICON(21, 11), 2, 0.4f},
  {"Force the Hand of Fate",
   "Summon a random golden cookie. Each existing golden cookie makes this spell +15% more likely to backfire.",
   "Summon an unlucky wrath cookie.", ICON(22, 11), 10, 0.6f},
  {"Stretch Time", "All active buffs gain 10% more time (up to 5 more minutes).",
   "All active buffs are shortened by 20% (up to 10 minutes shorter).", ICON(23, 11), 8, 0.2f},
  {"Spontaneous Edifice",
   "The spell picks a random building you could afford if you had twice your current cookies, and gives it to you "
   "for free. The building selected must be under 400, and cannot be your most-built one (unless it is your only "
   "one).",
   "Lose a random building.", ICON(24, 11), 20, 0.75f},
  {"Haggler's Charm", "Upgrades are 2% cheaper for 1 minute.", "Upgrades are 2% more expensive for an hour.\x03What's "
   "that spell? Loadsamoney!\x04", ICON(25, 11), 10, 0.1f},
  {"Summon Crafty Pixies", "Buildings are 2% cheaper for 1 minute.", "Buildings are 2% more expensive for an hour.",
   ICON(26, 11), 10, 0.2f},
  {"Gambler's Fever Dream", "Cast a random spell at half the magic cost, with twice the chance of backfiring.", NULL,
   ICON(27, 11), 3, 0.05f},
  {"Resurrect Abomination", "Instantly summon a wrinkler if conditions are fulfilled.", "Pop one of your wrinklers.",
   ICON(28, 11), 20, 0.1f},
  {"Diminish Ineptitude", "Spells backfire 10 times less for the next 5 minutes.",
   "Spells backfire 5 times more for the next 10 minutes.", ICON(29, 11), 5, 0.2f},
};

static float magic_max, magic_ps;
static int sel;                      /* the spell chosen; N_SPELLS: the lump refill */
static int pending = -1, pending_t;  /* the Gambler's Fever Dream: the spell it casts in a second */
static float pending_cost, pending_self;
extern bool refill_lump(int n);
extern bool can_refill_lump(void);
extern bool can_lumps(void);
extern bool spawn_wrinkler(void);
extern bool pop_random_wrinkler(void);

static void compute_magic_max(void) {
  const Building *t = &S.buildings[B_WIZARD_TOWER];
  double towers = t->amount > 1 ? t->amount : 1, lvl = t->level > 1 ? t->level : 1;
  magic_max = (float)floor(4 + pow(towers, 0.6) + log((towers + (lvl - 1) * 10) / 15 + 1) * 15);
  if (S.grimoire_magic > magic_max) S.grimoire_magic = magic_max;
}

static int spell_cost(int s) {
  double out = spells[s].cost_min + magic_max * spells[s].cost_percent;
  out *= 1 - 0.1 * aura_mult(AURA_SUPREME_INTELLECT);
  return (int)floor(out);
}

static double fail_chance(int s) {
  double f = 0.15;
  if (has_buff(BUFF_MAGIC_ADEPT)) f *= 0.1;
  if (has_buff(BUFF_MAGIC_INEPT)) f *= 5;
  f *= 1 + 0.1 * aura_mult(AURA_SUPREME_INTELLECT);
  if (s == SP_FATE) f += 0.15 * G.nshimmers_golden;
  return f;
}

static void say(const char *s) { popup(s, LEFT_W / 2, COOKIE_Y - 30); }

static Shimmer *golden(int force, int wrath) {
  Shimmer *m = shimmer_new(SHIMMER_GOLDEN, force, false);
  if (m) m->wrath = (uint8_t)wrath;
  return m;
}

/* a spell's effect: true if it took (false: nothing to do, no magic spent) */
static bool spell_win(int s) {
  char t[96], c[48];
  switch (s) {
  case SP_CONJURE: {
    double v = S.cookies * 0.15 < G.cookies_ps * 60 * 30 ? S.cookies * 0.15 : G.cookies_ps * 60 * 30;
    if (v < 7) v = 7;
    earn(v);
    cookies_str(v, 0, c);
    snprintf(t, sizeof t, "You magic \x01%s\x02 out of thin air.", c);
    notify("Conjure Baked Goods!", t, ICON(21, 11), 6);
    snprintf(t, sizeof t, "+%s!", c);
    say(t);
    return true;
  }
  case SP_FATE: {
    int choices[8], n = 0;
    choices[n++] = GC_FRENZY, choices[n++] = GC_LUCKY;
    if (!has_buff(BUFF_DRAGONFLIGHT)) choices[n++] = GC_CLICK_FRENZY;
    if (rnd() < 0.1) choices[n++] = GC_STORM, choices[n++] = GC_STORM, choices[n++] = GC_BLAB;
    if (G.buildings_owned >= 10 && rnd() < 0.25) choices[n++] = GC_BUILDING_SPECIAL;
    if (rnd() < 0.15) n = 0, choices[n++] = GC_STORM_DROP;
    if (rnd() < 0.0001) choices[n++] = GC_SUGAR_LUMP;
    Shimmer *m = golden(choices[rnd_int(n)], 0);
    if (m && m->force == GC_STORM_DROP) m->size_mult = (float)(rnd() * 0.75 + 0.25);
    say("Promising fate!");
    return true;
  }
  case SP_STRETCH: {
    int changed = 0;
    for (int i = 0; i < MAX_BUFFS; i++) {
      Buff *b = &S.buffs[i];
      if (!b->used) continue;
      int32_t gain = b->max_time / 10 < FPS * 60 * 5 ? b->max_time / 10 : FPS * 60 * 5;
      b->max_time += gain, b->time += gain, changed++;
    }
    if (!changed) {
      say("No buffs to alter!");
      return false;
    }
    say("Zap! Buffs lengthened.");
    return true;
  }
  case SP_EDIFICE: {
    int max = 0, owned = 0, cand[NB], n = 0;
    for (int b = 0; b < NB; b++) {
      if (S.buildings[b].amount > max) max = S.buildings[b].amount;
      if (S.buildings[b].amount > 0) owned++;
    }
    for (int b = 0; b < NB; b++)
      if ((S.buildings[b].amount < max || owned == 1) && building_price(b) <= S.cookies * 2 && S.buildings[b].amount < 400)
        cand[n++] = b;
    if (!n) {
      say("No buildings to improve!");
      return false;
    }
    int b = cand[rnd_int(n)];
    Building *m = &S.buildings[b];
    m->amount++, m->bought++, G.buildings_owned++;
    if (m->amount > m->highest) m->highest = m->amount;
    extern void building_bought(int b);
    building_bought(b);
    G.recalculate = true, G.store_dirty = true;
    snprintf(t, sizeof t, "A new %s\nbursts out of the ground.", res_text(building_rec(b)->single));
    say(t);
    return true;
  }
  case SP_HAGGLER:
    kill_buff(BUFF_HAGGLER_MISERY);
    gain_buff(BUFF_HAGGLER_LUCK, 60, 2, 0);
    say("Upgrades are cheaper!");
    return true;
  case SP_PIXIES:
    kill_buff(BUFF_PIXIE_MISERY);
    gain_buff(BUFF_PIXIE_LUCK, 60, 2, 0);
    say("Crafty pixies\nBuildings are cheaper!");
    return true;
  case SP_GAMBLER: {
    int self = spell_cost(SP_GAMBLER), cand[N_SPELLS], n = 0;
    for (int k = 0; k < N_SPELLS; k++)
      if (k != SP_GAMBLER && S.grimoire_magic - self >= spell_cost(k) * 0.5f) cand[n++] = k;
    if (!n) {
      say("No eligible spells!");
      return false;
    }
    pending = cand[rnd_int(n)];
    pending_cost = spell_cost(pending) * 0.5f, pending_self = (float)self, pending_t = FPS;
    char cs[32];
    beautify(pending_cost, 0, cs);
    snprintf(t, sizeof t, "Casting %s\nfor %s magic...", spells[pending].name, cs);
    say(t);
    return true;
  }
  case SP_RESURRECT:
    if (!spawn_wrinkler()) {
      say("Unable to spawn a wrinkler!");
      return false;
    }
    say("Rise, my precious!");
    return true;
  case SP_DIMINISH:
    kill_buff(BUFF_MAGIC_INEPT);
    gain_buff(BUFF_MAGIC_ADEPT, 5 * 60, 10, 0);
    say("Ineptitude diminished!");
    return true;
  }
  return false;
}

static bool spell_fail(int s) {
  char t[96], c[48];
  switch (s) {
  case SP_CONJURE: {
    Buff *b = gain_buff(BUFF_CLOT, 60 * 15, 0.5, 0);
    double v = (S.cookies * 0.15 < G.cookies_ps * 60 * 15 ? S.cookies * 0.15 : G.cookies_ps * 60 * 15) + 13;
    if (v > S.cookies) v = S.cookies;
    spend(v);
    extern void buff_desc(const Buff *b, char *out, int size);
    char d[96];
    buff_desc(b, d, sizeof d);
    notify("Clot", d, ICON(15, 5), 6);
    cookies_str(v, 0, c);
    snprintf(t, sizeof t, "Backfire!\nSummoning failed! Lost %s!", c);
    say(t);
    return true;
  }
  case SP_FATE: {
    int choices[6], n = 0;
    choices[n++] = GC_CLOT, choices[n++] = GC_RUIN;
    if (rnd() < 0.1) choices[n++] = GC_CURSED_FINGER, choices[n++] = GC_BLOOD_FRENZY;
    if (rnd() < 0.003) choices[n++] = GC_SUGAR_LUMP;
    if (rnd() < 0.1) n = 0, choices[n++] = GC_BLAB;
    golden(choices[rnd_int(n)], 1);
    say("Backfire!\nSinister fate!");
    return true;
  }
  case SP_STRETCH: {
    int changed = 0;
    for (int i = 0; i < MAX_BUFFS; i++) {
      Buff *b = &S.buffs[i];
      if (!b->used) continue;
      int32_t loss = b->time / 5 < FPS * 60 * 10 ? b->time / 5 : FPS * 60 * 10;
      b->time -= loss;
      if (b->time < 0) b->time = 0;
      changed++;
    }
    if (!changed) {
      say("No buffs to alter!");
      return false;
    }
    say("Backfire!\nFizz! Buffs shortened.");
    return true;
  }
  case SP_EDIFICE: {
    if (G.buildings_owned == 0) {
      say("Backfired, but no buildings to destroy!");
      return false;
    }
    int cand[NB], n = 0;
    for (int b = 0; b < NB; b++)
      if (S.buildings[b].amount > 0) cand[n++] = b;
    int b = cand[rnd_int(n)];
    S.buildings[b].amount--, G.buildings_owned--;
    G.recalculate = true, G.store_dirty = true;
    snprintf(t, sizeof t, "Backfire!\nOne of your %s\ndisappears in a puff of smoke.", res_text(building_rec(b)->plural));
    say(t);
    return true;
  }
  case SP_HAGGLER:
    kill_buff(BUFF_HAGGLER_LUCK);
    gain_buff(BUFF_HAGGLER_MISERY, 60 * 60, 2, 0);
    say("Backfire!\nUpgrades are pricier!");
    return true;
  case SP_PIXIES:
    kill_buff(BUFF_PIXIE_LUCK);
    gain_buff(BUFF_PIXIE_MISERY, 60 * 60, 2, 0);
    say("Backfire!\nNasty goblins\nBuildings are pricier!");
    return true;
  case SP_RESURRECT:
    if (!pop_random_wrinkler()) {
      say("Backfire!\nBut no wrinkler was harmed.");
      return false;
    }
    say("Backfire!\nSo long, ugly...");
    return true;
  case SP_DIMINISH:
    kill_buff(BUFF_MAGIC_ADEPT);
    gain_buff(BUFF_MAGIC_INEPT, 10 * 60, 5, 0);
    say("Backfire!\nIneptitude magnified!");
    return true;
  }
  return false;
}

/* casts a spell (cost < 0: its usual cost); false if there isn't the magic, or it did nothing */
static bool cast(int s, float cost, double fail_max, bool passthrough) {
  if (cost < 0) cost = (float)spell_cost(s);
  if (S.grimoire_magic < cost) return false;
  double fail = fail_chance(s);
  if (fail_max > 0 && fail < fail_max) fail = fail_max;
  /* (the game seeds its random with the spells cast: the same cast has the same outcome) */
  uint32_t keep = G.seed;
  G.seed = 0x9E3779B9u * (S.spells_cast_total + 1) ^ (uint32_t)S.bakery_name1 << 8 ^ S.bakery_name2 ^ 0x5bd1e995u;
  if (!G.seed) G.seed = 1;
  bool failed = spells[s].fail_desc && rnd() >= 1 - fail;
  bool out = failed ? spell_fail(s) : spell_win(s);
  G.seed = keep;
  if (!out) return false;
  if (!passthrough) {
    S.spells_cast++, S.spells_cast_total++;
    if (S.spells_cast_total >= 9) win(A_BIBBIDI_BOBBIDI_BOO);
    if (S.spells_cast_total >= 99) win(A_IM_THE_WIZ);
    if (S.spells_cast_total >= 999) win(A_A_WIZARD_IS_YOU);
  }
  S.grimoire_magic -= cost;
  if (S.grimoire_magic < 0) S.grimoire_magic = 0;
  return true;
}

/* ------------------------------------------------------------------ the frames */
void grimoire_logic(void) {
  if (S.buildings[B_WIZARD_TOWER].level < 1) return;
  if (G.T % 5 == 0 || magic_max == 0) compute_magic_max();
  double m = S.grimoire_magic / (magic_max > 100 ? magic_max : 100);
  magic_ps = (float)((sqrt(m) > 0.002 ? sqrt(m) : 0.002) * 0.002);
  S.grimoire_magic += magic_ps;
  if (S.grimoire_magic > magic_max) S.grimoire_magic = magic_max;
  if (pending >= 0 && --pending_t <= 0) {
    int s = pending;
    pending = -1;
    if (!cast(s, pending_cost, 0.5, true)) {
      S.grimoire_magic += pending_self;
      say("That's too bad!\nMagic refunded.");
    }
  }
}
void grimoire_reset(bool hard) {
  compute_magic_max();
  S.grimoire_magic = magic_max;
  S.spells_cast = 0;
  if (hard) S.spells_cast_total = 0;
  pending = -1;
}
/* its first level: the meter starts full */
void grimoire_on_level(void) {
  compute_magic_max();
  if (S.buildings[B_WIZARD_TOWER].level == 1) S.grimoire_magic = magic_max;
}

/* ------------------------------------------------------------------ its panel */
#define GX STORE_X
#define GW STORE_W
#define SPELL_W 30
#define SPELL_H 37
static void spell_pos(int s, int *x, int *y) {
  int row = s < 5 ? 0 : 1, n = row ? 4 : 5, k = row ? s - 5 : s;
  int gap = 3, total = n * SPELL_W + (n - 1) * gap;
  *x = GX + (GW - total) / 2 + k * (SPELL_W + gap);
  *y = NEWS_H + 28 + row * (SPELL_H + 4);
}

void grimoire_draw(void) {
  gfx_clip(GX, NEWS_H, GW, SCREEN_H - NEWS_H);
  gfx_pattern(IM_MG_BG_GRIMOIRE, GX, NEWS_H, GW, SCREEN_H - NEWS_H, 192, 48, GX, NEWS_H, 256);
  gfx_image_scaled(IM_BORDER_SOFT, GX, NEWS_H, GW, SCREEN_H - NEWS_H, 256);
  gfx_text_outline_center(F_KAVOON, GX + GW / 2, NEWS_H + 20, "Grimoire", WHITE, 256);
  for (int s = 0; s < N_SPELLS; s++) {
    int x, y;
    spell_pos(s, &x, &y);
    bool ready = S.grimoire_magic >= spell_cost(s);
    int col = (s + 3) % 4;   /* (the game's frames: nth-child(4n+1) ... ) */
    int row = sel == s ? 1 : 0;
    gfx_fill_alpha(x + 2, y + 2, SPELL_W, SPELL_H, BLACK, 110);
    gfx_image_part(IM_SPELL_BG, col * SPELL_W, row * SPELL_H, SPELL_W, SPELL_H, x, y - (sel == s), ready ? 256 : 205);
    gfx_icon(spells[s].icon, x + 3, y + 2 - (sel == s && ready ? (int)(fabsf(sinf(G.real_t * 0.2f)) * 2) : 0),
             ready ? 256 : 205);
    char t[16];
    snprintf(t, sizeof t, "%d", spell_cost(s));
    gfx_text_outline_center(F_NAME_SMALL, x + SPELL_W / 2, y + SPELL_H - 3, t,
                            ready ? sel == s ? WHITE : RGB(220, 220, 220) : RGB(255, 51, 51), 256);
  }
  /* the magic meter, its lump refill at its left */
  int bw = (int)(magic_max * 1.5f), bx, by = NEWS_H + 118;
  if (bw > GW - 40) bw = GW - 40;
  if (bw < 20) bw = 20;
  bx = GX + (GW - bw) / 2 + 10;
  gfx_icon_scaled(ICON(29, 14), bx - 22, by - 3, 18, can_lumps() ? 256 : 120);
  if (sel == N_SPELLS) gfx_rect(bx - 23, by - 4, 20, 20, WHITE, 256);
  gfx_fill_alpha(bx - 1, by - 1, bw + 2, 12, BLACK, 200);
  int fw = magic_max > 0 ? (int)(bw * S.grimoire_magic / magic_max) : 0;
  for (int i = 0; i < fw; i++) {
    float k = fmodf(i + G.real_t * 0.5f, 32) / 32;
    gfx_vgrad(bx + i, by, 1, 10, gfx_mix(HEX(0x7a3fd1), HEX(0xc58fff), (int)(k * 256)), 256, 200);
  }
  gfx_rect(bx - 2, by - 2, bw + 4, 14, HEX(0xece2b6), 200);
  char t[96], a[24], b[24];
  beautify(S.grimoire_magic, 0, a);
  beautify(floor(magic_max), 0, b);
  if (S.grimoire_magic < magic_max) {
    char ps[24];
    double r = magic_ps * FPS > 0.01 ? magic_ps * FPS : 0.01;
    beautify(r, 2, ps);
    snprintf(t, sizeof t, "%s/%s (+%s/s)", a, b, ps);
  } else
    snprintf(t, sizeof t, "%s/%s", a, b);
  gfx_text_outline_center(F_SMALL, bx + bw / 2, by + 9, t, WHITE, 256);
  snprintf(t, sizeof t, "Spells cast: %u (total: %u)", S.spells_cast, S.spells_cast_total);
  gfx_text_outline_center(F_SMALL, GX + GW / 2, by + 26, t, RGB(200, 200, 200), 256);
  if (aura_mult(AURA_SUPREME_INTELLECT) > 0)
    gfx_text_outline_center(F_SMALL, GX + GW / 2, by + 38, "Supreme Intellect: 10% cheaper, 10% more fails",
                            RGB(200, 180, 255), 256);
  gfx_text_center(F_SMALL, GX + GW / 2, SCREEN_H - 6, "EXE cast   Back close", RGB(160, 160, 160), 256);
  gfx_unclip();
}

/* the spell's tooltip (over the left column, as the store's) */
int grimoire_tip(int x, int y, int w, bool draw) {
  char l[400], c[32];
  int yy = y;
  if (sel == N_SPELLS) {
    extern double lump_refill_remaining(void);
    snprintf(l, sizeof l, "Press EXE to refill \x01" "100 units\x02 of your magic meter for \x01" "1 sugar lump\x02.\n%s",
             can_refill_lump() ? "(can be done once every 15 minutes)" : "(usable again later)");
    return gfx_para(&PS_SMALL, x, y, w, l, draw);
  }
  const Spell *s = &spells[sel];
  if (draw) gfx_icon(s->icon, x, yy, 256);
  if (draw) gfx_text_outline(F_NAME, x + 27, yy + 11, s->name, WHITE, 256);
  int cost = spell_cost(sel);
  snprintf(c, sizeof c, "%d", cost);
  char cb[64] = "";
  if (s->cost_percent) snprintf(cb, sizeof cb, " (%d magic +%d%% of max magic)", s->cost_min, (int)ceilf(s->cost_percent * 100));
  if (draw) {
    int tx = x + 27;
    tx = gfx_text(F_SMALL, tx, yy + 22, "Magic cost: ", RGB(204, 204, 204), 256);
    tx = gfx_text(F_BOLD, tx, yy + 22, c, cost <= S.grimoire_magic ? RGB(102, 255, 102) : RGB(255, 102, 102), 256);
    gfx_text(F_SMALL, tx, yy + 22, cb, RGB(170, 170, 170), 256);
  }
  yy += 26;
  if (s->fail_desc) {
    snprintf(l, sizeof l, "Chance to backfire: %d%%", (int)ceil(100 * fail_chance(sel)));
    if (draw) gfx_text(F_SMALL, x + 27, yy + 6, l, RGB(255, 102, 102), 256);
    yy += 10;
  }
  if (draw) tip_line(x, yy + 2, w);
  yy += 6;
  ParaStyle green = PS_TIP;
  green.color = RGB(102, 255, 102);
  snprintf(l, sizeof l, "\x01" "Effect:\x02 %s", s->desc);
  yy += gfx_para(&green, x, yy, w, l, draw) + 4;
  if (s->fail_desc) {
    ParaStyle red = PS_TIP;
    red.color = RGB(255, 102, 102);
    snprintf(l, sizeof l, "\x01" "Backfire:\x02 %s", s->fail_desc);
    yy += gfx_para(&red, x, yy, w, l, draw) + 2;
  }
  return yy - y;
}

void grimoire_keys(uint32_t p) {
  if (p & K_LEFT && sel > 0) sel--;
  if (p & K_RIGHT && sel < N_SPELLS) sel++;
  if (p & K_DOWN) sel = sel < 5 ? (sel + 5 < N_SPELLS ? sel + 5 : N_SPELLS - 1) : N_SPELLS;
  if (p & K_UP) sel = sel == N_SPELLS ? 5 : sel >= 5 ? sel - 5 : sel;
  U.tip_t = FPS * 6;
  for (int d = 1; d <= 9; d++)
    if (p & K_DIGIT(d)) sel = d - 1, p |= K_EXE;
  if (!(p & K_EXE)) return;
  if (sel == N_SPELLS) {
    if (S.grimoire_magic < magic_max && refill_lump(1)) {
      S.grimoire_magic += 100;
      if (S.grimoire_magic > magic_max) S.grimoire_magic = magic_max;
    }
    return;
  }
  cast(sel, -1, 0, false);
}
