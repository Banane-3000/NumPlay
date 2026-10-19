/* The left column, as the game's DrawBackground draws it at 0.37 of its size: the blue background
 * (the grandmas' when the elders are angry), falling cookies, the big cookie's two turning flares,
 * its shadow, the cookie and its wobble, the cursors around it, the milk, the wrinklers, the
 * shaded borders (golden in a frenzy); then the cookies' count, golden cookies, buffs, popups,
 * notes. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

#define SC 0.37f
#define PANEL_H (SCREEN_H - NEWS_H)
extern float elder_wrath_d;
extern const char *buff_name(const Buff *b);
extern int buff_icon(const Buff *b);
extern int buff_aura(const Buff *b);

int milk_image(int i) { return IM_MILK_PLAIN + i; }
int santa_image(void) { return IM_SANTA0 + S.santa_level; }

static bool good_buff, bad_buff;
static void auras(void) {
  good_buff = bad_buff = false;
  for (int i = 0; i < MAX_BUFFS; i++) {
    const Buff *b = &S.buffs[i];
    if (!b->used || b->time <= 0) continue;
    if (buff_aura(b) == 1) good_buff = true;
    if (buff_aura(b) == 2) bad_buff = true;
  }
}

static void background(void) {
  float d = S.prefs.not_scary ? 0 : elder_wrath_d;
  int bg = IM_BG_BLUE, fade = -1;
  float r = 0;
  if (d >= 1 && d < 2) r = d - 1, fade = IM_BG_GRANDMAS1;
  else if (d >= 2 && d < 3) r = d - 2, bg = IM_BG_GRANDMAS1, fade = IM_BG_GRANDMAS2;
  else if (d >= 3) r = d - 3 > 1 ? 1 : d - 3, bg = IM_BG_GRANDMAS2, fade = IM_BG_GRANDMAS3;
  float tile = (float)gfx_image_w(IM_BG_BLUE);   /* (512 x 0.37, as it is packed) */
  gfx_pattern(bg, 0, NEWS_H, LEFT_W, PANEL_H, tile, tile, 0, NEWS_H, 256);
  if (fade >= 0 && r > 0) gfx_pattern(fade, 0, NEWS_H, LEFT_W, PANEL_H, tile, tile, 0, NEWS_H, (int)(r * 256));
}

static void shower(void) {
  if (!S.prefs.particles) return;
  float d = elder_wrath_d;
  int pic = -1;
  if (d <= 1.5f || S.prefs.not_scary) {
    if (G.cookies_ps >= 1000) pic = IM_SHOWER3;
    else if (G.cookies_ps >= 500) pic = IM_SHOWER2;
    else if (G.cookies_ps >= 50) pic = IM_SHOWER1;
  }
  if (pic < 0) return;
  float opacity = 1;
  if (d >= 1 && !S.prefs.not_scary) opacity = 1 - ((d < 1.5f ? d : 1.5f) - 1) / 0.5f;
  float y = fmodf(floorf(G.real_t * 2), 512) * SC;
  gfx_pattern(pic, 0, NEWS_H, LEFT_W, PANEL_H, (float)gfx_image_w(pic), (float)gfx_image_h(pic), 0, NEWS_H + y, (int)(opacity * 256));
}

static void particles(int kind) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    const Particle *p = &G.particles[i];
    if (p->life < 0 || p->kind != kind) continue;
    float o = sqrtf(1 - (float)p->life / (FPS * p->dur));
    int im = icon_index_image(p->pic);
    if (im < 0) continue;
    float s = 48 * p->size * SC;
    gfx_image_rotated(im, p->x, p->y, s, s, p->r * (float)M_PI / 180, (int)(o * 256));
  }
}

static void flares(float cx, float cy) {
  float alpha_mult = 1;
  int pic = IM_FLARE;
  bool inv = false;
  if (good_buff) pic = IM_FLARE_GOLD;
  else if (bad_buff) pic = IM_FLARE_RED, inv = true;
  float r1 = fmodf(G.real_t / FPS / 4, 1), r2 = fmodf(G.real_t / FPS / 4 + 0.5f, 1);
  if (inv) r1 = 1 - r1, r2 = 1 - r2;
  float rot = fmodf(G.real_t * 0.5f, 360) / 360 * 2 * (float)M_PI;
  float s1 = 512 * SC * (0.5f + sqrtf(r1)), s2 = 512 * SC * (0.5f + sqrtf(r2));
  gfx_blend(BLEND_NORMAL);
  gfx_image_rotated_soft(pic, cx, cy, s1, s1, rot, (int)(sinf(r1 * (float)M_PI) * alpha_mult * 256));
  gfx_image_rotated_soft(pic, cx, cy, s2, s2, rot + (float)M_PI, (int)(sinf(r2 * (float)M_PI) * alpha_mult * 256));
}

static void cursors(float cx, float cy) {
  if (!S.prefs.cursors) return;
  int amount = S.buildings[B_CURSOR].amount;
  bool fancy = S.prefs.fancy;
  bool dragon = has_buff(BUFF_DRAGONFLIGHT) || has_buff(BUFF_DRAGON_HARVEST);
  float rot = 0;
  for (int i = 0; i < amount && i < 1000; i++) {
    int n = i / 50;
    float w = 0;
    if (fancy) w = sinf(G.real_t * 0.025f + (float)((i + n * 12.45f) - 25 * floorf((i + n * 12.45f) / 25)) / 25 * 2 * (float)M_PI);
    if (w > 0.997f) w = 1.5f;
    else if (w > 0.994f) w = 0.5f;
    else w = 0;
    w *= -4;
    if (fancy) w += sinf((n + G.real_t * 0.01f) * (float)M_PI / 2) * 4;
    float y = (140 + n * 16 + w) - 16;
    float r = 7.2f;
    if (i == 0 && fancy) r -= G.real_t * 0.1f;
    if (i % 50 == 0) r += 7.2f / 2;
    rot += r / 360 * 2 * (float)M_PI;
    /* the cursor drawn at (0, y) in a frame turned by rot: its centre */
    float lx = 16, ly = y + 16;
    float sx = cx + (lx * cosf(rot) - ly * sinf(rot)) * SC, sy = cy + (lx * sinf(rot) + ly * cosf(rot)) * SC;
    if (!gfx_visible((int)sx - 7, (int)sy - 7, 14, 14)) continue;
    /* (turned beforehand: the nearest of 32 turns) */
    int k = (int)floorf(rot / (2 * (float)M_PI) * 32 + 0.5f) & 31;
    gfx_image(IM_CURSOR_R0 + k, (int)floorf(sx - 7.5f), (int)floorf(sy - 7.5f), dragon ? 64 : 256);
  }
}

static void milk(void) {
  if (!S.prefs.milk) return;
  float h = PANEL_H;
  float x = fmodf(floorf(G.real_t * 2 - (G.milk_h - G.milk_hd) * 2000 + 480 * 2), 480) * SC;
  float y = G.milk_hd * h;
  float a = 1;
  if (G.ascend_timer > 0) {
    float k = (float)G.ascend_timer / (FPS * 5 / 2);
    y *= 1 - k * k * 2, a *= 1 - k * k * 2;
  } else if (G.reincarnate_timer > 0) {
    float k = 1 - (float)G.reincarnate_timer / FPS;
    y *= 1 - k * k * 2, a *= 1 - k * k * 2;
  }
  if (y <= 0 || a <= 0) return;
  int rank = (int)floorf(G.milk_progress);
  if (rank > N_MILKS - 1) rank = N_MILKS - 1;
  if (S.milk_type > 0 && S.ascension_mode != 1) rank = S.milk_type - 1;
  extern int milk_image(int i);
  float top = SCREEN_H - y;
  float tw = MILK_TILE, th = gfx_image_h(milk_image(rank)) * MILK_TILE / (float)gfx_image_w(milk_image(rank));
  gfx_pattern(milk_image(rank), 0, (int)top, LEFT_W, (int)ceilf(y), tw, th, x, top, (int)(0.95f * a * 256));
}

static void wrinklers(void) {
  for (int i = 0; i < MAX_WRINKLERS; i++) {
    const Wrinkler *w = &S.wrinklers[i];
    if (w->phase == 0) continue;
    float sw = (100 + 2 * sinf(G.real_t * 0.2f + i * 3)) * SC, sh = (200 + 5 * sinf(G.real_t * 0.2f - 2 + i * 3)) * SC;
    float ang = -w->r * (float)M_PI / 180;
    /* drawn from (-sw/2, -10) in a frame turned by -r at the wrinkler: its centre */
    float lx = 0, ly = -10 * SC + sh / 2;
    float cx = w->x + lx * cosf(ang) - ly * sinf(ang), cy = w->y + lx * sinf(ang) + ly * cosf(ang);
    gfx_image_rotated(w->type == 1 ? IM_SHINY_WRINKLER : IM_WRINKLER, cx, cy, sw, sh, ang, (int)(w->close * 256));
  }
}

/* the cookies' count: "1.234 million" on its line, "cookies" under it, then per second */
static void counter(void) {
  char num[64], line[96];
  double c = round(G.cookies_d);
  beautify(c, 0, num);
  if (G.cookies_d >= 1000000) {   /* (the game pads to 3 decimals) */
    char *sp = strchr(num, ' '), *dot = strchr(num, '.');
    if (sp) {
      char pad[8] = "";
      if (!dot || dot > sp) strcpy(pad, ".000");
      else if (sp - dot == 2) strcpy(pad, "00");
      else if (sp - dot == 3) strcpy(pad, "0");
      char rest[48];
      strcpy(rest, sp);
      strcpy(sp, pad);
      strcat(num, rest);
    }
  }
  snprintf(line, sizeof line, "%s %s", num, c == 1 ? "cookie" : "cookies");
  char *l1 = line, *l2 = NULL;
  if (strlen(line) > 14) {
    char *sp = strchr(line, ' ');
    if (sp) *sp = 0, l2 = sp + 1;
  }
  int y = NEWS_H + (int)(PANEL_H * 0.1f);
  int h = l2 ? 39 : 27;
  gfx_fill_alpha(0, y, LEFT_W, h, BLACK, 102);
  gfx_text_outline_center(F_TITLE, LEFT_W / 2, y + 13, l1, WHITE, 256);
  if (l2) gfx_text_outline_center(F_NAME, LEFT_W / 2, y + 25, l2, WHITE, 256);
  char ps[64], t[48];
  beautify(G.cookies_ps * (1 - G.cps_sucked), 1, t);
  snprintf(ps, sizeof ps, "per second: %s", t);
  gfx_text_outline_center(F_SMALL, LEFT_W / 2, y + h - 4, ps, G.cps_sucked > 0 ? RGB(255, 0, 0) : WHITE, 256);
}

static const char *const name_a[] = {"Magic", "Fantastic", "Fancy", "Sassy", "Snazzy", "Pretty", "Cute", "Pirate",
                                     "Ninja", "Zombie", "Robot", "Radical", "Urban", "Cool", "Hella", "Sweet",
                                     "Awful", "Double", "Triple", "Turbo", "Techno", "Disco", "Electro", "Dancing",
                                     "Wonder", "Mutant", "Space", "Science", "Medieval", "Future", "Captain", "Bearded",
                                     "Lovely", "Tiny", "Big", "Fire", "Water", "Frozen", "Metal", "Plastic", "Solid",
                                     "Liquid", "Moldy", "Shiny", "Happy", "Happy Little", "Slimy", "Tasty",
                                     "Delicious", "Hungry", "Greedy", "Lethal", "Professor", "Doctor", "Power",
                                     "Chocolate", "Crumbly", "Choklit", "Righteous", "Glorious", "Mnemonic", "Psychic",
                                     "Frenetic", "Hectic", "Crazy", "Royal", "El", "Von"};
static const char *const name_b[] = {"Cookie", "Machine", "Robot", "Headbanger", "Bandit", "Castle", "Brain", "Kitchen",
                                     "Hamster", "Pug", "Bunny", "Monster", "Alligator", "Bowling", "Neutron",
                                     "Mustache", "Cowboy", "Princess", "Lord", "Tyrant", "Dragon", "Ghost", "Raptor",
                                     "Warlock", "Wizard", "Magician", "Pastry", "Bakery", "Baker", "Chef", "Cake",
                                     "Muffin", "Thing", "Chocolate", "Avalanche", "Fudge", "Kingdom", "Overlord",
                                     "Platypus", "Badger", "Squirrel", "Possum", "Raccoon", "Gunslinger", "Gnome",
                                     "Space Cowboy", "Toast", "Spaghetti", "Turtle", "Cat", "Kitten", "Dog", "Puppy",
                                     "Cookies", "Fungus", "Stew", "Machete", "Biscuit", "Crumb", "Fruitcake", "Saga",
                                     "Melon", "Pancake", "Waffle", "Mittens", "Feline", "Nose", "Nugget"};
void bakery_name(char *out, int size) {
  int a = S.bakery_name1 % (sizeof name_a / sizeof name_a[0]), b = S.bakery_name2 % (sizeof name_b / sizeof name_b[0]);
  snprintf(out, size, "%s %s's bakery", name_a[a], name_b[b]);
}

static void bakery_box(void) {
  char t[64];
  bakery_name(t, sizeof t);
  int y = NEWS_H + (int)(PANEL_H * 0.1f) - 13;
  int w = LEFT_W * 3 / 4;
  gfx_round_rect((LEFT_W - w) / 2, y, w, 12, 5, BLACK, 102);
  gfx_text_center(F_NAME_SMALL, LEFT_W / 2, y + 9, t, RGB(204, 204, 204), 256);
}

/* what the special tabs (Santa, the dragon) show at the bottom left */
static void special_tabs(void) {
  int n = (has(U_A_FESTIVE_HAT) ? 1 : 0) + (has(U_A_CRUMBLY_EGG) ? 1 : 0);
  if (!n) return;
  float y = SCREEN_H - 24 * SC * 2 - 48 * SC * 2 * n + 10;
  int k = 0;
  if (has(U_A_FESTIVE_HAT)) {
    extern int santa_image(void);
    int im = santa_image();
    if (im >= 0) gfx_image_scaled(im, 4, y - 9 + sinf(G.real_t * 0.2f) * 1, 18, 18, 256);
    y += 20, k++;
  }
  if (has(U_A_CRUMBLY_EGG)) {
    extern int dragon_frame(void);
    float bob = fabsf(cosf(G.real_t * 0.2f + k)) * 2;
    gfx_image_scaled(IM_DRAGON0 + dragon_frame(), 4, y - 9 - bob, 18, 18, 256);
  }
}

void draw_left(void) {
  auras();
  float cx = COOKIE_X, cy = COOKIE_Y;
  if (!gfx_visible(0, NEWS_H, LEFT_W, PANEL_H)) return;
  gfx_clip(0, NEWS_H, LEFT_W, PANEL_H);
  background();
  if (G.ascend_timer == 0) {
    shower();
    particles(0);
    flares(cx, cy);
    if (G.reincarnate_timer > 0)
      gfx_fill_alpha(0, NEWS_H, LEFT_W, PANEL_H, BLACK, (int)((1 - (float)G.reincarnate_timer / FPS) * 256));
    if (has_buff(BUFF_DRAGONFLIGHT) || has_buff(BUFF_DRAGON_HARVEST)) {
      float s = 300 * 2 * (1 + sinf(G.real_t * 0.013f) * 0.1f) * SC;
      gfx_image_scaled(IM_DRAGON_BG, cx - s / 2, cy - s / (1.4f + 0.2f * sinf(G.real_t * 0.01f)), s, s, 256);
    }
    float s = gfx_image_w(IM_COOKIE) * (fabsf(G.big_size - 1) < 0.004f ? 1 : G.big_size);   /* (as it is, but when it wobbles) */
    if (S.prefs.fancy) gfx_image_scaled(IM_COOKIE_SHADOW, cx - s / 2, cy - s / 2 + 20 * SC, s, s, 256);
    gfx_image_scaled(IM_COOKIE, cx - s / 2, cy - s / 2, s, s, 256);
    if (good_buff && S.prefs.particles) {
      gfx_blend(BLEND_ADD);
      float size = (float)(rnd() * 30 + 5) * SC, a = (float)(rnd() * 2 * M_PI), dd = s * 0.9f * (float)rnd() / 2;
      gfx_image_scaled(IM_GLINT, cx - size / 2 + sinf(a) * dd, cy - size / 2 + cosf(a) * dd, size, size,
                       (int)((rnd() * 0.65 + 0.1) * 256));
      gfx_blend(BLEND_NORMAL);
    }
    cursors(cx, cy);
  } else {
    extern void draw_ascend_cookie(float cx, float cy);
    draw_ascend_cookie(cx, cy);
  }
  milk();
  if (G.ascend_timer == 0) {
    wrinklers();
    if (has(U_SHIMMERING_VEIL_OFF)) {
      gfx_blend(BLEND_ADD);
      float s = (300 + sinf(G.real_t * 0.037f) * 20) * SC;
      gfx_image_rotated(IM_SHIMMERING_VEIL, cx, cy, s, s, -G.real_t * 0.01f, 256);
      gfx_blend(BLEND_NORMAL);
    }
    special_tabs();
    particles(2);
    for (int i = 0; i < MAX_TEXT_PARTICLES; i++) {
      const TextParticle *t = &G.texts[i];
      if (t->life < 0) continue;
      float o = sqrtf(1 - (float)t->life / t->dur);
      gfx_text_outline_center(F_NAME, (int)t->x, (int)t->y, t->text, WHITE, (int)(o * 256));
    }
    int border = good_buff ? IM_BORDER_GOLD : bad_buff ? IM_BORDER_RED : IM_BORDER_SOFT;
    if (good_buff && S.prefs.fancy) gfx_blend(BLEND_ADD);
    gfx_image_scaled_fast(border, 0, NEWS_H, LEFT_W, PANEL_H, 256);
    gfx_blend(BLEND_NORMAL);
    bakery_box();
    counter();
  } else
    gfx_image_scaled_fast(IM_BORDER_SOFT, 0, NEWS_H, LEFT_W, PANEL_H, 256);
  gfx_unclip();
}

/* golden cookies, wrath cookies, the cookie storm's drops */
void draw_shimmers(void) {
  for (int i = 0; i < MAX_SHIMMERS; i++) {
    const Shimmer *m = &G.shimmers[i];
    if (!m->used || m->type != SHIMMER_GOLDEN) continue;
    float k = (float)m->life / (FPS * m->dur) * 2 - 1;
    float curve = 1 - k * k * k * k;
    int id = m->id;
    float rot = 0, scale = m->size_mult * curve;
    if (S.prefs.fancy) {
      rot = sinf(id * 0.69f) * 24 + sinf(G.real_t * (0.35f + sinf(id * 0.97f) * 0.15f) + id) * (3 + sinf(id * 0.36f) * 2);
      scale = m->size_mult * (1 + sinf(id * 0.53f) * 0.2f) * curve *
              (1 + (0.06f + sinf(id * 0.41f) * 0.05f) * sinf(G.real_t * (0.25f + sinf(id * 0.73f) * 0.15f) + id));
    }
    float s = 36 * scale;
    gfx_image_rotated(m->wrath ? IM_WRATH_COOKIE : IM_GOLD_COOKIE, m->x + 18, m->y + 18, s, s,
                      rot * (float)M_PI / 180, (int)(curve * 256));
  }
}

/* the buffs in effect, at the top right of the left column, with the time they have left */
void draw_buffs(void) {
  int y = NEWS_H + 2;
  for (int i = 0; i < MAX_BUFFS; i++) {
    const Buff *b = &S.buffs[i];
    if (!b->used || b->time <= 0) continue;
    int x = LEFT_W - 22;
    gfx_round_rect(x - 1, y - 1, 22, 22, 3, BLACK, 90);
    gfx_icon_scaled(buff_icon(b), x, y, 20, 256);
    /* the time gone, as a darkening that sweeps around */
    float t = 1 - (float)b->time / b->max_time;
    for (int yy = 0; yy < 20; yy++)
      for (int xx = 0; xx < 20; xx++) {
        float a = atan2f(xx - 9.5f, -(yy - 9.5f));
        if (a < 0) a += 2 * (float)M_PI;
        if (a < t * 2 * (float)M_PI) gfx_fill_alpha(x + xx, y + yy, 1, 1, BLACK, 110);
      }
    y += 23;
  }
}

/* texts that float up (Lucky!, Frenzy...) */
void draw_popups(void) {
  for (int i = 0; i < MAX_POPUPS; i++) {
    const Popup *p = &G.popups[i];
    if (p->life <= 0) continue;
    float t = 1 - (float)p->life / (FPS * 4);
    float dy = t < 0.2f ? t / 0.2f * 32 * 0.5f : 32 * 0.5f;
    float o = t < 0.2f ? 1 : 1 - (t - 0.2f) / 0.8f;
    char lines[4][64];
    int n = 0;
    const char *s = p->text;
    while (*s && n < 4) {
      int k = 0;
      while (*s && *s != '\n' && k < 63) lines[n][k++] = *s++;
      lines[n][k] = 0;
      n++;
      if (*s == '\n') s++;
    }
    int w = 0;
    for (int k = 0; k < n; k++) {
      int lw = gfx_text_width(k ? F_SMALL : F_NAME, lines[k]);
      if (lw > w) w = lw;
    }
    int x = (int)p->x, y = (int)(p->y - dy);
    if (x - w / 2 < 4) x = 4 + w / 2;
    if (x + w / 2 > SCREEN_W - 4) x = SCREEN_W - 4 - w / 2;
    int h = 13 + (n - 1) * 11;
    gfx_round_rect(x - w / 2 - 6, y - 4, w + 12, h + 6, 6, BLACK, (int)(o * 128));
    for (int k = 0; k < n; k++)
      gfx_text_outline_center(k ? F_SMALL : F_NAME, x, y + 9 + (k ? 2 + k * 11 : 0), lines[k], WHITE, (int)(o * 256));
  }
}

/* the notes at the bottom of the screen: the newest two, and how many more there are */
void draw_notes(void) {
  int y = SCREEN_H - 2, shown = 0, more = 0;
  static const ParaStyle PS_NOTE_TITLE = {F_BOLD, F_BOLD, F_BOLD, WHITE, WHITE, WHITE, 11, 0};
  for (int i = 0; i < MAX_NOTES; i++) {
    const Note *n = &G.notes[i];
    if (!n->title[0]) continue;
    if (shown == 2) {
      more++;
      continue;
    }
    int w = 190, x = (SCREEN_W - w) / 2;
    int tx = x + 6 + (n->icon >= 0 ? 26 : 0), tw = w - 12 - (n->icon >= 0 ? 26 : 0);
    char desc[256];
    extern void text_beautify(const char *src, char *dst, int size);
    text_beautify(n->desc, desc, sizeof desc);
    int th = gfx_para(&PS_NOTE_TITLE, tx, 0, tw, n->title, false);
    int dh = desc[0] ? gfx_para(&PS_SMALL, tx, 0, tw, desc, false) : 0;
    int h = 6 + th + (desc[0] ? 3 + dh : 0) + 2;
    if (h < 30 && n->icon >= 0) h = 30;
    y -= h;
    if (gfx_visible(x, y, w, h)) {
      draw_frame_box(x, y, w, h);
      if (n->icon >= 0) gfx_icon(n->icon, x + 4, y + (h - 24) / 2, 256);
      int yy = y + 3 + (h - (6 + th + (desc[0] ? 3 + dh : 0) + 2)) / 2;
      yy += gfx_para(&PS_NOTE_TITLE, tx, yy, tw, n->title, true);
      if (desc[0]) {
        tip_line(tx, yy + 1, tw);
        gfx_para(&PS_SMALL, tx, yy + 3, tw, desc, true);
      }
    }
    y -= 2;
    shown++;
  }
  if (more) {
    char t[48];
    snprintf(t, sizeof t, "+%d more (delete: close)", more);
    int tw = gfx_text_width(F_SMALL, t) + 10;
    gfx_round_rect((SCREEN_W - tw) / 2, y - 12, tw, 12, 4, BLACK, 170);
    gfx_text_center(F_SMALL, SCREEN_W / 2, y - 3, t, RGB(200, 200, 200), 256);
  }
}
