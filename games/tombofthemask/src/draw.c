/* Drawing (draw.h). The screen is made 16 rows at a time in a strip and sent: the tomb (walls from their quarter
 * sprites, then what lies in it, what moves, the player, the effects, the lava), then the bar on top and the key
 * line at the bottom, then any menu. Sprites are the original's white and black masks, tinted as it tints them
 * (GameController's colours: the zone's pair, yellow, the enemies' green, grey while frozen). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "draw.h"
#include "art.h"
#include "inner.h"

#define BAND 16
#define BAR 14                          /* the bar on top */
#define KEYS 226                        /* the key line, to the bottom */
#define MIDY ((BAR + KEYS) / 2)
#define RGB(r, g, b) (uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
#define BLACK 0
#define WHITE 0xFFFF
#define YELLOW 0xFFA0                   /* (1, 0.965, 0) */
#define GREEN 0x07FA                    /* (0, 1, 0.824): the enemies */
#define GREY 0xBDD7                     /* (0.729, 0.729, 0.729): frozen */
#define DIM RGB(70, 70, 70)
#define SHADE RGB(28, 28, 28)

#ifdef HOST
int tm_clipped;
const char *tm_clipped_text;
#endif

static uint16_t strip[320 * BAND];
static int band;                        /* the strip's top row */
static bool ink_only;                   /* sprites' black pixels left out (pictures on a coloured box) */
static uint32_t now_ms;

/* ------------------------------------------------------------------ pixels */
static uint16_t mix(uint16_t a, uint16_t b, int n) {   /* a over b, n/256 */
  int r = ((a >> 11) * n + (b >> 11) * (256 - n)) >> 8;
  int g = (((a >> 5) & 63) * n + ((b >> 5) & 63) * (256 - n)) >> 8;
  int bl = ((a & 31) * n + (b & 31) * (256 - n)) >> 8;
  return (uint16_t)(r << 11 | g << 5 | bl);
}
/* two colours back and forth, eased, a period each way (UpdateDotColor, UpdateWallColor) */
static uint16_t pulse(uint16_t c0, uint16_t c1, uint32_t t, uint32_t period) {
  uint32_t k = t % (2 * period);
  if (k >= period) k = 2 * period - k;
  int f = (int)(k * 256 / period);
  f = f * f * (768 - 2 * f) >> 16;                       /* smoothstep */
  return mix(c1, c0, f);
}
static void fill(int x, int y, int w, int h, uint16_t c) {
  int x0 = x < 0 ? 0 : x, x1 = x + w > 320 ? 320 : x + w;
  int y0 = y < band ? band : y, y1 = y + h > band + BAND ? band + BAND : y + h;
  for (int j = y0; j < y1; j++)
    for (int i = x0; i < x1; i++) strip[(j - band) * 320 + i] = c;
}
static void frame(int x, int y, int w, int h, int t, uint16_t c) {
  fill(x, y, w, t, c), fill(x, y + h - t, w, t, c), fill(x, y, t, h, c), fill(x + w - t, y, t, h, c);
}
static void dots_line(int x, int y, int w, uint16_t c) {
  for (int i = 0; i < w; i += 2) fill(x + i, y, 1, 1, c);
}

/* a sprite with its top left at (x, y), turned r quarter turns clockwise, mirrored first if flip, scaled s times;
 * white pixels take the tint, black ones stay black */
static void spr_at(int id, int x, int y, int r, bool flip, uint16_t tint, int s) {
  if (id < 0 || id >= TM_SPRITES) return;
  const tm_sprite *sp = &tm_sprites[id];
  int w = sp->w, h = sp->h, bw = (r & 1 ? h : w) * s, bh = (r & 1 ? w : h) * s;
  if (y >= band + BAND || y + bh <= band || x >= 320 || x + bw <= 0) return;
  const uint8_t *cov = tm_cover + sp->at, *ink = tm_ink + sp->at;
  int ya = y < band ? band : y, yb = y + bh > band + BAND ? band + BAND : y + bh;
  int xa = x < 0 ? 0 : x, xb = x + bw > 320 ? 320 : x + bw;
  for (int yy = ya; yy < yb; yy++) {
    int j = (yy - y) / s;
    uint16_t *out = strip + (yy - band) * 320;
    for (int xx = xa; xx < xb; xx++) {
      int i = (xx - x) / s, u, v;
      switch (r & 3) {
        case 0: u = i, v = j; break;
        case 1: u = j, v = h - 1 - i; break;
        case 2: u = w - 1 - i, v = h - 1 - j; break;
        default: u = w - 1 - j, v = i; break;
      }
      if (flip) u = w - 1 - u;
      int k = v * w + u;
      if (!(cov[k >> 3] & (128 >> (k & 7)))) continue;
      if (ink[k >> 3] & (128 >> (k & 7))) out[xx] = tint;
      else if (!ink_only) out[xx] = BLACK;
    }
  }
}
/* centred on (cx, cy) */
static void spr(int id, int cx, int cy, int r, bool flip, uint16_t tint) {
  if (id < 0 || id >= TM_SPRITES) return;
  const tm_sprite *sp = &tm_sprites[id];
  int bw = r & 1 ? sp->h : sp->w, bh = r & 1 ? sp->w : sp->h;
  spr_at(id, cx - bw / 2, cy - bh / 2, r, flip, tint, 1);
}

/* ------------------------------------------------------------------ text (the original's pixel font, capitals) */
static unsigned glyph_of(char ch) {
  unsigned c = (unsigned char)ch;
  if (c >= 'a' && c <= 'z') c -= 32;
  return c < 32 || c > 127 ? '?' - 32 : c - 32;
}
static int text_w(const char *s) {
  int w = 0;
  for (; *s; s++) w += tm_glyphs[glyph_of(*s)].advance;
  return w;
}
static int text(const char *s, int x, int y, uint16_t col) {
#ifdef HOST
  if (band == 0 && (x < 0 || x + text_w(s) > 320 || y < 0 || y + 10 > 240)) tm_clipped++, tm_clipped_text = s;
#endif
  for (; *s; s++) {
    unsigned c = glyph_of(*s);
    const tm_glyph *gl = &tm_glyphs[c];
    if (c) {
      int x0 = x + gl->x, y0 = y + gl->y;
      for (int j = 0; j < gl->h; j++) {
        int yy = y0 + j;
        if (yy < band || yy >= band + BAND) continue;
        for (int i = 0; i < gl->w; i++) {
          int k = j * gl->w + i, xx = x0 + i;
          if ((unsigned)xx < 320 && (tm_font_bits[gl->at + (k >> 3)] & (128 >> (k & 7)))) strip[(yy - band) * 320 + xx] = col;
        }
      }
    }
    x += gl->advance;
  }
  return x;
}
static void ctext(const char *s, int cx, int y, uint16_t col) { text(s, cx - text_w(s) / 2, y, col); }
static void rtext(const char *s, int rx, int y, uint16_t col) { text(s, rx - text_w(s), y, col); }
/* lines no wider than w, from the top; returns the y after them */
static int wrap(const char *s, int x, int y, int w, int step, uint16_t col, bool centred) {
  char line[48];
  while (*s) {
    int n = 0, last = -1;
    while (s[n] && s[n] != '\n') {
      if (n >= 47) break;
      memcpy(line, s, (size_t)n + 1), line[n + 1] = 0;
      if (text_w(line) > w) break;
      if (s[n] == ' ') last = n;
      n++;
    }
    int cut = n;
    if (s[n] && s[n] != '\n' && last > 0) cut = last;
    memcpy(line, s, (size_t)cut), line[cut] = 0;
    if (centred) ctext(line, x + w / 2, y, col);
    else text(line, x, y, col);
    s += cut;
    while (*s == ' ' || *s == '\n') {
      bool nl = *s == '\n';
      s++;
      if (nl) break;
    }
    y += step;
  }
  return y;
}

/* ------------------------------------------------------------------ the view */
static int cam_px, cam_py, shake_x, shake_y;
static uint16_t c_main, c_sub, c_wall, c_dot, c_enemy, c_spike, c_lava;
static int to_px(int32_t u) { return (int)((int64_t)u * 3 >> 10); }   /* 12 pixels a tile */
static int sx_of(int32_t wx) { return 160 + to_px(wx) - cam_px + shake_x; }
static int sy_of(int32_t wy) { return MIDY - (to_px(wy) - cam_py) + shake_y; }

static int lane_of(int x) {
  x &= TM_W - 1;
  if (x < TM_LANE) return 0;
  if (x >= TM_LANE + TM_GAP && x < 2 * TM_LANE + TM_GAP) return 1;
  return -1;
}
/* a wall for the walls' look: walls, spikes, trap spikes, bonus walls; the arcade's void too (but its halls) */
static bool wall_look(const tm_game *g, int x, int y) {
  uint8_t t = tm_tile(g, x, y);
  if (g->mode == MODE_ARCADE && lane_of(x) < 0) return t != T_EMPTY || !(tm_look(g, x, y) & 128);
  return t == T_WALL || t == T_INNER || t == T_SPIKES || t == T_MSPIKES || t == T_BONUS_WALL;
}

static unsigned vary(int x, int y, int q) { return (unsigned)(x * 7349 + y * 1931 + q * 977) * 2654435761u >> 24; }

/* a wall cell from four 6x6 quarters by its neighbours: corner, inner corner, edge or nothing (WallController) */
static void wall_cell(const tm_game *g, int x, int y, int px, int py, int set, uint16_t c, bool alone) {
  static const int8_t qd[4][2] = {{-1, 1}, {1, 1}, {-1, -1}, {1, -1}};   /* UL UR DL DR: (h side, v side) */
  for (int q = 0; q < 4; q++) {
    int hx = qd[q][0], vy = qd[q][1];
    bool h = !alone && wall_look(g, x + hx, y), v = !alone && wall_look(g, x, y + vy), d = !alone && wall_look(g, x + hx, y + vy);
    int part;
    if (!h && !v) part = q;                                    /* corner */
    else if (h && v && !d) part = 4 + q;                       /* inner corner */
    else if (!h && v) part = hx < 0 ? 9 : 10;                  /* edge left or right */
    else if (h && !v) part = vy > 0 ? 8 : 11;                  /* edge up or down */
    else continue;
    int qx = px + (q & 1) * 6, qy = py + (q >> 1) * 6;
    if (set < 5) {
      int first = tm_wall_part[set][part][0], n = tm_wall_part[set][part][1];
      spr_at(first + (int)(vary(x, y, q) % (unsigned)n), qx, qy, 0, false, c, 1);
    } else {
      spr_at(S_SECRET + part, qx, qy, 0, false, c, 1);
    }
  }
}

/* spikes and trap spikes: their teeth on the open sides */
static void spike_cell(const tm_game *g, int x, int y, int px, int py, int first, uint16_t c) {
  static const int8_t qd[4][2] = {{-1, 1}, {1, 1}, {-1, -1}, {1, -1}};
  for (int q = 0; q < 4; q++) {
    int hx = qd[q][0], vy = qd[q][1];
    bool h = wall_look(g, x + hx, y), v = wall_look(g, x, y + vy);
    int part;
    if (!h && !v) part = q;
    else if (!h) part = hx < 0 ? 5 : 6;
    else if (!v) part = vy > 0 ? 4 : 7;
    else continue;
    spr_at(first + part, px + (q & 1) * 6, py + (q >> 1) * 6, 0, false, c, 1);
  }
}

/* quarter turns for a sprite drawn pointing up (the player's), or right (cannons, bullets, trap spikes) */
static int dir_turn(int dx, int dy) { return dy > 0 ? 0 : dx > 0 ? 1 : dy < 0 ? 2 : 3; }
static int dir_turn_r(int dx, int dy) { return dx > 0 ? 0 : dy < 0 ? 1 : dx < 0 ? 2 : 3; }

static void cell(const tm_game *g, int x, int y) {
  uint8_t t = tm_tile(g, x, y);
  if (t == T_EMPTY && !(g->mode == MODE_ARCADE && lane_of(x) < 0)) return;
  int cx = sx_of(x * TT), cy = sy_of(y * TT), px = cx - 6, py = cy - 6;
  if (py >= band + BAND || py + 12 <= band) {
    if (t != T_TEETH_R && t != T_TEETH_L && t != T_FISH) return;
  }
  uint8_t lk = tm_look(g, x, y);
  uint32_t tick = g->tick;
  int set = lk & 7;
  if (set > 5) set = 0;
  switch (t) {
    case T_EMPTY:
    case T_WALL:
    case T_INNER:
      if (t == T_EMPTY || wall_look(g, x, y)) wall_cell(g, x, y, px, py, set, c_wall, false);
      break;
    case T_BONUS_WALL: wall_cell(g, x, y, px, py, 5, c_wall, false); break;
    case T_BRICK: wall_cell(g, x, y, px, py, set, c_wall, true); break;
    case T_SPIKES: spike_cell(g, x, y, px, py, S_SPIKES, c_spike); break;
    case T_MSPIKES: spike_cell(g, x, y, px, py, S_MSPIKES, c_wall); break;
    case T_DOT: spr(S_DOT, cx, cy, 0, false, c_dot); break;
    case T_COIN: spr(S_COIN + (int)(tick / 6 % 4), cx, cy, 0, false, c_dot); break;
    case T_STAR: spr(S_STAR + (int)(tick / 6 % 4), cx, cy, 0, false, YELLOW); break;
    case T_EXIT: spr(S_EXIT + (int)(tick / 6 % 3), cx, cy, 0, false, YELLOW); break;
    case T_ICE: spr(S_ICE, cx, cy, 0, false, YELLOW); break;
    case T_DOOR: spr(S_DOOR, cx, cy, 0, false, YELLOW); break;
    case T_ADDICT: spr(S_ADDICT + (int)(tick / 6 % 4), cx, cy, 0, false, YELLOW); break;
    case T_FREEZE: spr(S_FREEZE + (int)(tick / 6 % 4), cx, cy, 0, false, YELLOW); break;
    case T_MAGNET: spr(S_MAGNET + (int)(tick / 6 % 4), cx, cy, 0, false, YELLOW); break;
    case T_BOOST: spr(S_BOOST + (int)(tick / 6 % 4), cx, cy, 0, false, YELLOW); break;
    case T_ROTOR_CW:
    case T_ROTOR_CCW: spr(S_ROTOR, cx, cy, 0, t == T_ROTOR_CCW, c_wall); break;
    case T_FISH:
      if (g->fish_frame > 0) spr(S_FISH_ATTACK + (g->fish_frame > 4 ? 4 : g->fish_frame), cx, cy, 0, false, c_wall);
      else spr(S_FISH + (int)(tick / 6 % 4), cx, cy, 0, false, c_wall);
      break;
    case T_TEETH_R:
    case T_TEETH_L: spr(S_TEETH + (int)(tick / 6 % 6), cx + (t == T_TEETH_R ? 6 : -6), cy, 0, t == T_TEETH_L, c_enemy); break;
    case T_TIMER_WALL: {
      int f = 0;
      for (int i = 0; i < TM_TIMER_WALLS; i++)
        if (g->tw[i].left > 0 && g->tw[i].x == x && g->tw[i].y == y) f = g->tw[i].left > 110 ? 10 - (g->tw[i].left - 110) : 10;
      spr(S_TIMERWALL + (f < 0 ? 0 : f > 10 ? 10 : f), cx, cy, 0, false, c_wall);
      break;
    }
    case T_CLOSE:
      dots_line(px, py, 12, c_wall), dots_line(px, py + 11, 12, c_wall);
      for (int j = 0; j < 12; j += 2) fill(px, py + j, 1, 1, c_wall), fill(px + 11, py + j, 1, 1, c_wall);
      break;
    default:
      if (tm_is_portal(t)) spr(S_PORTAL + (int)(tick / 4 % 11), cx, cy, t >= 61 ? 1 : 0, false, c_wall);
      else if (tm_is_tramplin(t)) spr(S_TRAMPLIN, cx, cy, t == T_TRAMP_UR ? 0 : t == T_TRAMP_DR ? 1 : t == T_TRAMP_DL ? 2 : 3, false, c_wall);
      else if (tm_is_cannon(t)) {
        int dx = t == T_CANNON_R ? 1 : t == T_CANNON_L ? -1 : 0, dy = t == T_CANNON_U ? 1 : t == T_CANNON_D ? -1 : 0;
        spr(S_CANNON + (g->cannon_flash > 0 && g->pw_time[PW_FREEZE] <= 0), cx, cy, dir_turn_r(dx, dy), false, c_enemy);
      }
      break;
  }
}

/* the player: its mask's frames by state, turned to the wall it rests on or the way it flies */
static void player(const tm_game *g) {
  if (!g->alive) return;
  int cx = sx_of(g->x), cy = sy_of(g->y);
  if (cy - 16 >= band + BAND || cy + 16 <= band) return;
  int m = g->p.mask, base = S_CHAR0_ARRIVE + m * (S_CHAR1_ARRIVE - S_CHAR0_ARRIVE);
  bool moving = !tm_standing(g);
  if (moving && g->trail > 0 && !g->running) {
    /* the trail (Hero_jump_trail: 8 pixels wide, stretched behind) */
    int len = to_px(g->trail) > 36 ? 36 : to_px(g->trail);
    if (g->dx) fill(g->dx > 0 ? cx - len : cx, cy - 4, len, 8, YELLOW);
    else fill(cx - 4, g->dy > 0 ? cy : cy - len, 8, len, YELLOW);
  }
  int id, r;
  if (g->pstate == ST_ARRIVE) id = base + (g->anim < 8 ? g->anim : 7), r = 0;
  else if (g->pstate == ST_RUN) id = base + 8 + 6 + 5 + g->anim % 10, r = 0;
  else if (g->pstate == ST_LONG) id = base + 8 + 6 + g->anim % 5, r = moving ? dir_turn(g->dx, g->dy) : 0;
  else if (moving) id = S_FLIGHT + g->anim % 3, r = dir_turn(g->dx, g->dy);
  else id = base + 8 + g->anim % 6, r = (4 - g->face) & 3;   /* face 0 floor, 1 right wall, 2 ceiling, 3 left */
  spr(id, cx, cy, r, false, g->blink & 4 ? DIM : YELLOW);
  if (g->pw_time[PW_SHIELD] > 0 && (g->pw_time[PW_SHIELD] > 120 || g->tick & 8)) spr(S_SHIELD_FX + (int)(g->tick / 6 % 2), cx, cy, 0, false, YELLOW);
}

static void actors(const tm_game *g, bool monkeys) {
  for (int i = 0; i < TM_ACTORS; i++) {
    const tm_actor *a = &g->a[i];
    if (!a->type || (a->type == A_MONKEY) != monkeys) continue;
    int cx = sx_of(a->x), cy = sy_of(a->y);
    uint16_t c = a->frozen ? GREY : c_enemy;
    switch (a->type) {
      case A_BAT: spr(S_BAT + a->frame % 5, cx, cy, 0, a->dx < 0, c); break;
      case A_BULLET: if (a->timer) spr(S_BULLET, cx, cy, dir_turn_r(a->dx, a->dy), false, c_enemy); break;
      case A_ROCK: spr(S_ROCK, cx, cy, 0, false, c_enemy); break;
      case A_SPIKESM: if (!a->state) spr(S_SPIKESM + (a->frame > 2 ? 2 : a->frame), cx, cy, dir_turn_r(a->dx, a->dy), false, c); break;
      case A_PULL: spr(a->aux == T_COIN ? S_COIN : S_DOT, cx, cy, 0, false, c_dot); break;
      case A_MONKEY: {
        spr((a->state == 4 || a->state == 2 ? S_MONKEY_EMPTY : S_MONKEY) + a->frame % 8, cx, cy, 0, false, c_enemy);
        break;
      }
      default: break;
    }
  }
}

static void things(const tm_game *g) {
  for (int p = 0; p < TM_PLATFORMS; p++) {
    const tm_platform *pl = &g->plat[p];
    if (!pl->active) continue;
    for (int i = 0; i < pl->count; i++)
      spr(S_PLATFORM + pl->frame % 3, sx_of(pl->x + pl->bx[i] * TT), sy_of(pl->y + pl->by[i] * TT), 0, false, c_wall);
  }
  /* rotors mid-turn: their parts swing round the centre */
  static const int16_t turn_sin[8] = {0, 57, 111, 160, 200, 231, 250, 256};
  for (int r = 0; r < TM_ROTORS; r++) {
    const tm_rotor *ro = &g->rotor[r];
    if (!ro->active || !ro->turning) continue;
    int k = ro->timer > 7 ? 7 : ro->timer, s = turn_sin[k], c = turn_sin[7 - k];
    int sgn = ro->ccw ? -1 : 1, rcx = sx_of(ro->x * TT), rcy = sy_of(ro->y * TT);
    for (int i = 0; i < ro->count; i++) {
      int ox = ro->px[i] * 12, oy = -ro->py[i] * 12;               /* screen offsets */
      int nx = (ox * c - sgn * oy * s) / 256, ny = (oy * c + sgn * ox * s) / 256;
      if (tm_wallish(ro->tile[i])) fill(rcx + nx - 5, rcy + ny - 5, 10, 10, ro->tile[i] == T_SPIKES ? c_spike : c_wall);
      else frame(rcx + nx - 5, rcy + ny - 5, 10, 10, 1, c_wall);
    }
  }
  for (int s = 0; s < TM_SNAKES; s++) {
    const tm_snake *sn = &g->snake[s];
    if (!sn->active) continue;
    const tm_snake_line *l = &sn->l[sn->line];
    uint16_t c = sn->frozen ? GREY : c_enemy;
    int hx = sx_of(l->x * TT + l->dx * HALF), hy = sy_of(l->y * TT + l->dy * HALF);
    if (sn->delay > 0) {
      spr((l->dy ? S_SNAKE_OUT_UP : S_SNAKE_OUT) + (int)((60 - sn->delay) / 10 % 6), hx, hy, l->dy < 0 ? 2 : 0, l->dx < 0, c);
      continue;
    }
    for (int i = 0; i < l->n; i++) {
      if (!l->open[i]) continue;
      int32_t c0 = (2 * i + 1) * HALF;
      if (sn->q < c0 - HALF || sn->q - 23 * TT > c0 + HALF) continue;
      int x = sx_of((l->x + l->dx * (i + 1)) * TT), y = sy_of((l->y + l->dy * (i + 1)) * TT);
      fill(x - 4, y - 4, 8, 8, c);
      fill(x - 2, y - 2, 4, 4, BLACK);
    }
  }
  for (int b = 0; b < TM_BALLS; b++) {
    const tm_ball *bl = &g->ball[b];
    if (bl->active) spr(S_LIGHTBALL + (int)(g->tick / 4 % 4), sx_of(bl->x), sy_of(bl->y), 0, false, bl->awake ? c_enemy : DIM);
  }
}

static void effects(const tm_game *g) {
  for (int i = 0; i < TM_FX; i++) {
    const tm_actor *f = &g->fx[i];
    if (!f->type) continue;
    int cx = sx_of(f->x), cy = sy_of(f->y), t = f->timer;
    switch (f->type) {
      case FX_LANDING:
        if (t < 18) spr(S_LANDING + t / 3, cx + f->dx * 6, cy - f->dy * 6, dir_turn(-f->dx, -f->dy), false, YELLOW);
        break;
      case FX_DOT: if (t < 12) spr(S_DOT_FX + t / 4, cx, cy, 0, false, c_dot); break;
      case FX_COIN: if (t < 16) spr(S_COIN_FX + t / 4, cx, cy, 0, false, YELLOW); break;
      case FX_POWER: if (t < 20) spr(S_POWER_FX + t / 4, cx, cy, 0, false, YELLOW); break;
      case FX_ICE: if (t < 16) spr(S_ICE_FX + t / 4, cx, cy, 0, false, YELLOW); break;
      case FX_PORTAL: if (t < 21) spr(S_PORTAL_FX + t / 3, cx, cy, 0, false, c_wall); break;
      case FX_BANG: if (t < 12) spr(S_BANG + t / 3, cx, cy, 0, false, c_enemy); break;
      case FX_DEATH: if (t < 40) spr(S_DEATH + t / 4, cx, cy, 0, false, YELLOW); break;
      case FX_JUMP: if (t < 6) spr(S_JUMP_FX + t / 3, cx - f->dx * 8, cy + f->dy * 8, dir_turn(f->dx, f->dy), false, YELLOW); break;
      case FX_BRICK: if (t < 8) frame(cx - 6, cy - 6, 12, 12, 1, WHITE); break;
      default: break;
    }
  }
}

static void lava(const tm_game *g) {
  if (!g->lava_max) return;
  int top = sy_of(tm_lava_shown(g));
  if (top >= band + BAND) return;
  int y0 = top < band ? band : top;
  fill(0, y0, 320, band + BAND - y0, c_lava);
  if (top >= band) {
    /* its surface: a row of little waves */
    for (int x = 0; x < 320; x++) {
      int w = (x + (int)(g->tick / 2)) % 12;
      if (w < 4) fill(x, top - 1, 1, 1, c_lava);
    }
    fill(0, top + 2, 320, 1, mix(WHITE, c_lava, 80));
  }
}

static void world(const tm_game *g) {
  int x0 = (cam_px - 172) / 12 - 1, x1 = (cam_px + 172) / 12 + 1;
  int ytop = (cam_py + MIDY - band + 20) / 12 + 1, ybot = (cam_py + MIDY - band - BAND - 20) / 12 - 1;
  for (int y = ybot; y <= ytop; y++)
    for (int x = x0; x <= x1; x++) cell(g, x, y);
  if (g->mode == MODE_ARCADE && g->gate_frame < 13) spr(S_GATES + g->gate_frame, sx_of(6 * TT), sy_of(-2 * TT), 0, false, c_wall);
  things(g);
  actors(g, false);
  player(g);
  effects(g);
  lava(g);
  actors(g, true);
}

/* ------------------------------------------------------------------ the bar on top and the key line */
static void num(char *b, size_t n, const char *fmt, unsigned v) { snprintf(b, n, fmt, v); }

static void bar(const tm_game *g) {
  fill(0, 0, 320, BAR, BLACK);
  fill(0, BAR - 1, 320, 1, SHADE);
  char b[32];
  if (g->mode == MODE_ARCADE) {
    spr(S_DOT, 8, 6, 0, false, YELLOW);
    num(b, sizeof b, "%u", g->score);
    text(b, 16, 2, YELLOW);
  } else {
    num(b, sizeof b, "STAGE %u", (unsigned)g->stage + 1);
    text(b, 4, 2, YELLOW);
    for (int i = 0; i < g->stars_all; i++) spr(S_STAR_SMALL, 112 + i * 12, 6, 0, false, i < g->stars ? YELLOW : DIM);
  }
  /* the powerups on: their icons and time left */
  static const int icon[PW_COUNT] = {S_SHIELD_ICON, S_ADDICT, S_FREEZE, S_MAGNET, S_BOOST};
  int x = g->mode == MODE_ARCADE ? 150 : 160;
  for (int i = 0; i < PW_COUNT; i++) {
    if (g->pw_time[i] <= 0) continue;
    spr(icon[i], x, 6, 0, false, YELLOW);
    int w = (int)(g->pw_time[i] * 10 / (g->pw_full[i] ? g->pw_full[i] : 1));
    fill(x - 5, 12, w, 1, YELLOW);
    x += 16;
  }
  num(b, sizeof b, "%u", g->p.coins);
  int w = text_w(b);
  rtext(b, 316, 2, YELLOW);
  spr(S_COIN_ICON, 316 - w - 8, 6, 0, false, YELLOW);
}

static void keyline(const char *s) {
  fill(0, KEYS, 320, 240 - KEYS, BLACK);
  fill(0, KEYS, 320, 1, SHADE);
  ctext(s, 160, KEYS + 3, WHITE);
}

/* ------------------------------------------------------------------ menus */
static void panel(int x, int y, int w, int h) {
  fill(x, y, w, h, BLACK);
  frame(x, y, w, h, 2, YELLOW);
}
/* a row of a list: the chosen one filled yellow with black writing, impossible to miss */
static void row(int x, int y, int w, bool on, const char *left, const char *right) {
  if (on) {
    fill(x, y, w, 14, YELLOW);
    for (int k = 0; k < 4; k++) fill(x + 4 + k, y + 3 + k, 1, 8 - 2 * k, BLACK);   /* a pointer */
  }
  uint16_t c = on ? BLACK : WHITE;
  text(left, x + 14, y + 2, c);
  if (right) rtext(right, x + w - 6, y + 2, on ? BLACK : YELLOW);
}

static const char *const title_items[M_COUNT] = {"ARCADE", "STAGES", "SHOP", "MASKS", "MISSIONS", "CHEST"};

static void title(const tm_game *g, const tm_ui *v) {
  fill(0, 0, 320, 240, BLACK);
  spr_at(S_LOGO, 160 - 140, 8, 0, false, YELLOW, 2);
  char b[40];
  unsigned lv = tm_level(g->p.xp);
  num(b, sizeof b, "LEVEL %u", lv);
  text(b, 20, 80, YELLOW);
  spr(S_COIN_ICON, 234, 85, 0, false, YELLOW);
  num(b, sizeof b, "%u", g->p.coins);
  text(b, 244, 80, YELLOW);
  /* the level's progress */
  if (lv < 20) {
    uint32_t a = tm_level_xp[lv - 1], z = tm_level_xp[lv];
    int w = (int)((uint64_t)(g->p.xp - a) * 120 / (z - a));
    frame(100, 82, 124, 8, 1, DIM), fill(102, 84, w, 4, YELLOW);
  }
  int ready = 0;
  for (int i = 0; i < 3; i++) ready += tm_mission_ready(&g->p, i);
  for (int i = 0; i < M_COUNT; i++) {
    const char *right = NULL;
    char r[24];
    if (i == M_ARCADE && g->p.best) num(r, sizeof r, "BEST %u", g->p.best), right = r;
    if (i == M_STAGES) num(r, sizeof r, "%u/200", (unsigned)g->p.reached + 1), right = r;
    if (i == M_MISSIONS && ready) num(r, sizeof r, "%u READY", (unsigned)ready), right = r;
    if (i == M_CHEST && tm_chest_free(&g->p)) right = "FREE";
    row(70, 100 + i * 18, 180, v->sel == i, title_items[i], right);
  }
  if (v->save_failed) ctext("THE SAVE COULD NOT BE WRITTEN", 160, 210, RGB(255, 90, 90));
}

static void stages(const tm_game *g, const tm_ui *v) {
  fill(0, 0, 320, 240, BLACK);
  int page = v->stage_sel / 40;
  char b[32];
  num(b, sizeof b, "STAGES %u-", (unsigned)page * 40 + 1);
  char e[12];
  num(e, sizeof e, "%u", (unsigned)page * 40 + 40);
  strcat(b, e);
  text(b, 8, 4, YELLOW);
  unsigned stars = 0;
  for (int i = 0; i < 200; i++) stars += g->p.stars[i];
  num(b, sizeof b, "%u", stars);
  rtext(b, 312, 4, YELLOW);
  spr(S_STAR_SMALL, 312 - text_w(b) - 8, 8, 0, false, YELLOW);
  for (int k = 0; k < 40; k++) {
    int s = page * 40 + k, x = 8 + (k % 8) * 38, y = 20 + (k / 8) * 40;
    bool open = s <= g->p.reached, on = s == v->stage_sel;
    if (on) fill(x, y, 36, 38, YELLOW);
    else frame(x, y, 36, 38, 1, open ? DIM : SHADE);
    num(b, sizeof b, "%u", (unsigned)s + 1);
    if (open) ctext(b, x + 18, y + 6, on ? BLACK : WHITE);
    else ink_only = on, spr(S_LOCK, x + 18, y + 12, 0, false, on ? BLACK : DIM), ink_only = false;
    ink_only = on;
    if (open)
      for (int i = 0; i < 3; i++) spr(S_STAR_SMALL, x + 7 + i * 11, y + 27, 0, false, i < g->p.stars[s] ? (on ? BLACK : YELLOW) : (on ? RGB(150, 140, 0) : SHADE));
    ink_only = false;
  }
}

static const char *const upgrade_name[4] = {"COIN ADDICT", "FREEZE", "MAGNET", "SCORE BOOST"};
static const int upgrade_icon[5] = {S_SHIELD_BIG, S_ADDICT_BIG, S_FREEZE_BIG, S_MAGNET_BIG, S_BOOST_BIG};

static void shop(const tm_game *g, const tm_ui *v) {
  fill(0, 0, 320, 240, BLACK);
  text("SHOP", 8, 4, YELLOW);
  char b[40];
  num(b, sizeof b, "%u", g->p.coins);
  rtext(b, 312, 4, YELLOW);
  spr(S_COIN_ICON, 312 - text_w(b) - 8, 8, 0, false, YELLOW);
  for (int i = 0; i < 5; i++) {
    int y = 22 + i * 38;
    bool on = v->shop_sel == i;
    if (on) fill(6, y, 308, 36, YELLOW);
    else frame(6, y, 308, 36, 1, SHADE);
    uint16_t c = on ? BLACK : WHITE, c2 = on ? BLACK : YELLOW;
    ink_only = on;
    spr(upgrade_icon[i], 26, y + 18, 0, false, c2);
    ink_only = false;
    if (i == 0) {
      text("SHIELD", 48, y + 5, c);
      num(b, sizeof b, "%u OF 10", g->p.shields);
      text(b, 48, y + 20, c);
      if (g->p.shields < 10) num(b, sizeof b, "%u", 200), rtext(b, 306, y + 12, c2);
      else rtext("FULL", 306, y + 12, c2);
    } else {
      int lv = g->p.upgrades[i - 1];
      text(upgrade_name[i - 1], 48, y + 5, c);
      for (int k = 0; k < 9; k++) {
        if (k < lv) fill(48 + k * 12, y + 22, 9, 7, c2);
        else frame(48 + k * 12, y + 22, 9, 7, 1, on ? BLACK : DIM);
      }
      num(b, sizeof b, i == 1 ? "%u DOTS" : "%u S", tm_powerup_secs[i - 1][lv]);
      text(b, 160, y + 20, c);
      if (lv < 9) num(b, sizeof b, "%u", tm_upgrade_cost[lv]), rtext(b, 306, y + 12, c2);
      else rtext("MAX", 306, y + 12, c2);
    }
  }
}

static void mask_line(int m, char *b, size_t n) {
  /* the mask's features, in words */
  b[0] = 0;
  for (int k = 0; k < 3; k++) {
    int f = tm_mask_feature[m][k][0], val = tm_mask_feature[m][k][1];
    char part[32];
    static const char *const what[9] = {"", "COINS", "COIN ADDICTS", "FREEZES", "MAGNETS", "BOOSTERS", "BONUS ZONES", "NO SNAKES", "NO MONKEYS"};
    if (!f) continue;
    if (f >= 7) snprintf(part, sizeof part, "%s", what[f]);
    else snprintf(part, sizeof part, "%s +%d%%", what[f], val);
    if (b[0]) strncat(b, ", ", n - strlen(b) - 1);
    strncat(b, part, n - strlen(b) - 1);
  }
  if (!b[0]) snprintf(b, n, "NO POWERS");
}

static void masks(const tm_game *g, const tm_ui *v) {
  fill(0, 0, 320, 240, BLACK);
  text("MASKS", 8, 4, YELLOW);
  char b[96];
  num(b, sizeof b, "%u", g->p.coins);
  rtext(b, 312, 4, YELLOW);
  spr(S_COIN_ICON, 312 - text_w(b) - 8, 8, 0, false, YELLOW);
  for (int m = 0; m < 16; m++) {
    int x = 24 + (m % 8) * 35, y = 22 + (m / 8) * 42;
    bool on = v->mask_sel == m, have = g->p.masks >> m & 1, open = tm_mask_open(&g->p, m);
    if (on) frame(x - 4, y - 4, 38, 42, 3, YELLOW);
    else if (m == g->p.mask) frame(x - 2, y - 2, 34, 38, 1, YELLOW);
    int id = S_CHAR0_IDLE + m * (S_CHAR1_ARRIVE - S_CHAR0_ARRIVE);
    spr_at(id, x + 1, y + 1, 0, false, have ? YELLOW : open ? WHITE : DIM, 2);
    if (!have && !open) spr(S_LOCK, x + 24, y + 28, 0, false, DIM);
  }
  int m = v->mask_sel;
  bool have = g->p.masks >> m & 1, open = tm_mask_open(&g->p, m);
  panel(6, 114, 308, 104);
  ctext(tm_mask_name[m], 160, 122, YELLOW);
  mask_line(m, b, sizeof b);
  wrap(b, 14, 140, 292, 13, WHITE, true);
  if (m == g->p.mask) snprintf(b, sizeof b, "WEARING IT");
  else if (have) snprintf(b, sizeof b, "YOURS");
  else if (!open) snprintf(b, sizeof b, "OPENS AT LEVEL %u", tm_mask_level[m]);
  else snprintf(b, sizeof b, "%u COINS", tm_mask_cost[m]);
  ctext(b, 160, 196, have || open ? YELLOW : DIM);
}

static void mission_text(int type, unsigned goal, char *b, size_t n) {
  switch (type) {
    case 0: snprintf(b, n, "COMPLETE STAGE %u", goal); break;
    case 1: snprintf(b, n, goal == 1 ? "PLAY THE ARCADE ONCE" : "PLAY THE ARCADE %u TIMES", goal); break;
    case 2: snprintf(b, n, "COLLECT %u DOTS", goal); break;
    case 3: snprintf(b, n, "COLLECT %u STARS", goal); break;
    case 4: snprintf(b, n, "STAGE %u WITHOUT A REVIVE", goal); break;
    case 5: snprintf(b, n, "ALL DOTS IN %u STAGES", goal); break;
    case 6: snprintf(b, n, "SCORE %u IN ONE RUN", goal); break;
    default: snprintf(b, n, "SPIN THE WHEEL %u TIMES", goal); break;
  }
}

static void missions(const tm_game *g, const tm_ui *v) {
  fill(0, 0, 320, 240, BLACK);
  text("MISSIONS", 8, 4, YELLOW);
  char b[48];
  static const int icon[8] = {0, 1, 2, 3, 4, 5, 6, 0};
  for (int i = 0; i < 3; i++) {
    int y = 24 + i * 64, t = g->p.mission_type[i];
    bool on = v->mission_sel == i, ready = tm_mission_ready(&g->p, i);
    if (on) fill(6, y, 308, 58, YELLOW);
    else frame(6, y, 308, 58, 1, ready ? YELLOW : SHADE);
    uint16_t c = on ? BLACK : WHITE, c2 = on ? BLACK : YELLOW;
    ink_only = on;
    spr(t == 7 ? S_CHEST : S_MISSION_ICONS + icon[t], 30, y + 29, 0, false, c2);
    unsigned goal = tm_mission_goal_of(&g->p, i);
    mission_text(t, goal, b, sizeof b);
    wrap(b, 58, y + 6, 250, 12, c, false);
    if (tm_mission_kind(t) == 2) snprintf(b, sizeof b, ready ? "DONE" : "NOT YET");
    else snprintf(b, sizeof b, "%u/%u", (unsigned)tm_mission_shown(&g->p, i), goal);
    text(b, 58, y + 40, c2);
    num(b, sizeof b, "+%u", tm_mission_reward[g->p.mission_index[i]]);
    int w = text_w(b);
    rtext(b, 306, y + 40, c2);
    spr(S_COIN_ICON, 306 - w - 8, y + 44, 0, false, c2);
    if (ready) rtext("READY", 306, y + 6, c2);
    ink_only = false;
  }
}

/* the wheel: eight prizes round the middle, turned, and the pointer on top */
static void chest(const tm_game *g, const tm_ui *v) {
  fill(0, 0, 320, 240, BLACK);
  text("CHEST", 8, 4, YELLOW);
  char b[40];
  num(b, sizeof b, "%u", g->p.coins);
  rtext(b, 312, 4, YELLOW);
  spr(S_COIN_ICON, 312 - text_w(b) - 8, 8, 0, false, YELLOW);
  static const char *const prize[8] = {"25", "200", "25", "BOX", "25", "250", "25", "SHIELD"};
  const tm_wheel *w = &v->wheel;
  float turn = (float)w->angle / 1024.0f;
  int cx = 160, cy = 118;
  spr(S_WHEEL_MIDDLE, cx, cy, 0, false, YELLOW);
  for (int s = 0; s < 8; s++) {
    /* sector s is under the pointer (on top) when the wheel has turned by s * 45 + 22.5 degrees */
    float a = (s * 45.0f + 22.5f + turn) * 3.14159265f / 180.0f;
    int x = cx + (int)(sinf(a) * 70.0f), y = cy - (int)(cosf(a) * 70.0f);
    bool won = w->done && w->sector == s;
    if (won) fill(x - 24, y - 8, 48, 16, YELLOW);
    ctext(prize[s], x, y - 4, won ? BLACK : s == 3 || s == 7 ? YELLOW : WHITE);
  }
  for (int i = 0; i < 8; i++) {
    float a = (i * 45.0f + turn) * 3.14159265f / 180.0f;
    fill(cx + (int)(sinf(a) * 56.0f) - 1, cy - (int)(cosf(a) * 56.0f) - 1, 3, 3, YELLOW);
  }
  /* the pointer */
  for (int k = 0; k < 6; k++) fill(cx - 6 + k, cy - 98 + k, 12 - 2 * k, 1, WHITE);
  if (w->done) {
    if (w->kind == 1) snprintf(b, sizeof b, "THE %s MASK", tm_mask_name[w->amount]);
    else if (w->kind == 3) snprintf(b, sizeof b, "A SHIELD");
    else snprintf(b, sizeof b, "%u COINS", (unsigned)w->amount);
    ctext(b, 160, 204, YELLOW);
  } else if (!w->spinning) {
    if (tm_chest_free(&g->p)) snprintf(b, sizeof b, "FREE SPIN");
    else {
      unsigned s = g->p.chest_wait;
      snprintf(b, sizeof b, "%u COINS (FREE IN %u:%02u OF PLAY)", TM_CHEST_COST, s / 3600, s / 60 % 60);
    }
    ctext(b, 160, 204, YELLOW);
  }
}

/* ------------------------------------------------------------------ in-game windows */
static void pause_menu(const tm_ui *v) {
  static const char *const items[PAUSE_COUNT] = {"RESUME", "RESTART", "HOW TO PLAY", "QUIT GAME"};
  panel(70, 60, 180, 100);
  ctext("PAUSED", 160, 68, YELLOW);
  for (int i = 0; i < PAUSE_COUNT; i++) row(78, 84 + i * 18, 164, v->sel == i, items[i], NULL);
}

static void quit_box(const tm_ui *v) {
  panel(60, 80, 200, 70);
  ctext("QUIT THIS RUN?", 160, 90, YELLOW);
  row(80, 112, 70, v->sel == 0, "NO", NULL);
  row(170, 112, 70, v->sel == 1, "YES", NULL);
}

static const char *const help[TM_HELP_PAGES] = {
  "ARROWS DASH. YOU FLY UNTIL A WALL STOPS YOU, THEN DASH AGAIN.\n"
  "YOU CANNOT DASH INTO THE WALL YOU STAND ON.\n"
  "DOTS GIVE POINTS. COINS PAY FOR THE SHOP, MASKS AND CHESTS.",
  "ARCADE: CLIMB, THE LAVA RISES BEHIND YOU.\n"
  "TAKE POWERUPS: COIN ADDICT, FREEZE, MAGNET, SCORE BOOST.\n"
  "OK USES A SHIELD FROM THE SHOP: IT TAKES ONE HIT.",
  "STAGES: REACH THE EXIT. TAKE THE THREE STARS.\n"
  "ALL THE DOTS OF A STAGE PAY 25 COINS (EVERY THIRD STAGE A SHIELD).\n"
  "BACK PAUSES. HOME SAVES AND QUITS."};

static void help_box(const tm_ui *v) {
  panel(10, 24, 300, 192);
  char b[24];
  snprintf(b, sizeof b, "HOW TO PLAY %u/%u", (unsigned)v->page + 1, TM_HELP_PAGES);
  ctext(b, 160, 32, YELLOW);
  wrap(help[v->page], 20, 52, 280, 14, WHITE, false);
}

static void revive_box(const tm_game *g, const tm_ui *v) {
  panel(50, 60, 220, 110);
  ctext("REVIVE?", 160, 70, YELLOW);
  char b[40];
  snprintf(b, sizeof b, "200 COINS (YOU HAVE %u)", (unsigned)g->p.coins);
  ctext(b, 160, 90, g->p.coins >= 200 ? WHITE : DIM);
  int left = tm_revives_left(g);
  snprintf(b, sizeof b, left == 1 ? "%d REVIVE LEFT" : "%d REVIVES LEFT", left);
  ctext(b, 160, 106, WHITE);
  int w = (int)(v->revive_left * 196 / 5000);
  frame(60, 126, 200, 8, 1, DIM), fill(62, 128, w, 4, YELLOW);
  if (g->p.coins < 200) ctext("NOT ENOUGH COINS", 160, 146, DIM);
}

static void results(const tm_game *g, const tm_ui *v) {
  panel(30, 30, 260, 180);
  char b[48];
  int y = 40;
  if (g->mode == MODE_ARCADE) {
    ctext("GAME OVER", 160, y, YELLOW);
    snprintf(b, sizeof b, "SCORE %u", (unsigned)g->score);
    ctext(b, 160, y += 24, WHITE);
    snprintf(b, sizeof b, g->score >= g->p.best ? "NEW BEST" : "BEST %u", (unsigned)g->p.best);
    ctext(b, 160, y += 16, YELLOW);
  } else {
    snprintf(b, sizeof b, g->ev_won ? "STAGE %u CLEAR" : "STAGE %u FAILED", (unsigned)g->stage + 1);
    ctext(b, 160, y, YELLOW);
    int n = g->stars_all;
    for (int i = 0; i < n; i++) spr(S_STAR_BIG, 160 - (n - 1) * 20 + i * 40, y + 34, 0, false, i < g->stars ? YELLOW : SHADE);
    y += 48;
    snprintf(b, sizeof b, "DOTS %u/%u", g->dots, g->dots_all);
    ctext(b, 160, y += 8, WHITE);
  }
  snprintf(b, sizeof b, "COINS +%u", (unsigned)(g->p.coins >= v->run_coins ? g->p.coins - v->run_coins : 0));
  ctext(b, 160, y += 16, WHITE);
  if (g->ev_reward == 1) ctext("ALL DOTS: +25 COINS", 160, y += 16, YELLOW);
  if (g->ev_reward == 2) ctext("ALL DOTS: +1 SHIELD", 160, y += 16, YELLOW);
  unsigned lv = tm_level(g->p.xp);
  snprintf(b, sizeof b, "LEVEL %u", lv);
  ctext(b, 160, y += 20, WHITE);
  if (lv < 20) {
    uint32_t a = tm_level_xp[lv - 1], z = tm_level_xp[lv];
    int w = (int)((uint64_t)(g->p.xp - a) * 196 / (z - a));
    frame(60, y + 14, 200, 8, 1, DIM), fill(62, y + 16, w, 4, YELLOW);
  }
}

/* ------------------------------------------------------------------ the screen */
static const char *keys_for(const tm_game *g, const tm_ui *v, char *b, size_t n) {
  switch (v->screen) {
    case V_TITLE: return "UP DOWN CHOOSE  OK OPEN  BACK QUIT";
    case V_STAGES: return "ARROWS CHOOSE  OK PLAY  BACK MENU";
    case V_PLAY:
      snprintf(b, n, "ARROWS DASH  OK SHIELD (%u)  BACK PAUSE", g->p.shields);
      return b;
    case V_PAUSE: return "UP DOWN CHOOSE  OK SELECT  BACK RESUME";
    case V_QUIT: return "LEFT RIGHT CHOOSE  OK SELECT";
    case V_HELP: return v->first_run ? "LEFT RIGHT PAGES  OK PLAY" : "LEFT RIGHT PAGES  BACK CLOSE";
    case V_REVIVE: return g->p.coins >= 200 ? "OK REVIVE  BACK GIVE UP" : "BACK GIVE UP";
    case V_RESULTS:
      return g->mode == MODE_STAGE && g->ev_won && g->stage < 199 ? "OK NEXT STAGE  BACK MAP" :
             g->mode == MODE_STAGE ? "OK AGAIN  BACK MAP" : "OK AGAIN  BACK MENU";
    case V_SHOP: return "UP DOWN CHOOSE  OK BUY  BACK MENU";
    case V_MASKS: {
      int m = v->mask_sel;
      if (g->p.masks >> m & 1) return m == g->p.mask ? "ARROWS CHOOSE  BACK MENU" : "ARROWS CHOOSE  OK WEAR  BACK MENU";
      return "ARROWS CHOOSE  OK BUY  BACK MENU";
    }
    case V_MISSIONS: return "UP DOWN CHOOSE  OK CLAIM  BACK MENU";
    default:
      if (v->wheel.spinning) return "THE WHEEL IS TURNING";
      return v->wheel.done ? "OK SPIN AGAIN  BACK MENU" : "OK SPIN  BACK MENU";
  }
}

void tm_draw(const tm_game *g, const tm_ui *v, tm_push_fn push) {
  now_ms = v->clock;
  bool in_game = v->screen == V_PLAY || v->screen == V_PAUSE || v->screen == V_QUIT || v->screen == V_REVIVE ||
                 v->screen == V_RESULTS || (v->screen == V_HELP && v->under != V_TITLE);
  if (in_game) {
    bool frozen = g->pw_time[PW_FREEZE] > 0;
    int ci = g->colors % 13;
    c_main = frozen ? GREY : tm_colors[0][ci], c_sub = frozen ? GREY : tm_colors[1][ci];
    c_wall = pulse(c_main, c_sub, g->tick, 30);
    c_dot = pulse(YELLOW, c_main, g->tick, 12);
    c_enemy = frozen ? GREY : GREEN;
    uint32_t sp = g->tick % 36;
    c_spike = frozen ? GREY : sp < 12 ? mix(c_main, GREEN, (int)(sp * 256 / 12)) : c_main;
    c_lava = pulse(GREEN, c_main, g->tick, 12);
    cam_px = to_px(g->cam_x), cam_py = to_px(g->cam_y);
    shake_x = shake_y = 0;
    if (g->shake > 0) {
      int s = g->shake > 4 ? 2 : 1;
      shake_x = g->tick & 2 ? s : -s, shake_y = g->tick & 4 ? s : -s;
    }
  }
  char kb[64];
  const char *keys = keys_for(g, v, kb, sizeof kb);
  for (band = 0; band < 240; band += BAND) {
    if (in_game) {
      memset(strip, 0, sizeof strip);
      world(g);
      bar(g);
    }
    switch (v->screen) {
      case V_TITLE: title(g, v); break;
      case V_STAGES: stages(g, v); break;
      case V_SHOP: shop(g, v); break;
      case V_MASKS: masks(g, v); break;
      case V_MISSIONS: missions(g, v); break;
      case V_CHEST: chest(g, v); break;
      case V_PAUSE: pause_menu(v); break;
      case V_QUIT: quit_box(v); break;
      case V_HELP:
        if (v->under == V_TITLE) fill(0, 0, 320, 240, BLACK);
        help_box(v);
        break;
      case V_REVIVE: revive_box(g, v); break;
      case V_RESULTS: results(g, v); break;
      default: break;
    }
    keyline(keys);
    push(0, band, 320, BAND, strip);
  }
}
