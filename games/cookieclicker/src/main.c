/* Cookie Clicker: Orteil's idle game, for the NumWorks calculator.
 *
 * Click the big cookie, buy buildings that bake for you, then upgrades that make them bake more,
 * and catch the golden cookies. The numbers are the game's own (version 2.05): its 20 buildings
 * with their prices and cookies per second, each one 15% dearer than the last, the tiered
 * upgrades that double a building, the cursor's thousand fingers, the mice, flavored cookies,
 * kittens that turn milk (achievements) into cookies, and golden cookies with Frenzy, Lucky and
 * Click frenzy.
 *
 * The bank is counted in doubles, as the game does; prices and rates in floats, whose 7 digits
 * are more than the game shows (1.234 million). Inside NumPlay it draws with the launcher's
 * strips, shapes and fonts and unpacks its names and news with the launcher's DEFLATE decoder;
 * on its own (CookieClicker.nwa) it brings them along. */
#include <eadk.h>
#include "../../../launcher/src/gfx.h"
#include "../../../launcher/src/inflate.h"
#include "../../common/np_text.h"

#if NP_MODULE /* (np_file_read and np_file_write: NumPlay's, launcher/src/np.h) */
#define np_app_begin()
#define np_app_end() 0
#else
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#define np_file_read ef_read
#define np_file_write ef_write
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Cookie Clicker";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
void np_push(int x, int y, int w, int h, const color_t *px) {
  eadk_display_push_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, px);
}
#endif
#include "data.h"

/* the keys, in 32 bits: those from 36 on come down 23 */
#define K(k) (1u << ((k) < 32 ? (k) : (k) - 23))
static uint32_t keys(void) {
  uint64_t s = eadk_keyboard_scan();
  return ((uint32_t)s & 0x80001FFFu) | ((uint32_t)(s >> 32) & 0x1FFFF0u) << 9;
}
#define WHITE 0xFFFF
#define CREAM HEX(0xEDE3D3)
#define GOLD HEX(0xFFD966)
#define WOOD HEX(0x3A2716)
#define DARK HEX(0x24170C)
#define NB 20
#define LW 144 /* the left panel, the cookie's; the store is right of it */
#define SX (LW + 2)
#define CX 72 /* the big cookie */
#define CY 128
#define INFO 190 /* what the selected thing does */

static const char *const bname[NB] = {
  T("Cursor"), T("Grandma"), T("Farm"), T("Mine"), T("Factory"), T("Bank"), T("Temple"), T("Wizard tower"),
  T("Shipment"), T("Alchemy lab"), T("Portal"), T("Time machine"), T("Antimatter condenser"), T("Prism"),
  T("Chancemaker"), T("Fractal engine"), T("Javascript console"), T("Idleverse"), T("Cortex baker"), T("You")};
/* each building's price and cookies per second, as two digits and a power of ten */
static const int8_t bdata[NB][4] = {
  {15, 0, 10, -2}, {10, 1, 10, -1}, {11, 2, 80, -1}, {12, 3, 47, 0}, {13, 4, 26, 1}, {14, 5, 14, 2},
  {20, 6, 78, 2}, {33, 7, 44, 3}, {51, 8, 26, 4}, {75, 9, 16, 5}, {10, 11, 10, 6}, {14, 12, 65, 6},
  {17, 13, 43, 7}, {21, 14, 29, 8}, {26, 15, 21, 9}, {31, 16, 15, 10}, {71, 18, 11, 11}, {12, 21, 83, 11},
  {19, 23, 64, 12}, {54, 25, 51, 13}};
/* the upgrade tiers' colours (Plain, Berrylium, Blueberrylium...), the milks' */
static const uint32_t tier_rgb[10] = {0xCCB3AC, 0xFF89E7, 0x00DEFF, 0xFFCC2F, 0xE9D673,
                                      0xA8BF91, 0x60FF50, 0xF01700, 0x9AB834, 0x7E7AB9};
static const uint32_t milk_rgb[N_MILK] = {0xF4F1EA, 0x8B5A33, 0xF06C93};

/* ------------------------------------------------------------ the bakery (cookieclicker.sav) */
static struct {
  uint8_t magic, version, flags, mode; /* flags: 1 the guide was read; mode: buy 1, 10 or 100 (0-2) */
  uint16_t amount[NB];
  uint32_t clicks, golden;
  uint32_t gc_t, frenzy, cfrenzy;      /* ms since the last golden cookie; what is left of each effect */
  double cookies, earned, handmade;
  uint8_t up[(N_UP + 7) / 8], unl[(N_UP + 7) / 8], ach[(N_ACH + 7) / 8];
} sv;
#define SAVE_NAME "cookieclicker.sav"

static union { /* the inflater's tables while the texts are unpacked, the strip after */
  color_t strip[SCREEN_W * STRIP_H];
  uint8_t tables[3400];
} ram;
static char text[TEXT_SIZE], buf[160], pop[48];
static uint16_t text_at[N_TEXT];
static uint8_t list[N_UP];
static float cps, raw_cps, cpc, each[NB], milk;
static int nach, nup, owned, nlist, tab = 1, sel[3], top[3], ov, osel, toast = -1, gc_x, gc_y, news_x, news_w;
static uint32_t now, seed, toast_t, news_n, gc_life, gc_dur, pop_t, bounce_t;
static struct {
  int16_t x, y;
  uint32_t t;
  char s[32];
} rises[4];

static uint32_t rnd(void) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  return seed ^= seed << 5;
}
static bool has(const uint8_t *bits, int i) { return bits[i >> 3] >> (i & 7) & 1; }
static void set(uint8_t *bits, int i) { bits[i >> 3] |= (uint8_t)(1 << (i & 7)); }
static const char *txt(int i) { return text + text_at[i]; }
static float powi(float x, int n) {
  float r = 1;
  for (; n; n >>= 1, x *= x)
    if (n & 1) r *= x;
  return r;
}
static float p10(int e) { return e < 0 ? 1 / powi(10, -e) : powi(10, e); }
static float base_price(int b) { return bdata[b][0] * p10(bdata[b][1]); }
static int many(void) { return sv.mode == 2 ? 100 : sv.mode == 1 ? 10 : 1; }
/* the price of n more buildings b: 15% more for each one owned, rounded up */
static float price(int b, int n) {
  float p = base_price(b) * powi(1.15f, sv.amount[b]);
  return __builtin_ceilf((n > 1 ? p * (powi(1.15f, n) - 1) / 0.15f : p) * 0.999999f);
}

/* ------------------------------------------------------------ numbers, as the game writes them */
static const char *const units[12] = {
  T(" million"), T(" billion"), T(" trillion"), T(" quadrillion"), T(" quintillion"), T(" sextillion"),
  T(" septillion"), T(" octillion"), T(" nonillion"), T(" decillion"), T(" undecillion"), T(" duodecillion")};
static char *cpy(char *o, const char *s) {
  while ((*o = *s++)) o++;
  return o;
}
static char *num(char *o, uint32_t v) { /* with commas: 1,234,567 */
  char t[14];
  int n = 0;
  do {
    if (n % 4 == 3) t[n++] = ',';
    t[n++] = (char)('0' + v % 10);
  } while (v /= 10);
  while (n) *o++ = t[--n];
  *o = 0;
  return o;
}
/* 0.1 and 12.3 (a decimal when there is one, below 1,000), 123,456, then 1.234 million, 12.5 billion... */
static char *big(char *o, double v) {
  int u = 0;
  if (v < 1e6) {
    uint32_t t = (uint32_t)(v * 10 + .5);
    if (v >= 1000 || t % 10 == 0) return num(o, v >= 1000 ? (uint32_t)v : t / 10);
    o = num(o, t / 10);
    *o++ = '.', *o++ = (char)('0' + t % 10), *o = 0;
    return o;
  }
  for (v /= 1e6; v >= 999.9995 && u < 11; u++) v /= 1000;
  uint32_t m = (uint32_t)(v * 1000 + .5);
  o = num(o, m / 1000);
  if ((m %= 1000)) {
    *o++ = '.';
    for (int d = 100; m; d /= 10) *o++ = (char)('0' + m / d), m %= d;
  }
  return cpy(o, units[u]);
}
static char *cat(char *o, const char *a, double v, const char *b) { return cpy(big(cpy(o, a), v), b); }

/* ------------------------------------------------------------ cookies per second */
static int bought(int from, int n) {
  int k = 0;
  while (n--) k += has(sv.up, from++);
  return k;
}
/* the cursor's thousand fingers: what each building that isn't a cursor adds to it and to a click */
static float fingers(void) {
  float add = has(sv.up, 3) ? 0.1f : 0;
  for (int t = 4; t < N_CURSOR; t++)
    if (has(sv.up, t)) add *= t == 4 ? 5 : t == 5 ? 10 : 20;
  return add * (float)(owned - sv.amount[0]);
}
static void compute(void) {
  owned = 0, nup = bought(0, N_UP), nach = 0;
  for (int b = 0; b < NB; b++) owned += sv.amount[b];
  for (int i = 0; i < N_ACH; i++) nach += has(sv.ach, i);
  milk = (float)nach / 25;
  float mult = 1, total = 0, clicks = (float)(1 << bought(0, 3)), frenzy = sv.frenzy ? 7 : 1;
  for (int i = 0; i < N_COOKIE; i++)
    if (has(sv.up, U_COOKIE + i)) mult *= 1 + (float)(cookie_up[i] >> 5) / 100;
  for (int i = 0; i < N_KITTEN; i++) /* +10%, 12.5%, 15% of the milk */
    if (has(sv.up, U_KITTEN + i)) mult *= 1 + milk * (float)(4 + i) / 40;
  for (int b = 0; b < NB; b++) {
    float c = bdata[b][2] * p10(bdata[b][3]);
    if (b == 0) c = c * clicks + fingers();
    else c *= (float)(1 << bought(U_TIER + (b - 1) * TIER_N, TIER_N));
    each[b] = c * mult * frenzy;
    total += sv.amount[b] * c;
  }
  raw_cps = total * mult, cps = raw_cps * frenzy;
  cpc = (clicks + fingers() + (float)bought(U_MOUSE, N_MOUSE) * cps / 100) * (sv.cfrenzy ? 777 : 1);
}

/* ------------------------------------------------------------ upgrades */
static const uint8_t cursor_at[N_CURSOR] = {1, 1, 10, 25, 50, 100, 150, 200};
static const uint8_t cursor_e[N_CURSOR] = {2, 2, 4, 5, 7, 8, 9, 10}, kitten_e[3] = {6, 9, 13};
static const uint8_t tier_e[TIER_N] = {1, 1, 2}, tier_at[TIER_N] = {1, 5, 25}, golden_at[3] = {7, 27, 77};

static float up_price(int u) {
  if (u < U_MOUSE) return (u == 1 ? 5 : 1) * p10(cursor_e[u]);
  if (u < U_KITTEN) return 5 * p10(4 + 2 * (u - U_MOUSE));
  if (u < U_GOLD) return 9 * p10(kitten_e[u - U_KITTEN]);
  if (u < U_COOKIE) return 7 * (p10(9 + 2 * (u - U_GOLD) + (u == U_GOLD + 2)) - 1) / 9; /* 777,777,777... */
  if (u < U_TIER) { /* 999,999 or 4,999,995... */
    int c = cookie_up[u - U_COOKIE];
    return (p10((c & 15) + 5) - 1) * (c & 16 ? 5 : 1);
  }
  int t = (u - U_TIER) % TIER_N; /* 10, 50, 500, 50,000... times its building's price */
  return base_price(1 + (u - U_TIER) / TIER_N) * p10(tier_e[t]) * (t ? 5 : 1);
}
static bool up_ready(int u) { /* unlocked by what the bakery has done */
  if (u < U_MOUSE) return sv.amount[0] >= cursor_at[u];
  if (u < U_KITTEN) return sv.handmade >= p10(3 + 2 * (u - U_MOUSE));
  if (u < U_GOLD) return milk >= (u == U_KITTEN ? 0.5f : (float)(u - U_KITTEN));
  if (u < U_COOKIE) return sv.golden >= golden_at[u - U_GOLD];
  if (u < U_TIER) return sv.earned * 20 >= up_price(u);
  return sv.amount[1 + (u - U_TIER) / TIER_N] >= tier_at[(u - U_TIER) % TIER_N];
}
static void up_desc(int u) { /* what it does, in buf */
  if (u < 3) cpy(buf, T("The mouse and cursors are twice as efficient."));
  else if (u == 3) cpy(buf, T("The mouse and cursors gain +0.1 cookies for each other building."));
  else if (u < U_MOUSE) cat(buf, T("Thousand fingers x"), u == 4 ? 5 : u == 5 ? 10 : 20, "");
  else if (u < U_KITTEN) cpy(buf, T("Clicking gains +1% of your CpS."));
  else if (u < U_GOLD) cpy(buf, T("More CpS the more milk you have."));
  else if (u < U_COOKIE) cpy(buf, u < U_GOLD + 2 ? T("Golden cookies come twice as often, stay twice as long.")
                                               : T("Golden cookie effects last twice as long."));
  else if (u < U_TIER) cat(buf, T("Cookie production +"), cookie_up[u - U_COOKIE] >> 5, "%");
  else cpy(cpy(buf, bname[1 + (u - U_TIER) / TIER_N]), T(": twice as efficient."));
}
static void sort_list(void) { /* the store's upgrades: unlocked, not bought yet, cheapest first */
  nlist = 0;
  for (int u = 0; u < N_UP; u++) {
    if (!has(sv.unl, u) || has(sv.up, u)) continue;
    int i = nlist++;
    float p = up_price(u);
    while (i && up_price(list[i - 1]) > p) list[i] = list[i - 1], i--;
    list[i] = (uint8_t)u;
  }
}

/* ------------------------------------------------------------ achievements */
/* their kind: baked, per second, from clicking, buildings, golden cookies, owned, upgrades; where each
   kind starts; the counts they ask for */
static const uint8_t kinds[6] = {A_CPS, A_CLICK, A_BUILD, A_GOLDEN, A_OWNED, A_UPGRADED};
static const uint16_t counts[] = {1, 2, 50, 1, 7, 27, 77, 100, 500, 20, 50};
static int threshold(int a, float *v) {
  static const uint8_t first[7] = {0, 0, 0, 0, 3, 7, 9}; /* (in counts) */
  int k = 0, i;
  while (k < 6 && a >= kinds[k]) k++;
  i = a - (k ? kinds[k - 1] : 0);
  *v = k == 0 ? (a ? p10(3 * a / 2 + 2) : 1) : k == 1 ? p10(6 * i / 5) : k == 2 ? p10(3 + 2 * i)
     : k == 3 ? counts[i < N_CURSOR_ACH ? i : 0] : counts[first[k] + i];
  return k;
}
static int ach_building(int a) { return a - A_BUILD < N_CURSOR_ACH ? 0 : a - A_BUILD - N_CURSOR_ACH + 1; }
static bool ach_won(int a) {
  float v;
  int k = threshold(a, &v);
  return (k == 0 ? sv.earned : k == 2 ? sv.handmade : k == 1 ? raw_cps : k == 3 ? sv.amount[ach_building(a)]
          : k == 4 ? (double)sv.golden : k == 5 ? owned : nup) >= v;
}
static void win(int a) {
  if (has(sv.ach, a)) return;
  set(sv.ach, a);
  toast = a, toast_t = now;
}
static void ach_desc(int a) { /* what it takes, in buf */
  static const char *const pre[7] = {T("Bake "), T("Bake "), T("Make "), T("Have "), T("Click "), T("Own "), T("Purchase ")};
  static const char *const post[7] = {T(" cookies."), T(" cookies per second."),
                                      T(" cookies from clicking."), " x ", T(" golden cookies."), T(" buildings."),
                                      T(" upgrades.")};
  float v;
  int k = threshold(a, &v);
  cpy(cat(buf, pre[k], v, post[k]), k == 3 ? bname[ach_building(a)] : "");
  if (v == 1) /* 1 cookie, 1 golden cookie: the plural s goes (none of the verbs ends in one) */
    for (char *s = buf, *d = buf; (*d = *s); s++) d += !(*s == 's' && (s[1] == ' ' || s[1] == '.'));
}

static void check(void) { /* once a second, and after a purchase: what unlocks, what is won */
  compute();
  for (int a = 0; a < N_ACH; a++)
    if (ach_won(a)) win(a);
  compute();
  for (int u = 0; u < N_UP; u++)
    if (!has(sv.unl, u) && up_ready(u)) set(sv.unl, u), nlist = -1;
  if (nlist < 0) sort_list();
}
static void save(void) {
  sv.magic = 'C', sv.version = 1;
  np_file_write(SAVE_NAME, &sv, sizeof sv);
}

/* ------------------------------------------------------------ playing */
static void earn(double v) { sv.cookies += v, sv.earned += v; }
static void click(void) {
  int i = sv.clicks++ & 3;
  earn(cpc);
  sv.handmade += cpc, bounce_t = now;
  rises[i].x = (int16_t)(CX - 30 + rnd() % 60), rises[i].y = (int16_t)(CY - 24 - rnd() % 24), rises[i].t = now;
  big(cpy(rises[i].s, "+"), cpc);
}
static void buy(void) {
  int s = sel[tab];
  float p = tab ? price(s, many()) : up_price(list[s]);
  if (sv.cookies < p) return;
  sv.cookies -= p;
  if (tab) sv.amount[s] = (uint16_t)(sv.amount[s] + many());
  else set(sv.up, list[s]), sort_list();
  check();
  save();
}

/* the golden cookie: once 5 to 15 minutes have gone by (half that with Lucky day, half again with
   Serendipity), it shows for 13 seconds (twice as long with each); then Frenzy or Lucky, sometimes
   Click frenzy */
static int luck(void) { return has(sv.up, U_GOLD) + has(sv.up, U_GOLD + 1); }
static void golden_pop(void) {
  uint32_t dur = has(sv.up, U_GOLD + 2) + 1, e = rnd() % 30;
  sv.golden++, gc_life = 0;
  if (e == 0) sv.cfrenzy = 13000 * dur;
  else if (e < 16) sv.frenzy = 77000 * dur;
  else {
    double v = sv.cookies * 0.15 < cps * 900.0 ? sv.cookies * 0.15 : cps * 900.0;
    earn(v + 13);
    cat(pop, T("Lucky! +"), v + 13, "");
  }
  pop_t = now;
  check();
  save();
}

/* ------------------------------------------------------------ drawing */
#define VIS(y, h) ((y) < gfx_y1 && (y) + (h) > gfx_y0)
static void say(const np_font_t *f, int x, int y, const char *s, color_t c) { gfx_text(f, x, y, s, c, 256); }
static void mid(const np_font_t *f, int x, int y, const char *s, color_t c) { gfx_text_center(f, x, y, s, c, 256); }
static int para(int y, const char *s, color_t c) { return gfx_paragraph(&np_font_small, SX + 87, y, 166, 12, s, c, 256); }
/* a cookie, r pixels round: golden or not */
static void cookie(int x, int y, int r, bool gold) {
  static const int8_t chips[9][3] = {{-22, -24, 9}, {14, -30, 7}, {30, -2, 9}, {-34, 6, 7}, {-8, 4, 10},
                                     {18, 22, 8}, {-16, 30, 8}, {2, -12, 5}, {34, 24, 5}};
  if (!VIS(y - r, 2 * r)) return;
  gfx_circle(x, y, r, gold ? HEX(0xC48A1C) : HEX(0x9C5B26), 256);
  gfx_circle(x, y, r - 1 - r / 16, gold ? HEX(0xF2C33C) : HEX(0xD9A25E), 256);
  gfx_circle(x - r / 4, y - r / 4, r / 2, WHITE, 40);
  for (int i = 0; i < 9; i++) {
    int cx = x + chips[i][0] * r / 52, cy = y + chips[i][1] * r / 52, cr = chips[i][2] * r / 52 + 1;
    gfx_circle(cx, cy, cr, gold ? HEX(0xB07A12) : HEX(0x4B2611), 256);
    gfx_circle(cx - cr / 3, cy - cr / 3, cr / 3, WHITE, 50);
  }
}

static void left(void) {
  gfx_fill(0, 16, LW, SCREEN_H - 16, HEX(0x22406A));
  gfx_circle(CX, CY, 70, WHITE, 24); /* the light behind the big cookie */
  uint32_t b = now - bounce_t; /* a little smaller for a moment when clicked */
  cookie(CX, CY, b < 240 ? 48 + (int)b / 60 : 52, false);
  /* the milk, which rises with the achievements and changes flavor every 25 */
  int h = 10 + (int)((milk > 1 ? 1 : milk) * 20), rank = nach / 25; /* (59 achievements: 3 milks) */
  for (int x = 0; x < LW; x++) {
    int w = (int)(now / 40 + x) % 48;
    gfx_fill(x, SCREEN_H - h + (w < 24 ? w : 48 - w) / 6, 1, h + 8, gfx_rgb(milk_rgb[rank]));
  }
  /* the cookies: "1.234 million", the number on its own line, "million cookies" under it; per second */
  if (VIS(16, 56)) {
    char *name = NULL, *line2 = buf + 80;
    big(buf, sv.cookies < 1e6 ? (double)(uint32_t)sv.cookies : sv.cookies); /* (whole cookies) */
    for (char *s = buf; *s && !name; s++)
      if (*s == ' ') *s = 0, name = s + 1;
    mid(&np_font_title, CX, 40, buf, WHITE);
    cpy(name ? cpy(cpy(line2, name), " ") : line2, T("cookies"));
    mid(&np_font_body, CX, 54, line2, WHITE);
    cat(buf, T("per second: "), cps, "");
    mid(&np_font_small, CX, 67, buf, CREAM);
  }
  /* what the golden cookie did, and for how long still; +1 rising from the cookie */
  uint32_t ms = sv.cfrenzy ? sv.cfrenzy : sv.frenzy;
  if (ms) num(cpy(buf, sv.cfrenzy ? T("Click frenzy x777: ") : T("Frenzy x7: ")), (ms + 999) / 1000);
  if (ms || now - pop_t < 2500) mid(&np_font_small, CX, 196, ms ? buf : pop, GOLD);
  for (int i = 0; i < 4; i++) {
    uint32_t t = now - rises[i].t;
    if (rises[i].t && t < 900)
      gfx_text_center(&np_font_body, rises[i].x, rises[i].y - (int)t / 20, rises[i].s, WHITE, 256 - (int)t * 2 / 7);
  }
  if (gc_life) { /* the golden cookie, fading in and out */
    float k = (float)gc_life / (float)gc_dur * 2 - 1;
    int a = (int)((1 - k * k * k * k) * 256);
    gfx_circle(gc_x, gc_y, 20, HEX(0xFFE680), a / 3);
    cookie(gc_x, gc_y, 14, true);
    gfx_text_center(&np_font_small, gc_x, gc_y + 26, "shift", WHITE, a);
  }
  if (toast >= 0 && now - toast_t < 3500) {
    gfx_rrect(4, 206, LW - 8, 30, 6, DARK, 230);
    mid(&np_font_small, CX, 218, T("Achievement unlocked"), HEX(0xC9B79C));
    mid(&np_font_small, CX, 231, txt(X_ACH + toast), WHITE);
  }
}

/* ------------------------------------------------------------ the store */
static void fit(const np_font_t *f, const char *s, int w) { /* s in buf, cut to w pixels */
  char *e = cpy(buf, s);
  while (gfx_text_width(f, buf) > w && e > buf + 4) cpy(--e, "...");
}
/* a picture: a building's (or a mouse, a kitten, a cookie) on its tier's colour */
static void picture(int u, int x, int y) {
  int t = u, k = IC_CURSOR;
  if (u >= U_MOUSE) t = u - U_MOUSE, k = IC_MOUSE;
  if (u >= U_KITTEN) t = u - U_KITTEN, k = IC_KITTEN;
  if (u >= U_TIER) t = (u - U_TIER) % TIER_N, k = 1 + (u - U_TIER) / TIER_N;
  if (u < 0) t = 3, k = -1 - u; /* (a building) */
  gfx_rrect(x, y, 18, 18, 4, gfx_rgb(tier_rgb[t]), 256);
  if (u >= U_GOLD && u < U_TIER) cookie(x + 9, y + 9, 7, u < U_COOKIE);
  else
    for (int i = 0; i < 144; i++)
      if ((uint8_t)text[ICONS_AT + 18 * k + (i >> 3)] >> (i & 7) & 1) gfx_fill(x + 3 + i % 12, y + 3 + i / 12, 1, 1, DARK);
}
static bool locked(int b) { return !sv.amount[b] && sv.earned < base_price(b); }
static int rows(void) {
  int n = 2;
  for (int b = 0; b < NB; b++)
    if (!locked(b)) n = b + 3; /* the next two buildings show as ??? */
  return tab == 0 ? nlist : tab == 1 ? (n < NB ? n : NB) : N_ACH;
}

static void store(void) {
  static const char *const tabs[3] = {T("Upgrades"), T("Buildings"), T("Stats")};
  gfx_fill(LW, 16, SCREEN_W - LW, SCREEN_H - 16, WOOD);
  /* the tabs; how many are bought at once, how many achievements are won */
  if (VIS(16, 30)) {
    for (int i = 0; i < 3; i++) {
      if (i == tab) gfx_rrect(SX + 1 + i * 58, 18, 56, 14, 5, HEX(0x8A6238), 256);
      mid(&np_font_small, SX + 29 + i * 58, 29, tabs[i], i == tab ? WHITE : HEX(0xB79C7C));
    }
    *buf = 0;
    if (tab == 1) num(cpy(buf, T("Buy x")), (uint32_t)many());
    if (tab == 2) cpy(num(cpy(num(buf, (uint32_t)nach), " / "), N_ACH), T(" achievements"));
    mid(&np_font_small, SX + 87, 42, buf, HEX(0x9CF29C));
  }
  /* the list */
  int n = rows(), h = tab == 2 ? 12 : 24, s = sel[tab];
  for (int r = 0; r < 144 / h && top[tab] + r < n; r++) {
    int i = top[tab] + r, y = 46 + r * h;
    if (!VIS(y, h)) continue;
    gfx_fill(SX, y, SCREEN_W - SX, h - 1, i == s ? HEX(0x6E4D2E) : tab == 2 ? WOOD : HEX(0x4C331E));
    if (tab == 2) {
      fit(&np_font_small, txt(X_ACH + i), 166);
      say(&np_font_small, SX + 4, y + 9, buf, has(sv.ach, i) ? WHITE : HEX(0x8C7A66));
      continue;
    }
    float p;
    if (tab == 1) {
      if (sv.amount[i]) num(buf, sv.amount[i]), gfx_text_right(&np_font_title, SCREEN_W - 4, y + 20, buf, WHITE, 70);
      if (!locked(i)) picture(-1 - i, SX + 3, y + 3);
      cpy(buf, locked(i) ? "???" : bname[i]);
      p = price(i, many());
    } else {
      picture(list[i], SX + 3, y + 3);
      fit(&np_font_body, txt(list[i]), SCREEN_W - SX - 30);
      p = up_price(list[i]);
    }
    say(&np_font_body, SX + 25, y + 11, buf, WHITE);
    big(buf, p);
    say(&np_font_small, SX + 25, y + 21, buf, sv.cookies >= p ? HEX(0x6CF26C) : HEX(0xFF6B6B));
  }
  /* what the selected one does */
  if (!VIS(INFO, SCREEN_H - INFO)) return;
  gfx_fill(SX, INFO, SCREEN_W - SX, SCREEN_H - INFO, DARK);
  int y = INFO + 14;
  *buf = 0;
  if (s >= n || (tab == 1 && locked(s))) return;
  if (tab == 1)
    cat(num(cpy(cat(buf, T("Each makes "), each[s], T(" cookies per second.")), "\n"), sv.amount[s]), T(" make "),
        each[s] * sv.amount[s], "");
  else if (tab == 0) y = para(y, txt(list[s]), GOLD), up_desc(list[s]);
  else y = para(y, txt(X_ACH + s), GOLD), ach_desc(s);
  para(y, buf, CREAM);
}

/* ------------------------------------------------------------ the news, and the menus */
static void next_news(void) { /* how far the bakery's fame has gone: the digits of a tenth of all it baked */
  int l = 0;
  while (l < N_LORE - 1 && sv.earned >= 10 * p10(l)) l++;
  news_n = (uint32_t)(X_NEWS + l);
  news_x = SCREEN_W, news_w = gfx_text_width(&np_font_small, txt(news_n));
}

enum { OV_NONE, OV_MENU, OV_HELP, OV_QUIT };
static const char *const menu_items[3] = {T("Resume"), T("How to play"), T("Quit game")};
static const char *const help_keys[12] = {
  "OK", T("click the cookie"), "shift", T("catch a golden cookie"), T("Arrows"), T("choose, change tabs"),
  "EXE", T("buy"), "x", T("buy 1, 10 or 100"), T("Back"), T("menu")};
static void button(int y, const char *s, bool on) {
  gfx_rrect(98, y, 124, 24, 12, on ? GOLD : HEX(0x6B4A2C), 256);
  mid(&np_font_body, 160, y + 17, s, on ? WOOD : WHITE);
}
static void overlay(void) {
  static const char *const titles[4] = {0, "Cookie Clicker", T("How to play"), T("Quit the game?")};
  gfx_fill_alpha(0, 0, SCREEN_W, SCREEN_H, 0, 130);
  gfx_rrect(40, 22, 240, 204, 14, WOOD, 256);

  mid(&np_font_title, 160, 52, titles[ov], WHITE);
  if (ov == OV_HELP) {
    for (int i = 0; i < 6; i++) {
      gfx_text_right(&np_font_small, 128, 76 + 16 * i, help_keys[2 * i], GOLD, 256);
      say(&np_font_small, 136, 76 + 16 * i, help_keys[2 * i + 1], WHITE);
    }
    mid(&np_font_small, 160, 182, T("Your bakery only bakes while the game is open."), HEX(0xC9B79C));
    button(192, T("Play"), true);
    return;
  }
  int n = ov == OV_MENU ? 3 : 2;
  for (int i = 0; i < n; i++) button(196 - 32 * (n - i), ov == OV_MENU ? menu_items[i] : i ? T("No") : T("Yes"), osel == i);
  if (ov == OV_QUIT) mid(&np_font_small, 160, 96, T("Your bakery is saved."), WHITE);
}

static void scene(void *ctx) {
  if (VIS(0, 16)) { /* the news */
    gfx_fill(0, 0, SCREEN_W, 16, HEX(0x150D06));
    say(&np_font_small, news_x, 12, txt(news_n), HEX(0xE8DCC8));
  }
  left();
  store();
  if (ov) overlay();
}

/* ------------------------------------------------------------ the game */
static void load(void) {
  uint32_t len = 0;
  const uint8_t *d = np_file_read(SAVE_NAME, &len);
  if (d && len == sizeof sv && d[0] == 'C' && d[1] == 1) /* (or a new bakery: sv starts as zeros) */
    for (uint32_t i = 0; i < sizeof sv; i++) ((uint8_t *)&sv)[i] = d[i];
  /* whatever it holds, a bakery this game could have saved */
  for (double *v = &sv.cookies; v <= &sv.handmade; v++)
    if (!(*v >= 0 && *v < 1e300)) *v = 0;
}
static void move(int d) {
  int n = rows(), shown = tab == 2 ? 12 : 6, *s = &sel[tab], *t = &top[tab];
  *s += d;
  if (*s >= n) *s = n - 1;
  if (*s < 0) *s = 0;
  if (*s < *t) *t = *s;
  if (*s >= *t + shown) *t = *s - shown + 1;
}

int main(void) {
  np_app_begin();
  /* the names and the news, unpacked once */
  if (np_inflate_tables_size() <= sizeof ram.tables)
    np_inflate((uint8_t *)text, TEXT_SIZE, text_z, sizeof text_z, (np_inflate_tables_t *)(void *)ram.tables);
  for (int i = 0, at = 0; i < N_TEXT; i++) {
    text_at[i] = (uint16_t)at;
    while (at < TEXT_SIZE - 1 && text[at]) at++;
    at++;
  }
  color_t *their_strip = gfx_buf;
  gfx_buf = ram.strip;
  now = (uint32_t)eadk_timing_millis();
  seed = eadk_random() | 1;
  load();
  sort_list();
  check();
  next_news();
  if (!(sv.flags & 1)) ov = OV_HELP, sv.flags |= 1;
  uint32_t held = keys(), stale = held; /* keys still down from the launcher wait to be let go */
  uint32_t repeat_at = 0, last = now, second = now, saved = now;
  for (;;) {
    now = (uint32_t)eadk_timing_millis();
    uint32_t dt = now - last > 250 ? 250 : now - last, k = keys(), hit = k & ~held;
    const uint32_t ud = K(eadk_key_up) | K(eadk_key_down) | K(eadk_key_eight) | K(eadk_key_two);
    last = now, stale &= k;
    if (hit & ud) repeat_at = now + 300;
    else if ((k & ud & ~stale) && (int32_t)(now - repeat_at) >= 0) hit |= k & ud & ~stale, repeat_at = now + 60;
    held = k;
    if (k & (K(eadk_key_home) | K(eadk_key_on_off))) break;
    bool ok = hit & (K(eadk_key_ok) | K(eadk_key_five)), exe = hit & K(eadk_key_exe);
    int dx = !!(hit & (K(eadk_key_right) | K(eadk_key_six))) - !!(hit & (K(eadk_key_left) | K(eadk_key_four)));
    int dy = !!(hit & (K(eadk_key_down) | K(eadk_key_two))) - !!(hit & (K(eadk_key_up) | K(eadk_key_eight)));
    if (ov) {
      int n = ov == OV_MENU ? 3 : 2, was = ov, c = osel = (osel + dy + n) % n;
      if (hit & K(eadk_key_back)) ov = OV_NONE;
      else if (ok || exe) { /* (Quit game asks first, No chosen) */
        ov = was == OV_MENU && c ? OV_MENU + c : OV_NONE, osel = 1;
        if (was == OV_QUIT && !c) break;
      }
    } else {
      if (ok) click();
      if ((hit & K(eadk_key_shift)) && gc_life) golden_pop();
      if (hit & K(eadk_key_back)) ov = OV_MENU, osel = 0, save();
      if (hit & K(eadk_key_multiplication)) sv.mode = (uint8_t)((sv.mode + 1) % 3);
      tab = (tab + dx + 3) % 3;
      move(dy);
      if (exe && tab < 2 && sel[tab] < rows()) buy();
    }
    /* time goes by: baking, the golden cookie and its effects */
    earn((double)cps * dt / 1000);
    for (uint32_t *t = &sv.frenzy; t <= &sv.cfrenzy; t++)
      if (*t && !(*t = *t > dt ? *t - dt : 0)) compute();
    if (gc_life) gc_life = gc_life > dt ? gc_life - dt : 0;
    else if ((sv.gc_t += dt) >= 300000u >> luck()) {
      uint32_t lo = 300000u >> luck();
      float f = (float)(sv.gc_t - lo) / (float)(2 * lo);
      if (f > 1 || (float)(rnd() & 0xFFFF) < f * f * f * f * f * 65536)
        sv.gc_t = 0, gc_dur = gc_life = 13000u << luck(), gc_x = 20 + (int)(rnd() % (LW - 40)), gc_y = 84 + (int)(rnd() % 100);
    }
    if (now - second >= 1000) second = now, check();
    if (now - saved >= 10000) saved = now, save();
    news_x -= (int)(dt * 45 / 1000) + 1;
    if (news_x < -news_w) next_news();
    eadk_display_wait_for_vblank();
    gfx_render(scene, 0, 0, SCREEN_H);
    uint32_t spent = (uint32_t)eadk_timing_millis() - now;
    if (spent < 30) eadk_timing_msleep(30 - spent);
  }
  save();
  gfx_buf = their_strip;
  return np_app_end();
}
