/* The news ticker (the game's getNewTicker): its English lines, as tools/pack.py reads them from the
 * game (NK_* says what brings each up), chosen the way the game chooses; the fortunes of the Fortune
 * cookies heavenly upgrade, which pressing the ticker collects. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

typedef struct {
  uint32_t text;
  uint8_t kind, pad;
  uint16_t arg;
} NewsRec;
_Static_assert(sizeof(NewsRec) == 8, "NewsRec");
static const NewsRec *news_rec(int i) { return RES(NewsRec, OFF_NEWS) + i; }

uint32_t ticker_n;
int ticker_effect;   /* what pressing the ticker gives: 0 nothing, -1 a golden cookie, -2 an hour of CpS, else
                        the fortune upgrade + 1 */
extern void bakery_name_short(char *out, int size);

/* the game's loreProgress: how far the bakery's fame has gone */
static int lore_progress(void) {
  double v = log10(S.cookies_earned / 10) + 1;
  if (!(v > -1e9)) return 0;   /* (no cookies yet: -Infinity, 0 in the game) */
  return (int)v;               /* (|0: toward zero) */
}

/* a template written out: \x10 choices \x11 ... \x12, \x13 the bakery's name and numbers, \x14n:add; a
 * random number */
static const char *expand(const char *t, char *o, char *end, char **out) {
  while (*t && *t != 0x11 && *t != 0x12) {
    unsigned char c = (unsigned char)*t;
    if (c == 0x10) {
      /* the alternatives at this level */
      int n = 1, depth = 0;
      const char *p = t + 1;
      for (; *p && !(depth == 0 && *p == 0x12); p++) {
        if (*p == 0x10) depth++;
        else if (*p == 0x12) depth--;
        else if (*p == 0x11 && depth == 0) n++;
      }
      int k = rnd_int(n);
      p = t + 1, depth = 0;
      while (k > 0) {
        if (*p == 0x10) depth++;
        else if (*p == 0x12) depth--;
        else if (*p == 0x11 && depth == 0) k--;
        p++;
      }
      p = expand(p, o, end, &o);
      /* past the rest of the alternatives */
      depth = 0;
      while (*p && !(depth == 0 && *p == 0x12)) {
        if (*p == 0x10) depth++;
        else if (*p == 0x12) depth--;
        p++;
      }
      t = *p ? p + 1 : p;
      continue;
    }
    char v[64] = "";
    if (c == 0x13) {
      double y = ceil(S.cookies_earned / 8000000000.0);
      switch (t[1]) {
        case 'B': bakery_name_short(v, sizeof v); break;
        case 'E': beautify(S.cookies_earned, 0, v); break;
        case 'Y': beautify(y, 0, v); break;
        case 'P': snprintf(v, sizeof v, "%s", y == 1 ? "" : "s"); break;
      }
      t += 2;
    } else if (c == 0x14) {
      int n = 0, add = 0;
      t++;
      while (*t >= '0' && *t <= '9') n = n * 10 + (*t++ - '0');
      if (*t == ':') t++;
      while (*t >= '0' && *t <= '9') add = add * 10 + (*t++ - '0');
      if (*t == ';') t++;
      snprintf(v, sizeof v, "%d", rnd_int(n) + add);
    } else {
      if (o < end) *o++ = *t;
      t++;
      continue;
    }
    for (char *q = v; *q && o < end; q++) *o++ = *q;
  }
  *out = o;
  return t;
}

static void write_out(int i, char *out, int size) {
  char *o = out;
  expand(res_text(news_rec(i)->text), out, out + size - 1, &o);
  *o = 0;
}

void news_next(char *out, int size) {
  static uint16_t list[N_NEWS];
  int n = 0, lore = lore_progress();
  bool scary = !S.prefs.not_scary;
  ticker_effect = 0;
#define ADD(kind_test)                                                     \
  for (int i = 0; i < N_NEWS; i++) {                                       \
    const NewsRec *r = news_rec(i);                                        \
    if (kind_test) list[n++] = (uint16_t)i;                                \
  }
  if (ticker_n % 2 == 0 || lore > 14) {
    if (rnd() < 0.75 || S.cookies_earned < 10000) {
      int grandmas = S.buildings[B_GRANDMA].amount;
      bool wrong = has_achiev(A_JUST_WRONG);
      bool sold = wrong && rnd() < 0.05, angry = scary && wrong && rnd() < 0.4;
      ADD((r->kind == NK_GRANDMA && grandmas > 0) || (r->kind == NK_GRANDMA_THREAT && scary && grandmas >= 50) ||
          (r->kind == NK_SOLD_GRANDMA && sold) || (r->kind == NK_GRANDMA_ANGRY && angry) ||
          (r->kind == NK_GRANDMA_RETURN && scary && grandmas >= 1 && S.pledges > 0 && S.elder_wrath == 0) ||
          (r->kind == NK_BUILDING && S.buildings[r->arg].amount > 0) ||
          (r->kind == NK_SEASON && S.season == r->arg && S.cookies_earned >= 1000));
    }
    if (rnd() < 0.05)
      ADD((r->kind == NK_RARE_ACHIEV && has_achiev(r->arg)) || (r->kind == NK_RARE_UPGRADE && has(r->arg)));
    if (has_achiev(A_DUDE_SWEET) && rnd() < 0.2) ADD(r->kind == NK_DUDE_SWEET);
    if (rnd() < 0.001) ADD(r->kind == NK_WILL_WRIGHT);
    if (S.cookies_earned >= 10000) ADD(r->kind == NK_MISC);
  }
  if (n == 0) ADD(r->kind == NK_LORE && r->arg == lore);
  if (S.elder_wrath > 0 && ((S.pledges == 0 && S.resets == 0 && rnd() < 0.3) || rnd() < 0.03)) {
    n = 0;
    ADD(r->kind == NK_WRATH && r->arg == S.elder_wrath);
  }
  if (S.season == SEASON_FOOLS) {
    n = 0;
    bool rare = S.cookies_earned >= 1000 && rnd() < 0.05;
    ADD((r->kind == NK_FOOLS && S.cookies_earned >= 1000) || (r->kind == NK_FOOLS_RARE && rare) ||
        (r->kind == NK_FOOLS_BUILDING && ticker_n % 2 == 0 && S.buildings[r->arg].amount > 0));
    /* (the first of the business lines the fame has reached) */
    int best = -1;
    for (int i = 0; i < N_NEWS; i++) {
      const NewsRec *r = news_rec(i);
      if (r->kind == NK_FOOLS_LORE && lore <= r->arg && (best < 0 || r->arg < news_rec(best)->arg)) best = i;
    }
    if (best >= 0) list[n++] = (uint16_t)best;
  }
#undef ADD
  ticker_n++;
  /* a fortune, now and then */
  if (G.T > FPS * 10 && has(U_FORTUNE_COOKIES) && rnd() < (has_achiev(A_O_FORTUNA) ? 0.04 : 0.02)) {
    int cand[N_FORTUNES + 2], m = 0;
    for (int i = 0; i < N_FORTUNES; i++)
      if (!has_unlocked(LIST(FORTUNES)[i])) cand[m++] = LIST(FORTUNES)[i] + 1;
    if (!S.fortune_gc) cand[m++] = -1;
    if (!S.fortune_cps) cand[m++] = -2;
    if (m) {
      ticker_effect = cand[rnd_int(m)];
      if (ticker_effect == -1) snprintf(out, size, "Today is your lucky day!");
      else if (ticker_effect == -2) {
        uint32_t h = (uint32_t)S.bakery_name1 * 2654435761u ^ (uint32_t)S.bakery_name2 * 40503u ^ S.resets * 977u;
        int k[4];
        for (int i = 0; i < 4; i++) h = h * 1103515245u + 12345u, k[i] = (int)(h >> 16) % 100;
        snprintf(out, size, "Your lucky numbers are: %d %d %d %d", k[0], k[1], k[2], k[3]);
      } else {
        const UpgradeRec *r = upgrade_rec(ticker_effect - 1);
        const char *name = res_text(r->name), *hash = strchr(name, '#');
        snprintf(out, size, "%s: %s", hash ? hash : name, res_text(r->quote));
      }
      return;
    }
  }
  if (!n) {
    out[0] = 0;
    return;
  }
  write_out(list[rnd_int(n)], out, size);
}

/* pressing the ticker: counted (Tabloid addiction), and its fortune collected */
void ticker_click(void) {
  S.ticker_clicks++;
  if (S.ticker_clicks >= 50) win(A_TABLOID_ADDICTION);
  if (ticker_effect == -1) {
    notify("Fortune!", "A golden cookie has appeared.", ICON(10, 32), 0);
    S.fortune_gc = 1;
    Shimmer *m = shimmer_new(SHIMMER_GOLDEN, GC_NONE, false);
    if (m) m->wrath = 0;
  } else if (ticker_effect == -2) {
    notify("Fortune!", "You gain \x01one hour\x02 of your CpS (capped at double your bank).", ICON(10, 32), 0);
    S.fortune_cps = 1;
    earn(G.cookies_ps * 60 * 60 < S.cookies ? G.cookies_ps * 60 * 60 : S.cookies);
  } else if (ticker_effect > 0) {
    int u = ticker_effect - 1;
    notify(res_text(upgrade_rec(u)->name), "You've unlocked a new upgrade.", upgrade_rec(u)->icon, 0);
    unlock(u);
  }
  ticker_effect = 0;
}
