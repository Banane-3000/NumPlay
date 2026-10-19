/* The level: its size and start, its layout, 16x16 blocks and 256x256 chunks; drawing them into
 * the two planes as the camera moves (a column or a row of blocks each time it crosses 16 pixels,
 * the background in sections that scroll at their own speeds); the camera following Sonic; each
 * zone's background deformation (the scroll table: a scroll a line); the boundaries that move as
 * Sonic goes through an act; the art loaded for each zone; the oscillating numbers platforms swing
 * by, and the frames of animations shared by all objects of a kind (rings, signposts...). */
#include "s1.h"

uint8_t lvllayout[8][2][0x40];
static int nchunks; /* (in the zone's 256x256 chunks) */
const uint16_t *blocks16;
const uint16_t *chunks256;
union bgscroll_u bgscroll;
HScroll hscroll[256];

/* ------------------------------------------------------------------ size and start */
static const int16_t level_size[8][4][4] = {
  {{0, 0x24BF, 0, 0x300}, {0, 0x1EBF, 0, 0x300}, {0, 0x2960, 0, 0x300}, {0, 0x2ABF, 0, 0x300}},
  {{0, 0x19BF, 0, 0x530}, {0, 0x10AF, 0, 0x720}, {0, 0x202F, -0x100, 0x800}, {0, 0x20BF, 0, 0x720}},
  {{0, 0x17BF, 0, 0x1D0}, {0, 0x17BF, 0, 0x520}, {0, 0x1800, 0, 0x720}, {0, 0x16BF, 0, 0x720}},
  {{0, 0x1FBF, 0, 0x640}, {0, 0x1FBF, 0, 0x640}, {0, 0x2000, 0, 0x6C0}, {0, 0x3EC0, 0, 0x720}},
  {{0, 0x22C0, 0, 0x420}, {0, 0x28C0, 0, 0x520}, {0, 0x2C00, 0, 0x620}, {0, 0x2EC0, 0, 0x620}},
  {{0, 0x21C0, 0, 0x720}, {0, 0x1E40, -0x100, 0x800}, {0x2080, 0x2460, 0x510, 0x510}, {0, 0x3EC0, 0, 0x720}},
  {{0, 0x500, 0x110, 0x110}, {0, 0xDC0, 0x110, 0x110}, {0, 0x2FFF, 0, 0x320}, {0, 0x2FFF, 0, 0x320}},
  {{0}},
};

static const uint32_t start_loc[7][4] = {
  {D_START_GHZ1, D_START_GHZ2, D_START_GHZ3, 0},
  {D_START_LZ1, D_START_LZ2, D_START_LZ3, D_START_SBZ3},
  {D_START_MZ1, D_START_MZ2, D_START_MZ3, 0},
  {D_START_SLZ1, D_START_SLZ2, D_START_SLZ3, 0},
  {D_START_SYZ1, D_START_SYZ2, D_START_SYZ3, 0},
  {D_START_SBZ1, D_START_SBZ2, D_START_FZ, 0},
  {D_START_END1, D_START_END2, 0, 0},
};
static const uint32_t ending_start[8] = {
  D_START_CREDITS_DEMOS_GHZ1_CREDITS_DEMO_1, D_START_CREDITS_DEMOS_MZ2_CREDITS_DEMO,
  D_START_CREDITS_DEMOS_SYZ3_CREDITS_DEMO, D_START_CREDITS_DEMOS_LZ3_CREDITS_DEMO,
  D_START_CREDITS_DEMOS_SLZ3_CREDITS_DEMO, D_START_CREDITS_DEMOS_SBZ1_CREDITS_DEMO,
  D_START_CREDITS_DEMOS_SBZ2_CREDITS_DEMO, D_START_CREDITS_DEMOS_GHZ1_CREDITS_DEMO_2,
};
static const uint8_t loop_chunks[7][4] = {
  {0xB5, 0x7F, 0x1F, 0x20}, {0x7F, 0x7F, 0x7F, 0x7F}, {0x7F, 0x7F, 0x7F, 0x7F}, {0xAA, 0xB4, 0x7F, 0x7F},
  {0x7F, 0x7F, 0x7F, 0x7F}, {0x7F, 0x7F, 0x7F, 0x7F}, {0x7F, 0x7F, 0x7F, 0x7F},
};

void Lamp_LoadInfo(void);
static void BgScrollSpeed(int16_t y, int16_t x);

void LevelSizeLoad(void) {
  V.dle_routine = 0;
  const int16_t *s = level_size[V.zone][V.act & 3];
  V.limitleft2 = V.limitleft1 = s[0];
  V.limitright2 = V.limitright1 = s[1];
  V.limittop2 = V.limittop1 = s[2];
  V.limitbtm2 = V.limitbtm1 = s[3];
  V.limitleft3 = (int16_t)(V.limitleft2 + 0x240);
  V.fg_xblock = V.fg_yblock = 0x10;
  V.lookshift = 0x60;
  int16_t x, y;
  if (V.lastlamp) {
    Lamp_LoadInfo();
    x = player->x.w.i, y = player->y.w.i;
  } else {
    const uint16_t *p = DATAW(start_loc[V.zone][V.act & 3]);
    if (V.f_demo < 0) p = DATAW(ending_start[(V.creditsnum - 1) & 7]);
    x = (int16_t)p[0], y = (int16_t)p[1];
    player->x.w.i = x, player->y.w.i = y;
  }
  uint16_t cx = (uint16_t)(x - 160);
  if ((uint16_t)x < 160) cx = 0;
  if (cx >= (uint16_t)V.limitright2) cx = (uint16_t)V.limitright2;
  V.cam[0].x.w.i = (int16_t)cx;
  uint16_t cy = (uint16_t)(y - 96);
  if ((uint16_t)y < 96) cy = 0;
  if ((int16_t)cy >= V.limitbtm2) cy = (uint16_t)V.limitbtm2;
  V.cam[0].y.w.i = (int16_t)cy;
  BgScrollSpeed((int16_t)cy, (int16_t)cx);
  memcpy(V.loop256, loop_chunks[V.zone > 6 ? 6 : V.zone], 4);
}

static void BgScrollSpeed(int16_t y, int16_t x) {
  if (!V.lastlamp) {
    V.cam[1].y.w.i = V.cam[2].y.w.i = y;
    V.cam[1].x.w.i = V.cam[2].x.w.i = V.cam[3].x.w.i = x;
  }
  int32_t d0 = y;
  switch (V.zone) {
  case id_GHZ:
    V.cam[1].x.l = V.cam[1].y.l = V.cam[2].y.l = V.cam[3].y.l = 0;
    bgscroll.l[0] = bgscroll.l[1] = bgscroll.l[2] = 0;
    break;
  case id_LZ: V.cam[1].y.w.i = (int16_t)(d0 >> 1); break;
  case id_MZ: break;
  case id_SLZ:
    V.cam[1].y.w.i = (int16_t)((d0 >> 1) + 0xC0);
    V.cam[1].x.l = 0;
    break;
  case id_SYZ:
    d0 = (d0 * 0x30) >> 8;
    V.cam[1].y.w.i = (int16_t)(d0 + 1);
    V.cam[1].x.l = 0;
    break;
  case id_SBZ: V.cam[1].y.w.i = (int16_t)(((y & 0x7F8) >> 3) + 1); break;
  default: { /* the ending */
    int16_t bx = (int16_t)(V.cam[0].x.w.i >> 1);
    V.cam[1].x.w.i = V.cam[2].x.w.i = bx;
    bx = (int16_t)(bx >> 2);
    V.cam[3].x.w.i = (int16_t)(bx * 3);
    V.cam[1].y.l = V.cam[2].y.l = V.cam[3].y.l = 0;
    bgscroll.l[0] = bgscroll.l[1] = bgscroll.l[2] = 0;
    break;
  }
  }
}

/* ------------------------------------------------------------------ layout and blocks */
static const uint32_t layouts[7][4][2] = {
  {{D_LEVEL_GHZ1, D_LEVEL_GHZBG}, {D_LEVEL_GHZ2, D_LEVEL_GHZBG}, {D_LEVEL_GHZ3, D_LEVEL_GHZBG}, {0, 0}},
  {{D_LEVEL_LZ1, D_LEVEL_LZBG}, {D_LEVEL_LZ2, D_LEVEL_LZBG}, {D_LEVEL_LZ3, D_LEVEL_LZBG}, {D_LEVEL_SBZ3, D_LEVEL_LZBG}},
  {{D_LEVEL_MZ1, D_LEVEL_MZ1BG}, {D_LEVEL_MZ2, D_LEVEL_MZ2BG}, {D_LEVEL_MZ3, D_LEVEL_MZ3BG}, {0, 0}},
  {{D_LEVEL_SLZ1, D_LEVEL_SLZBG}, {D_LEVEL_SLZ2, D_LEVEL_SLZBG}, {D_LEVEL_SLZ3, D_LEVEL_SLZBG}, {0, 0}},
  {{D_LEVEL_SYZ1, D_LEVEL_SYZBG}, {D_LEVEL_SYZ2, D_LEVEL_SYZBG}, {D_LEVEL_SYZ3, D_LEVEL_SYZBG}, {0, 0}},
  {{D_LEVEL_SBZ1, D_LEVEL_SBZ1BG}, {D_LEVEL_SBZ2, D_LEVEL_SBZ2BG}, {D_LEVEL_SBZ2, D_LEVEL_SBZ2BG}, {0, 0}},
  {{D_LEVEL_ENDING, D_LEVEL_GHZBG}, {D_LEVEL_ENDING, D_LEVEL_GHZBG}, {0, 0}, {0, 0}},
};

static void layout_load(int plane, uint32_t off) {
  if (!off) return;
  const uint8_t *p = DATA(off);
  int w = p[0] + 1, h = p[1] + 1;
  p += 2;
  for (int r = 0; r < h && r < 8; r++)
    for (int c = 0; c < w; c++) {
      /* (rows longer than $40 run into the next plane's, as in the original) */
      int at = r * 0x80 + plane * 0x40 + c;
      if (at < 0x400) (&lvllayout[0][0][0])[at] = p[r * w + c];
    }
}

void LevelLayoutLoad(void) {
  memset(lvllayout, 0, sizeof lvllayout);
  layout_load(0, layouts[V.zone][V.act & 3][0]);
  layout_load(1, layouts[V.zone][V.act & 3][1]);
}

static const struct {
  uint32_t blocks, chunks, col;
  uint8_t nchunks, plc1, plc2, pal;
} headers[7] = {
  {D_BLK16_GHZ, D_BLK256_GHZ, D_COL_GHZ, N_BLK256_GHZ, plcid_GHZ, plcid_GHZ2, palid_GHZ},
  {D_BLK16_LZ, D_BLK256_LZ, D_COL_LZ, N_BLK256_LZ, plcid_LZ, plcid_LZ2, palid_LZ},
  {D_BLK16_MZ, D_BLK256_MZ, D_COL_MZ, N_BLK256_MZ, plcid_MZ, plcid_MZ2, palid_MZ},
  {D_BLK16_SLZ, D_BLK256_SLZ, D_COL_SLZ, N_BLK256_SLZ, plcid_SLZ, plcid_SLZ2, palid_SLZ},
  {D_BLK16_SYZ, D_BLK256_SYZ, D_COL_SYZ, N_BLK256_SYZ, plcid_SYZ, plcid_SYZ2, palid_SYZ},
  {D_BLK16_SBZ, D_BLK256_SBZ, D_COL_SBZ, N_BLK256_SBZ, plcid_SBZ, plcid_SBZ2, palid_SBZ1},
  {D_BLK16_GHZ, D_BLK256_GHZ, D_COL_GHZ, N_BLK256_GHZ, 0xFF, 0xFF, palid_Ending},
};
int level_plc1(void) { return headers[V.zone].plc1; }
int level_nchunks(void) { return nchunks; }

void LevelDataLoad(void) {
  blocks16 = DATAW(headers[V.zone].blocks);
  chunks256 = DATAW(headers[V.zone].chunks);
  nchunks = headers[V.zone].nchunks;
  LevelLayoutLoad();
  int pal = headers[V.zone].pal;
  if (V.zone == id_LZ && V.act == 3) pal = palid_SBZ3;
  if ((V.zone == id_SBZ && V.act == 1) || (V.zone == id_SBZ && V.act == 2)) pal = palid_SBZ2;
  PalLoad_Fade(pal);
  if (headers[V.zone].plc2 != 0xFF) AddPLC(headers[V.zone].plc2);
}

void ColIndexLoad(void) { V.collindex = DATA(headers[V.zone > 5 ? 0 : V.zone].col); }

/* ------------------------------------------------------------------ drawing blocks into the planes */
/* the block at (x, y) of a layout (0 foreground, 1 background), and the chunk word holding it */
static const uint16_t *block_at(int plane, int x, int y, uint16_t *word) {
  /* (the layout index can reach into the other plane's row: x up to $7F chunks, as in the original) */
  int c = (&lvllayout[0][0][0])[(((uint16_t)y >> 1) & 0x380) + (((uint16_t)x >> 8) & 0x7F) + plane * 0x40];
  *word = 0;
  if (!c || ((c - 1) & 0x7F) >= nchunks) return blocks16;
  uint16_t w = chunks256[((c - 1) & 0x7F) * 256 + ((y >> 4) & 15) * 16 + ((x >> 4) & 15)];
  *word = w;
  return blocks16 + (w & 0x3FF) * 4;
}

static void draw_block(uint16_t (*pl)[64], int col, int row, const uint16_t *b, uint16_t w) {
  uint16_t t0 = b[0], t1 = b[1], t2 = b[2], t3 = b[3];
  if (w & 0x1000) { /* flipped vertically: bottom pair on top */
    uint16_t a = t0, c = t1;
    t0 = t2 ^ 0x1000, t1 = t3 ^ 0x1000, t2 = a ^ 0x1000, t3 = c ^ 0x1000;
  }
  if (w & 0x800) {
    uint16_t a = t0, c = t2;
    t0 = t1 ^ 0x800, t1 = a ^ 0x800, t2 = t3 ^ 0x800, t3 = c ^ 0x800;
  }
  pl[row & 31][col & 63] = t0;
  pl[row & 31][(col + 1) & 63] = t1;
  pl[(row + 1) & 31][col & 63] = t2;
  pl[(row + 1) & 31][(col + 1) & 63] = t3;
}

/* a block of the layout at camera + (dx, dy), into the plane where the camera puts it */
static void draw_at(uint16_t (*pl)[64], int plane, const Cam *cam, int dy, int dx) {
  int x = (int16_t)(cam->x.w.i + dx), y = (int16_t)(cam->y.w.i + dy);
  uint16_t w;
  const uint16_t *b = block_at(plane, x, y, &w);
  draw_block(pl, (x & 0x1F0) >> 3, (y & 0xF0) >> 3, b, w);
}
/* at layout x = dx (not following the camera), camera y + dy */
static void draw_at_x0(uint16_t (*pl)[64], int plane, const Cam *cam, int dy, int x) {
  int y = (int16_t)(cam->y.w.i + dy);
  uint16_t w;
  const uint16_t *b = block_at(plane, x, y, &w);
  draw_block(pl, (x & 0x1F0) >> 3, (y & 0xF0) >> 3, b, w);
}

static void blocks_lr(uint16_t (*pl)[64], int plane, const Cam *cam, int dy, int dx, int n) {
  for (int i = 0; i < n; i++) draw_at(pl, plane, cam, dy, dx + i * 16);
}
static void blocks_lr_all(uint16_t (*pl)[64], int plane, const Cam *cam, int dy) {
  for (int i = 0; i < 32; i++) draw_at_x0(pl, plane, cam, dy, i * 16);
}
static void blocks_tb(uint16_t (*pl)[64], int plane, const Cam *cam, int dy, int dx, int n) {
  for (int i = 0; i < n; i++) draw_at(pl, plane, cam, dy + i * 16, dx);
}

static void draw_fg(const Cam *cam, uint8_t *flags) {
  if (!*flags) return;
  if (*flags & 1) blocks_lr(plane_a, 0, cam, -16, -16, 22);
  if (*flags & 2) blocks_lr(plane_a, 0, cam, 224, -16, 22);
  if (*flags & 4) blocks_tb(plane_a, 0, cam, -16, -16, 16);
  if (*flags & 8) blocks_tb(plane_a, 0, cam, -16, 320, 16);
  *flags &= (uint8_t)~0x0F;
}

static void draw_bg_top(const Cam *cam, uint8_t *flags) {
  if (!*flags) return;
  if (*flags & 1) blocks_lr(plane_b, 1, cam, -16, -16, 22);
  if (*flags & 2) blocks_lr(plane_b, 1, cam, 224, -16, 22);
  if (*flags & 4) blocks_tb(plane_b, 1, cam, -16, -16, 16);
  if (*flags & 8) blocks_tb(plane_b, 1, cam, -16, 320, 16);
  if (*flags & 0x10) blocks_lr_all(plane_b, 1, cam, -16);
  if (*flags & 0x20) blocks_lr_all(plane_b, 1, cam, 224);
  *flags &= (uint8_t)~0x3F;
}

/* (Scrap Brain and Marble: which camera each row of 16 pixels of the background follows) */
static const uint8_t scrollmap_sbz[0x22] = {0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 4, 4, 4, 4, 4, 4,
                                            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0};
static const uint8_t scrollmap_mz[0x62] = {
  0, 0, 0, 0, 0, 0, 6, 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0};
static const uint8_t scrollmap_ghz[16] = {0, 0, 0, 0, 6, 6, 6, 4, 4, 4, 0, 0, 0, 0, 0, 0};

static const Cam *bg_cam(const Cam *cams, int sel) { return sel <= 2 ? &cams[1] : sel == 4 ? &cams[2] : &cams[3]; }

/* a background row whose section follows the camera chosen by sel (0: the whole width of the layout) */
static void bg_row(const Cam *cams, int sel, int dy) {
  if (!sel) blocks_lr_all(plane_b, 1, &cams[1], dy);
  else blocks_lr(plane_b, 1, bg_cam(cams, sel), dy, -16, 22);
}

static void draw_scrollmap(const Cam *cams, uint8_t *flags, const uint8_t *map, int ybase, int ymask) {
  int dy = -16;
  bool row = false;
  if (*flags & 1) *flags &= (uint8_t)~1, row = true;
  else if (*flags & 2) *flags &= (uint8_t)~2, row = true, dy = 224;
  if (row) bg_row(cams, map[1 + (((cams[1].y.w.i - ybase + dy) & ymask) >> 4)], dy);
  if (!*flags) return;
  int dx = -16;
  uint8_t f = *flags & 0xA8;
  if (f) *flags = (uint8_t)(f >> 1), dx = 320;
  const uint8_t *m = map + (((cams[1].y.w.i - ybase) & ymask) >> 4);
  for (int i = 0; i < 16; i++) {
    int sel = m[i];
    if (*flags >> sel & 1) draw_at(plane_b, 1, bg_cam(cams, sel), -16 + i * 16, dx);
  }
  *flags = 0;
}

static void draw_bg_bottom(const Cam *cams, const Cam *cam, uint8_t *flags) {
  if (!*flags) return;
  if (V.zone == id_SBZ) {
    draw_scrollmap(cams, flags, scrollmap_sbz, 0, 0x1F0);
    return;
  }
  if (*flags & 1) *flags &= (uint8_t)~1, blocks_tb(plane_b, 1, cam, 112, -16, 3);
  if (*flags & 2) *flags &= (uint8_t)~2, blocks_tb(plane_b, 1, cam, 112, 320, 3);
}

static void draw_bg_block3(const Cam *cams, const Cam *cam, uint8_t *flags) {
  if (!*flags) return;
  if (V.zone == id_MZ) {
    draw_scrollmap(cams, flags, scrollmap_mz, 0x200, 0x7F0);
    return;
  }
  if (*flags & 1) *flags &= (uint8_t)~1, blocks_tb(plane_b, 1, cam, 64, -16, 3);
  if (*flags & 2) *flags &= (uint8_t)~2, blocks_tb(plane_b, 1, cam, 64, 320, 3);
}

void LoadTilesAsYouMove_BGOnly(void) {
  draw_bg_top(&V.cam[1], &V.scroll_flags[1]);
  draw_bg_bottom(V.cam, &V.cam[2], &V.scroll_flags[2]);
}

void LoadTilesAsYouMove(void) {
  draw_bg_top(&V.cam_dup[1], &V.scroll_flags_dup[1]);
  draw_bg_bottom(V.cam_dup, &V.cam_dup[2], &V.scroll_flags_dup[2]);
  draw_bg_block3(V.cam_dup, &V.cam_dup[3], &V.scroll_flags_dup[3]);
  draw_fg(&V.cam_dup[0], &V.scroll_flags_dup[0]);
}

/* the whole plane's width from the camera on, every row on screen and one more each side */
static void draw_chunks(uint16_t (*pl)[64], int plane, const Cam *cam) {
  for (int r = 0; r < 16; r++)
    for (int i = 0; i < 32; i++) draw_at(pl, plane, cam, -16 + r * 16, i * 16);
}

static void bg_rows_by_map(const uint8_t *map, int ybase, int ymask, int from) {
  for (int r = 0; r < 16; r++) {
    int dy = from + r * 16;
    bg_row(V.cam, map[((V.cam[1].y.w.i - ybase + dy) & ymask) >> 4], dy);
  }
}

void LoadTilesFromStart(void) {
  draw_chunks(plane_a, 0, &V.cam[0]);
  if (V.zone == id_GHZ || V.zone == 6) bg_rows_by_map(scrollmap_ghz, 0, 0xF0, 0);
  else if (V.zone == id_MZ) bg_rows_by_map(scrollmap_mz + 1, 0x200, 0x7F0, -16);
  else if (V.zone == id_SBZ && V.act == 0) bg_rows_by_map(scrollmap_sbz + 1, 0, 0x1F0, -16);
  else draw_chunks(plane_b, 1, &V.cam[1]);
}

/* ------------------------------------------------------------------ the camera */
static void ScrollHoriz(void) {
  int16_t old = V.cam[0].x.w.i;
  int16_t d = (int16_t)(player->x.w.i - V.cam[0].x.w.i - 144);
  int16_t nx;
  if ((uint16_t)(player->x.w.i - V.cam[0].x.w.i) < 144) { /* (an unsigned compare, as in the original) */
    nx = (int16_t)(V.cam[0].x.w.i + d);
    if (nx <= V.limitleft2) nx = V.limitleft2;
  } else if ((uint16_t)(player->x.w.i - V.cam[0].x.w.i - 144) >= 16) {
    d = (int16_t)(d - 16);
    if ((uint16_t)d >= 16) d = 16;
    nx = (int16_t)(V.cam[0].x.w.i + d);
    if (nx >= V.limitright2) nx = V.limitright2;
  } else {
    V.scrshiftx = 0;
    goto blocks;
  }
  V.scrshiftx = (int16_t)((nx - V.cam[0].x.w.i) << 8);
  V.cam[0].x.w.i = nx;
blocks:
  if (((V.cam[0].x.w.i & 0x10) ^ V.fg_xblock) == 0) {
    V.fg_xblock ^= 0x10;
    V.scroll_flags[0] |= (int16_t)(V.cam[0].x.w.i - old) < 0 ? 4 : 8;
  }
}

static void sv_set(int32_t ny) {
  int16_t old = V.cam[0].y.w.i;
  int32_t d = ny - V.cam[0].y.l;
  V.scrshifty = (int16_t)(d >> 8);
  V.cam[0].y.l = ny;
  if (((V.cam[0].y.w.i & 0x10) ^ V.fg_yblock) == 0) {
    V.fg_yblock ^= 0x10;
    V.scroll_flags[0] |= (int16_t)(V.cam[0].y.w.i - old) < 0 ? 1 : 2;
  }
}

static void sv_top(int16_t y) {
  if (y > V.limittop2) {
    sv_set((int32_t)y << 16);
    return;
  }
  if (y <= -0x100) {
    y &= 0x7FF;
    player->y.w.i &= 0x7FF;
    V.cam[0].y.w.i &= 0x7FF;
    V.cam[1].y.w.i &= 0x3FF;
  } else y = V.limittop2;
  sv_set((int32_t)y << 16);
}

static void sv_bottom(int16_t y) {
  if (y < V.limitbtm2) {
    sv_set((int32_t)y << 16);
    return;
  }
  if ((uint16_t)y >= 0x800) {
    player->y.w.i &= 0x7FF;
    V.cam[0].y.w.i = (int16_t)(V.cam[0].y.w.i - 0x800);
    V.cam[1].y.w.i &= 0x3FF;
    y = (int16_t)(y - 0x800);
  } else y = V.limitbtm2;
  sv_set((int32_t)y << 16);
}

static void ScrollVertical(void) {
  int16_t d = (int16_t)(player->y.w.i - V.cam[0].y.w.i);
  int16_t px;
  if (player->status & 4) d -= 5; /* (sonic_height - sonic_roll_height) */
  if (player->status & 2) {
    d = (int16_t)(d + 32 - V.lookshift);
    if (d < 0) goto fast;
    d = (int16_t)(d - 64);
    if (d >= 0) goto fast;
    if (V.f_bgscrollvert) goto boundary_moving;
    goto no_update;
  }
  d = (int16_t)(d - V.lookshift);
  if (d == 0) {
    if (V.f_bgscrollvert) goto boundary_moving;
  no_update:
    V.scrshifty = 0;
    return;
  }
  if (V.lookshift != 0x60) {
    px = 2;
  } else {
    int16_t s = player->inertia < 0 ? (int16_t)-player->inertia : player->inertia;
    if ((uint16_t)s >= 0x800) goto fast;
    px = 6;
  }
  goto move;
fast:
  px = 16;
move:
  if (d > px) {
    sv_bottom((int16_t)((V.cam[0].y.l + ((int32_t)px << 16)) >> 16));
    return;
  }
  if (d < -px) {
    sv_top((int16_t)((V.cam[0].y.l - ((int32_t)px << 16)) >> 16));
    return;
  }
  goto sweet;
boundary_moving:
  d = 0;
  V.f_bgscrollvert = 0;
sweet: {
  int16_t y = (int16_t)(V.cam[0].y.w.i + d);
  if (d >= 0) sv_bottom(y);
  else sv_top(y);
}
}

/* ------------------------------------------------------------------ background scrolling */
static void bgscroll_x(int cam, uint8_t *block, uint8_t *flags, int32_t d, int bit) {
  int32_t old = V.cam[cam].x.l, n = old + d;
  V.cam[cam].x.l = n;
  if ((((n >> 16) & 0x10) ^ *block) != 0) return;
  *block ^= 0x10;
  *flags |= (uint8_t)(1 << (n - old < 0 ? bit : bit + 1));
}
static void BGScroll_Block1(int32_t d, int bit) { bgscroll_x(1, &V.bg1_xblock, &V.scroll_flags[1], d, bit); }
static void BGScroll_Block2(int32_t d, int bit) { bgscroll_x(2, &V.bg2_xblock, &V.scroll_flags[2], d, bit); }
static void BGScroll_Block3(int32_t d, int bit) { bgscroll_x(3, &V.bg3_xblock, &V.scroll_flags[3], d, bit); }

static void BGScroll_YRelative(int32_t d5) {
  int32_t old = V.cam[1].y.l, n = old + d5;
  V.cam[1].y.l = n;
  if ((((n >> 16) & 0x10) ^ V.bg1_yblock) != 0) return;
  V.bg1_yblock ^= 0x10;
  V.scroll_flags[1] |= n - old < 0 ? 1 : 2;
}
static void BGScroll_XY(int32_t d4, int32_t d5) {
  int32_t old = V.cam[1].x.l, n = old + d4;
  V.cam[1].x.l = n;
  if ((((n >> 16) & 0x10) ^ V.bg1_xblock) == 0) {
    V.bg1_xblock ^= 0x10;
    V.scroll_flags[1] |= n - old < 0 ? 4 : 8;
  }
  BGScroll_YRelative(d5);
}
static void BGScroll_Y(int32_t d5) {
  int32_t old = V.cam[1].y.l, n = old + d5;
  V.cam[1].y.l = n;
  if ((((n >> 16) & 0x10) ^ V.bg1_yblock) != 0) return;
  V.bg1_yblock ^= 0x10;
  V.scroll_flags[1] |= n - old < 0 ? 0x10 : 0x20;
}
static void BGScroll_YAbsolute(int16_t y) {
  int16_t old = V.cam[1].y.w.i;
  V.cam[1].y.w.i = y;
  if (((y & 0x10) ^ V.bg1_yblock) != 0) return;
  V.bg1_yblock ^= 0x10;
  V.scroll_flags[1] |= (int16_t)(y - old) < 0 ? 1 : 2;
}

/* the scroll table from the background scroll buffer: a value for each 16 lines of background */
static void BGScroll_X(const int16_t *a2, int16_t d2) {
  int16_t fg = (int16_t)-V.cam[0].x.w.i;
  int skip = d2 & 0xF, line = 0;
  int16_t v = *a2++;
  for (int k = skip; k < 16 && line < 224; k++) hscroll[line].fg = fg, hscroll[line].bg = v, line++;
  for (int b = 0; b < 14; b++) {
    v = *a2++;
    for (int k = 0; k < 16 && line < 240; k++) hscroll[line].fg = fg, hscroll[line].bg = v, line++;
  }
}

/* a sequence of n values going from start (16.16) by step */
static void ramp(int16_t *to, int n, int16_t start, int32_t step) {
  int32_t d3 = (uint16_t)start;
  for (int i = 0; i < n; i++) {
    to[i] = (int16_t)d3;
    /* (swap, add.l, swap: the step adds to the value with its fraction above it) */
    uint32_t sw = ((uint32_t)d3 << 16) | ((uint32_t)d3 >> 16);
    sw += (uint32_t)step;
    d3 = (int32_t)((sw << 16) | (sw >> 16));
  }
}

static void deform_ghz(void) {
  int32_t d4 = (int32_t)V.scrshiftx * 0x60;
  BGScroll_Block3(d4, 0);
  d4 = (int32_t)V.scrshiftx * 0x80;
  BGScroll_Block2(d4, 0);
  int16_t y = (int16_t)(-((V.cam[0].y.w.i & 0x7FF) >> 5) + 0x20);
  if (y < 0) y = 0;
  V.bgscrposy_vdp = y;
  int16_t fg = (int16_t)(V.gamemode == id_Title ? 0 : -V.cam[0].x.w.i);
  bgscroll.l[0] += 0x10000, bgscroll.l[1] += 0xC000, bgscroll.l[2] += 0x8000;
  int line = 0;
  int16_t v = (int16_t)-((int16_t)(bgscroll.l[0] >> 16) + V.cam[3].x.w.i);
  for (int n = 32 - y; n > 0; n--) hscroll[line].fg = fg, hscroll[line++].bg = v;
  v = (int16_t)-((int16_t)(bgscroll.l[1] >> 16) + V.cam[3].x.w.i);
  for (int n = 0; n < 16; n++) hscroll[line].fg = fg, hscroll[line++].bg = v;
  v = (int16_t)-((int16_t)(bgscroll.l[2] >> 16) + V.cam[3].x.w.i);
  for (int n = 0; n < 16; n++) hscroll[line].fg = fg, hscroll[line++].bg = v;
  v = (int16_t)-V.cam[3].x.w.i;
  for (int n = 0; n < 48; n++) hscroll[line].fg = fg, hscroll[line++].bg = v;
  v = (int16_t)-V.cam[2].x.w.i;
  for (int n = 0; n < 40; n++) hscroll[line].fg = fg, hscroll[line++].bg = v;
  /* the water: faster the lower, from the hills' speed to the camera's */
  int16_t d0 = V.cam[2].x.w.i;
  int32_t d2 = (int16_t)(V.cam[0].x.w.i - d0);
  d2 = (int16_t)((d2 << 8) / 0x68);
  d2 <<= 8;
  uint32_t d3 = (uint16_t)d0;
  for (int n = 72 + y; n > 0 && line < 256; n--) {
    hscroll[line].fg = fg, hscroll[line++].bg = (int16_t)-(int16_t)d3;
    uint32_t sw = (d3 << 16) | (d3 >> 16);
    sw += (uint32_t)d2;
    d3 = (sw << 16) | (sw >> 16);
  }
}

#define drown_wobble ((const int8_t *)DATA(D_TAB_DROWN_WOBBLEDATA))
static const int8_t lz_scroll[256] = {
  1, 1, 2, 2, 3, 3, 3, 3, 2, 2, 1, 1, [128] = -1, -1, -2, -2, -3, -3, -3, -3, -2, -2, -1, -1,
  [160] = 1, 1, 2, 2, 3, 3, 3, 3, 2, 2, 1, 1};

static void deform_lz(void) {
  BGScroll_XY((int32_t)V.scrshiftx << 7, (int32_t)V.scrshifty << 7);
  V.bgscrposy_vdp = V.cam[1].y.w.i;
  uint8_t d2 = (uint8_t)(V.lz_deform >> 8), d3 = d2;
  V.lz_deform += 0x80;
  d2 = (uint8_t)(d2 + V.cam[1].y.w.i);
  d3 = (uint8_t)(d3 + V.cam[0].y.w.i);
  int16_t fg = (int16_t)-V.cam[0].x.w.i, bg = (int16_t)-V.cam[1].x.w.i;
  int16_t wy = V.waterpos1, sy = V.cam[0].y.w.i;
  for (int line = 0; line < 224; line++, sy++, d2++, d3++) {
    if (sy < wy) hscroll[line].fg = fg, hscroll[line].bg = bg;
    else {
      hscroll[line].fg = (int16_t)(fg + lz_scroll[d3]);
      hscroll[line].bg = (int16_t)(bg + drown_wobble[d2]);
    }
  }
}

static void deform_mz(void) {
  BGScroll_Block1((int32_t)V.scrshiftx * 0xC0, 2);
  BGScroll_Block3((int32_t)V.scrshiftx * 0x40, 6);
  BGScroll_Block2((int32_t)V.scrshiftx * 0x80, 4);
  int16_t d0 = 512;
  int16_t d1 = (int16_t)(V.cam[0].y.w.i - 456);
  if ((uint16_t)V.cam[0].y.w.i >= 456) d0 = (int16_t)(d0 + ((d1 * 3) >> 2));
  V.cam[2].y.w.i = V.cam[3].y.w.i = d0;
  BGScroll_YAbsolute(d0);
  V.bgscrposy_vdp = V.cam[1].y.w.i;
  V.scroll_flags[3] |= V.scroll_flags[1] | V.scroll_flags[2];
  V.scroll_flags[1] = V.scroll_flags[2] = 0;
  int16_t d2 = (int16_t)-V.cam[0].x.w.i, *a1 = bgscroll.w;
  int32_t st = (int16_t)(((d2 >> 2) - d2) * 8 / 5);
  st <<= 12;
  ramp(a1, 5, (int16_t)(d2 >> 1), st), a1 += 5;
  for (int i = 0; i < 2; i++) *a1++ = (int16_t)-V.cam[3].x.w.i;
  for (int i = 0; i < 9; i++) *a1++ = (int16_t)-V.cam[2].x.w.i;
  for (int i = 0; i < 16; i++) *a1++ = (int16_t)-V.cam[1].x.w.i;
  int16_t y = (int16_t)(V.cam[1].y.w.i - 512);
  int16_t idx = (uint16_t)y >= 0x100 ? 0x100 : y;
  BGScroll_X(bgscroll.w + ((idx & 0x1F0) >> 4), y);
}

static void deform_slz(void) {
  BGScroll_Y((int32_t)V.scrshifty << 7);
  V.bgscrposy_vdp = V.cam[1].y.w.i;
  int16_t d2 = (int16_t)-V.cam[0].x.w.i, *a1 = bgscroll.w;
  int32_t st = (int16_t)((((d2 >> 3) - d2) << 4) / 0x1C);
  st <<= 12;
  ramp(a1, 28, d2, st), a1 += 28;
  int16_t d0 = (int16_t)(d2 >> 3);
  d0 = (int16_t)(d0 + (d0 >> 1));
  for (int i = 0; i < 5; i++) *a1++ = d0;
  for (int i = 0; i < 5; i++) *a1++ = (int16_t)(d2 >> 2);
  for (int i = 0; i < 30; i++) *a1++ = (int16_t)(d2 >> 1);
  int16_t y = V.cam[1].y.w.i;
  BGScroll_X(bgscroll.w + (((y - 0xC0) & 0x3F0) >> 4), y);
}

static void deform_syz(void) {
  BGScroll_Y((int32_t)V.scrshifty * 0x30);
  V.bgscrposy_vdp = V.cam[1].y.w.i;
  int16_t d2 = (int16_t)-V.cam[0].x.w.i, *a1 = bgscroll.w;
  int32_t st = (int16_t)((((d2 >> 3) - d2) << 3) / 8);
  st <<= 12;
  ramp(a1, 8, (int16_t)(d2 >> 1), st), a1 += 8;
  for (int i = 0; i < 5; i++) *a1++ = (int16_t)(d2 >> 3);
  for (int i = 0; i < 6; i++) *a1++ = (int16_t)(d2 >> 2);
  st = (int16_t)(((d2 - (d2 >> 1)) << 4) / 0xE);
  st <<= 12;
  ramp(a1, 14, (int16_t)(d2 >> 1), st);
  int16_t y = V.cam[1].y.w.i;
  BGScroll_X(bgscroll.w + ((y & 0x1F0) >> 4), y);
}

static void deform_sbz(void) {
  if (V.act) {
    BGScroll_XY((int32_t)V.scrshiftx << 6, (int32_t)V.scrshifty << 5);
    V.bgscrposy_vdp = V.cam[1].y.w.i;
    for (int i = 0; i < 224; i++) hscroll[i].fg = (int16_t)-V.cam[0].x.w.i, hscroll[i].bg = (int16_t)-V.cam[1].x.w.i;
    return;
  }
  BGScroll_Block1((int32_t)V.scrshiftx << 7, 2);
  BGScroll_Block3((int32_t)V.scrshiftx << 6, 6);
  BGScroll_Block2((int32_t)V.scrshiftx * 0x60, 4);
  BGScroll_YRelative((int32_t)V.scrshifty << 5);
  V.cam[2].y.w.i = V.cam[3].y.w.i = V.bgscrposy_vdp = V.cam[1].y.w.i;
  V.scroll_flags[2] |= V.scroll_flags[1] | V.scroll_flags[3];
  V.scroll_flags[1] = V.scroll_flags[3] = 0;
  int16_t d2 = (int16_t)(-V.cam[0].x.w.i >> 2), *a1 = bgscroll.w;
  int32_t st = (int16_t)((((d2 >> 1) - d2) << 3) / 4);
  st <<= 12;
  ramp(a1, 4, d2, st), a1 += 4;
  for (int i = 0; i < 10; i++) *a1++ = (int16_t)-V.cam[3].x.w.i;
  for (int i = 0; i < 7; i++) *a1++ = (int16_t)-V.cam[2].x.w.i;
  for (int i = 0; i < 11; i++) *a1++ = (int16_t)-V.cam[1].x.w.i;
  int16_t y = V.cam[1].y.w.i;
  BGScroll_X(bgscroll.w + ((y & 0x1F0) >> 4), y);
}

void DeformLayers(void) {
  if (V.f_nobgscroll) return;
  memset(V.scroll_flags, 0, sizeof V.scroll_flags);
  ScrollHoriz();
  ScrollVertical();
  DynamicLevelEvents();
  V.scrposy_vdp = V.cam[0].y.w.i;
  V.bgscrposy_vdp = V.cam[1].y.w.i;
  switch (V.zone) {
  case id_LZ: deform_lz(); break;
  case id_MZ: deform_mz(); break;
  case id_SLZ: deform_slz(); break;
  case id_SYZ: deform_syz(); break;
  case id_SBZ: deform_sbz(); break;
  default: deform_ghz(); break;
  }
}

/* ------------------------------------------------------------------ art (pattern load cues) */
/* The original decompresses the art a few tiles a frame; here it is in place, so a cue loads at once. */
void AddPLC(int id) {
  const uint16_t *t = DATAW(D_PLC);
  const uint16_t *l = DATAW(D_PLC + t[1 + id]);
  for (int i = 0; i < l[0]; i++) vdp_load_art(l[1 + i * 2], l[2 + i * 2]);
}
void NewPLC(int id) { AddPLC(id); }
void QuickPLC(int id) { AddPLC(id); }
void ClearPLC(void) {}
void RunPLC(void) {}
bool plc_pending(void) { return false; }

void ClearScreen(void) {
  vdp_clear_planes();
  memset(hscroll, 0, sizeof hscroll);
  memset(spritetable, 0, sizeof spritetable);
  V.scrposy_vdp = V.bgscrposy_vdp = 0;
}

/* ------------------------------------------------------------------ oscillating numbers, shared animations */
void OscillateNumInit(void) {
  static const uint16_t base[0x21] = {0x7C, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0,
                                      0x80, 0, 0x50F0, 0x11E, 0x2080, 0xB4, 0x3080, 0x10E, 0x5080, 0x1C2, 0x7080,
                                      0x276, 0x80, 0, 0x80, 0};
  V.oscillate = base[0];
  memcpy(V.osc, base + 1, sizeof V.osc);
}

void OscillateNumDo(void) {
  static const uint16_t settings[16][2] = {{2, 0x10}, {2, 0x18}, {2, 0x20}, {2, 0x30}, {4, 0x20}, {8, 8},
                                           {8, 0x40}, {4, 0x40}, {2, 0x50}, {2, 0x50}, {2, 0x20}, {3, 0x30},
                                           {5, 0x50}, {7, 0x70}, {2, 0x10}, {2, 0x10}};
  if (player->routine >= 6) return;
  uint16_t d3 = V.oscillate;
  for (int i = 0, bit = 15; i < 16; i++, bit--) {
    uint16_t *val = &V.osc[i * 2], *spd = &V.osc[i * 2 + 1];
    uint8_t lim = (uint8_t)settings[i][1];
    if (!(d3 >> bit & 1)) {
      *spd = (uint16_t)(*spd + settings[i][0]);
      *val = (uint16_t)(*val + *spd);
      if (lim <= (uint8_t)(*val >> 8)) d3 |= (uint16_t)(1 << bit);
    } else {
      *spd = (uint16_t)(*spd - settings[i][0]);
      *val = (uint16_t)(*val + *spd);
      if (lim > (uint8_t)(*val >> 8)) d3 &= (uint16_t)~(1 << bit);
    }
  }
  V.oscillate = d3;
}

void SynchroAnimate(void) {
  if ((int8_t)--V.ani_time[0] < 0) V.ani_time[0] = 11, V.ani_frame[0] = (uint8_t)((V.ani_frame[0] - 1) & 7);
  if ((int8_t)--V.ani_time[1] < 0) V.ani_time[1] = 7, V.ani_frame[1] = (uint8_t)((V.ani_frame[1] + 1) & 3);
  if ((int8_t)--V.ani_time[2] < 0) {
    V.ani_time[2] = 7;
    if (++V.ani_frame[2] >= 6) V.ani_frame[2] = 0;
  }
  if (V.ani_time[3]) {
    uint16_t d0 = (uint16_t)(V.ani_time[3] + V.ani3_buf);
    V.ani3_buf = d0;
    d0 = (uint16_t)(d0 << 7 | d0 >> 9);
    V.ani_frame[3] = (uint8_t)(d0 & 3);
    V.ani_time[3]--;
  }
}

void SignpostArtLoad(void) {
  if (V.debuguse || V.act == 2) return;
  int16_t d1 = (int16_t)(V.limitright2 - 0x100);
  if (V.cam[0].x.w.i < d1 || !V.f_timecount || d1 == V.limitleft2) return;
  V.limitleft2 = d1;
  NewPLC(plcid_Signpost);
}
