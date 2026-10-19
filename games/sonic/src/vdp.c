/* The Mega Drive's video chip, as far as the game uses it: 2048 tiles of patterns (here each one
 * points at its pattern in data.bin, so loading art costs no memory), two 64x32 planes of tile words
 * scrolled by a table of a scroll a line, 80 sprites from the sprite table, 64 colours, and the
 * water palette from a line on (the horizontal interrupt the original uses in Labyrinth Zone).
 *
 * vdp_render() draws the 320x224 picture line by line: plane B, plane A over it, then the sprites
 * line, each pixel keeping the front one by the chip's rules (a high-priority tile over a low one,
 * sprites over the planes of their priority, the first sprite in the list in front of the others);
 * then the colours, 16 lines at a time to the screen, 8 lines below its top. */
#include "s1.h"
#include "plat.h"

const uint8_t *vram[2048];
uint16_t plane_a[32][64], plane_b[32][64];
uint16_t cram[64];
Sprite spritetable[80];
uint8_t vdp_backdrop = 0x20;
int16_t vdp_vscroll_a, vdp_vscroll_b;
bool vdp_line_scroll = true;
int vdp_hint_line = -1;
bool vdp_display = true;

const uint8_t vdp_blank_pattern[32];
#define PATTERNS ((const uint8_t *)DATA(D_TILES))
#define ARTS DATAW(D_ARTS)

void vdp_load_art_part(int art, int first, int count, int tile) {
  const uint8_t *p = PATTERNS + (uint32_t)(ARTS[art * 2] + first) * 32;
  for (int i = 0; i < count && tile + i < 2048; i++) vram[tile + i] = p + i * 32;
}
void vdp_load_art(int art, int tile) { vdp_load_art_part(art, 0, ARTS[art * 2 + 1], tile); }

void vdp_clear_planes(void) {
  memset(plane_a, 0, sizeof plane_a);
  memset(plane_b, 0, sizeof plane_b);
  for (int i = 0; i < 2048; i++)
    if (!vram[i]) vram[i] = vdp_blank_pattern;
}

/* a rectangle of tile words into a plane, each plus add */
void vdp_fill_plane(uint16_t (*plane)[64], int col, int row, int w, int h, const uint16_t *words, uint16_t add) {
  for (int r = 0; r < h; r++)
    for (int c = 0; c < w; c++) plane[(row + r) & 31][(col + c) & 63] = (uint16_t)(words[r * w + c] + add);
}

/* ------------------------------------------------------------------ the picture */
/* the levels the Mega Drive's colour converter puts out for each of its 8 steps */
static const uint8_t level[8] = {0, 52, 87, 116, 144, 172, 206, 255};
static uint16_t rgb(uint16_t c) {
  uint8_t r = level[(c >> 1) & 7], g = level[(c >> 5) & 7], b = level[(c >> 9) & 7];
  return (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | b >> 3);
}

extern uint16_t vdp_water_cram[64];
uint16_t vdp_water_cram[64];
static uint16_t lut[64], lut_water[64];
static uint8_t line[8 + 320 + 8], sline[320 + 64];
static uint16_t band[16 * 320];

/* one line of a plane: its opaque pixels as palette line, colour and priority (bit 7); over: on top of
   what is there, but not over a high-priority pixel with a low-priority one */
static void plane_line(uint8_t *out, uint16_t (*plane)[64], int hs, int ys, bool over) {
  int px = -hs & 511;
  const uint16_t *nt = plane[(ys >> 3) & 31];
  int fy = ys & 7, col = px >> 3;
  for (int x = -(px & 7); x < 320; x += 8, col = (col + 1) & 63) {
    uint16_t w = nt[col];
    const uint8_t *p = vram[w & 0x7FF] + ((w & 0x1000) ? 7 - fy : fy) * 4;
    uint32_t bits = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    if (!bits) continue;
    uint8_t add = (uint8_t)((w >> 9 & 0x30) | (w >> 8 & 0x80)), hi = w & 0x8000 ? 1 : 0;
    uint8_t *o = out + x;
    for (int i = 0; i < 8; i++) {
      int c = w & 0x800 ? bits >> (i * 4) & 15 : bits >> (28 - i * 4) & 15;
      if (c && (!over || hi || !(o[i] & 0x80))) o[i] = (uint8_t)(add | c);
    }
  }
}

/* the sprites of the list, in list order */
static struct {
  int16_t x, y, w, h;
  uint16_t tile;
} spr[80];
static int nspr;

static void collect_sprites(void) {
  nspr = 0;
  for (int i = 0, n = 0; n < 80; n++) {
    const Sprite *s = &spritetable[i];
    spr[nspr].x = (int16_t)((s->x & 0x1FF) - 128);
    spr[nspr].y = (int16_t)((s->y & 0x3FF) - 128);
    spr[nspr].w = (int16_t)(((s->size >> 2) & 3) + 1);
    spr[nspr].h = (int16_t)((s->size & 3) + 1);
    spr[nspr].tile = s->tile;
    nspr++;
    if (!s->link || s->link >= 80) break;
    i = s->link;
  }
}

static void sprite_line(int y) {
  memset(sline, 0, sizeof sline);
  for (int i = 0; i < nspr; i++) {
    int dy = y - spr[i].y, hc = spr[i].h, wc = spr[i].w;
    if (dy < 0 || dy >= hc * 8) continue;
    int x0 = spr[i].x;
    if (x0 >= 320 || x0 + wc * 8 <= 0) continue;
    uint16_t t = spr[i].tile;
    int row = t & 0x1000 ? hc * 8 - 1 - dy : dy;
    uint8_t add = (uint8_t)((t >> 9 & 0x30) | (t >> 8 & 0x80));
    for (int c = 0; c < wc; c++) {
      int tc = t & 0x800 ? wc - 1 - c : c;
      const uint8_t *p = vram[((t & 0x7FF) + tc * hc + (row >> 3)) & 0x7FF] + (row & 7) * 4;
      uint32_t bits = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
      if (!bits) continue;
      int xs = x0 + c * 8;
      for (int k = 0; k < 8; k++) {
        int x = xs + k;
        if ((unsigned)x >= 320 || sline[x]) continue;
        int v = t & 0x800 ? bits >> (k * 4) & 15 : bits >> (28 - k * 4) & 15;
        if (v) sline[x] = (uint8_t)(add | v);
      }
    }
  }
}

void vdp_render(void) {
  for (int i = 0; i < 64; i++) lut[i] = rgb(cram[i]), lut_water[i] = rgb(vdp_water_cram[i]);
  collect_sprites();
  uint8_t *out = line + 8;
  for (int y = 0; y < 224; y++) {
    uint16_t *dst = band + (y & 15) * 320;
    if (!vdp_display) {
      for (int x = 0; x < 320; x++) dst[x] = 0;
    } else {
      memset(line, 0, sizeof line);
      plane_line(out, plane_b, hscroll[y].bg, vdp_vscroll_b + y, false);
      plane_line(out, plane_a, hscroll[y].fg, vdp_vscroll_a + y, true);
      sprite_line(y);
      for (int x = 0; x < 320; x++) {
        uint8_t s = sline[x];
        if (s && ((s & 0x80) || !(out[x] & 0x80))) out[x] = s;
      }
      const uint16_t *l = vdp_hint_line >= 0 && y > vdp_hint_line ? lut_water : lut;
      uint16_t back = l[vdp_backdrop & 63];
      for (int x = 0; x < 320; x++) dst[x] = out[x] ? l[out[x] & 63] : back;
    }
    if ((y & 15) == 15) plat_push(0, 8 + y - 15, 320, 16, band);
  }
}
