/* The Special Stage: the maze of blocks turning round Sonic (each block a sprite, placed by the
 * turned grid), the birds and fish behind it that change into each other and the clouds and bubbles
 * further back (the original switches between pictures it drew into video memory at the start; here
 * the one shown is drawn into the plane when it changes), the colour cycles, Sonic rolling through
 * it, the rings, bumpers, UP and DOWN, R, glass and ghost blocks, the emerald, the GOAL blocks; and
 * the results after it. */
#include "s1.h"
#include "game.h"
#include "objs.h"

/* the blocks of the layout, by their number */
enum {
  SS_WallGreen_8 = 36, SS_Bumper, SS_W, SS_GOAL, SS_1Up, SS_UP, SS_DOWN, SS_R, SS_RedWhite, SS_Glass1_Blue,
  SS_Glass2_Green, SS_Glass3_Yellow, SS_Glass4_Pink, SS_R_Ani, SS_Bumper_Ani1, SS_Bumper_Ani2, SS_ZONE1,
  SS_ZONE2, SS_ZONE3, SS_ZONE4, SS_ZONE5, SS_ZONE6, SS_Ring, SS_Emerald1_Blue, SS_Emerald2_Yellow,
  SS_Emerald3_Pink, SS_Emerald4_Green, SS_Emerald5_Red, SS_Emerald6_Grey, SS_Ghost, SS_Ring_Ani1, SS_Ring_Ani2,
  SS_Ring_Ani3, SS_Ring_Ani4, SS_Emerald_Ani1, SS_Emerald_Ani2, SS_Emerald_Ani3, SS_Emerald_Ani4,
  SS_InvGhostTrigger, SS_Glass_Ani1, SS_Glass_Ani2, SS_Glass_Ani3, SS_Glass_Ani4, SS_NUM
};

#define ss_blocksize 24
#define ss_matrixsize 16
#define ss_rowlength 0x80
#define ss_rotatespeed 0x40
#define ss_timeout 30

/* the layout, 64 by 64 blocks, in a buffer 128 blocks wide with 32 empty rows and columns before it */
static uint8_t sslayout[ss_rowlength * 0x20 + 0x20 + ss_rowlength * 0x40];
#define SS_ACTUAL (ss_rowlength * 0x20 + 0x20)

/* each block's sprite: mappings, frame, tile word (SS_MapIndex, then changed as they animate) */
typedef struct {
  uint8_t map, frame;
  uint16_t tile;
} SSSprite;
#define W(p) {MAP_SSWalls, 0, (p) << 13 | ArtTile_SS_Wall}
static const SSSprite ss_mapindex[SS_NUM] = {
  {0, 0, 0},
  W(0), W(0), W(0), W(0), W(0), W(0), W(0), W(0), W(0), W(1), W(1), W(1), W(1), W(1), W(1), W(1), W(1), W(1),
  W(2), W(2), W(2), W(2), W(2), W(2), W(2), W(2), W(2), W(3), W(3), W(3), W(3), W(3), W(3), W(3), W(3), W(3),
  {MAP_Bump, 0, ArtTile_SS_Bumper},
  {MAP_SS_Shared, 0, ArtTile_SS_W_Block},
  {MAP_SS_Shared, 0, ArtTile_SS_Goal},
  {MAP_SS_Shared, 0, ArtTile_SS_Extra_Life},
  {MAP_SS_Up, 0, ArtTile_SS_Up_Down},
  {MAP_SS_Down, 0, ArtTile_SS_Up_Down},
  {MAP_SS_Shared, 0, Tile_Pal2 | ArtTile_SS_R_Block},
  {MAP_SS_Glass, 0, ArtTile_SS_Red_White_Block},
  {MAP_SS_Glass, 0, ArtTile_SS_Glass},
  {MAP_SS_Glass, 0, Tile_Pal4 | ArtTile_SS_Glass},
  {MAP_SS_Glass, 0, Tile_Pal2 | ArtTile_SS_Glass},
  {MAP_SS_Glass, 0, Tile_Pal3 | ArtTile_SS_Glass},
  {MAP_SS_Shared, 0, ArtTile_SS_R_Block},
  {MAP_Bump, 1, ArtTile_SS_Bumper},
  {MAP_Bump, 2, ArtTile_SS_Bumper},
  {MAP_SS_Shared, 0, ArtTile_SS_Zone_1},
  {MAP_SS_Shared, 0, ArtTile_SS_Zone_2},
  {MAP_SS_Shared, 0, ArtTile_SS_Zone_3},
  {MAP_SS_Shared, 0, ArtTile_SS_Zone_4},
  {MAP_SS_Shared, 0, ArtTile_SS_Zone_5},
  {MAP_SS_Shared, 0, ArtTile_SS_Zone_6},
  {MAP_Ring, 0, Tile_Pal2 | ArtTile_Ring},
  {MAP_SS_Chaos3, 0, ArtTile_SS_Emerald},
  {MAP_SS_Chaos3, 0, Tile_Pal2 | ArtTile_SS_Emerald},
  {MAP_SS_Chaos3, 0, Tile_Pal3 | ArtTile_SS_Emerald},
  {MAP_SS_Chaos3, 0, Tile_Pal4 | ArtTile_SS_Emerald},
  {MAP_SS_Chaos1, 0, ArtTile_SS_Emerald},
  {MAP_SS_Chaos2, 0, ArtTile_SS_Emerald},
  {MAP_SS_Shared, 0, ArtTile_SS_Ghost_Block},
  {MAP_Ring, 4, Tile_Pal2 | ArtTile_Ring},
  {MAP_Ring, 5, Tile_Pal2 | ArtTile_Ring},
  {MAP_Ring, 6, Tile_Pal2 | ArtTile_Ring},
  {MAP_Ring, 7, Tile_Pal2 | ArtTile_Ring},
  {MAP_SS_Glass, 0, Tile_Pal2 | ArtTile_SS_Emerald_Sparkle},
  {MAP_SS_Glass, 1, Tile_Pal2 | ArtTile_SS_Emerald_Sparkle},
  {MAP_SS_Glass, 2, Tile_Pal2 | ArtTile_SS_Emerald_Sparkle},
  {MAP_SS_Glass, 3, Tile_Pal2 | ArtTile_SS_Emerald_Sparkle},
  {MAP_SS_Shared, 2, ArtTile_SS_Ghost_Block},
  {MAP_SS_Glass, 0, ArtTile_SS_Glass},
  {MAP_SS_Glass, 0, Tile_Pal4 | ArtTile_SS_Glass},
  {MAP_SS_Glass, 0, Tile_Pal2 | ArtTile_SS_Glass},
  {MAP_SS_Glass, 0, Tile_Pal3 | ArtTile_SS_Glass},
};
#undef W
static SSSprite ss_sprite[SS_NUM];

/* the animations of blocks touched: what, its delay and step, which block (and the glass's next colour) */
typedef struct {
  uint8_t id, delay, frame, next;
  uint16_t block;
} SSAnim;
static SSAnim ss_anim[0x20];
static int16_t ss_matrix[ss_matrixsize][ss_matrixsize][2];

/* the background's scroll: the bubbles (a value and an angle each) and the clouds (16.16) */
static int16_t ss_bubbles[10][2];
static int32_t ss_clouds[7];

/* ------------------------------------------------------------------ the background */
/* the half picture of birds and fish shown (0-6), and the clouds' half (6 or 7) */
static int ss_fg_shown = -1, ss_bg_shown = -1;

/* birds and fish in a checkerboard of 8 by 8 tile pictures (SS_BGLoad drew the seven at the start) */
static void ss_draw_fg(int k) {
  const uint16_t *eni = DATAW(D_ENI_SSBG1);
  memset(plane_a, 0, sizeof plane_a);
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 8; c++) {
      int t = -1;
      if (((r + c) & 1) == (k <= 3 ? 0 : 1)) t = k + 1;
      else if (k == 0) t = 0;
      if (t >= 0) vdp_fill_plane(plane_a, c * 8, r * 8, 8, 8, eni + t * 64, ArtTile_SS_Background_Fish | Tile_Pal3);
    }
}

static void ss_draw_bg(int p) {
  const uint16_t *eni = DATAW(D_ENI_SSBG2);
  vdp_fill_plane(plane_b, 0, 0, 64, 32, eni + (p == 7 ? 64 * 32 : 0), ArtTile_SS_Background_Clouds | Tile_Pal3);
}

/* the backgrounds and colours changing in turn */
void PalCycle_SS(void) {
#define T(time, anim, plane, index, cyc2, extra) {(uint8_t)((time) - 1), anim, plane, (index) | (cyc2) << 7 | (extra)}
  static const uint8_t timing[32][4] = {
    T(4, 0, 7, 0x12, 1, 0), T(4, 0, 7, 0x10, 1, 0), T(4, 0, 7, 0xE, 1, 0), T(4, 0, 7, 0xC, 1, 0),
    T(4, 0, 7, 0xA, 1, 1),  T(4, 0, 7, 0, 1, 0),    T(4, 0, 7, 2, 1, 0),   T(4, 0, 7, 4, 1, 0),
    T(4, 0, 7, 6, 1, 0),    T(4, 0, 7, 8, 1, 0),    T(8, 8, 7, 0, 0, 0),   T(8, 0xA, 7, 0xC, 0, 0),
    T(0, 0xC, 7, 0x18, 0, 0), T(0, 0xC, 7, 0x18, 0, 0), T(8, 0xA, 7, 0xC, 0, 0), T(8, 8, 7, 0, 0, 0),
    T(4, 0, 6, 8, 1, 0),    T(4, 0, 6, 6, 1, 0),    T(4, 0, 6, 4, 1, 0),   T(4, 0, 6, 2, 1, 0),
    T(4, 0, 6, 0, 1, 1),    T(4, 0, 6, 0xA, 1, 0),  T(4, 0, 6, 0xC, 1, 0), T(4, 0, 6, 0xE, 1, 0),
    T(4, 0, 6, 0x10, 1, 0), T(4, 0, 6, 0x12, 1, 0), T(8, 2, 6, 0x24, 0, 0), T(8, 4, 6, 0x30, 0, 0),
    T(0, 6, 6, 0x3C, 0, 0), T(0, 6, 6, 0x3C, 0, 0), T(8, 4, 6, 0x30, 0, 0), T(8, 2, 6, 0x24, 0, 0),
  };
#undef T
  if (V.f_pause) return;
  if ((int16_t)--V.palss_time >= 0) return;
  const uint8_t *a0 = timing[V.palss_num++ & 0x1F];
  V.palss_time = (int8_t)a0[0] < 0 ? 0x200 - 1 : a0[0];
  V.ssbganim = a0[1];
  if (ss_fg_shown != a0[1] / 2) ss_draw_fg(ss_fg_shown = a0[1] / 2);
  if (ss_bg_shown != a0[2]) ss_draw_bg(ss_bg_shown = a0[2]);
  V.scrposy_vdp = 0; /* (the half shown is the plane: no scroll to its half) */
  uint8_t d0 = a0[3];
  if (!(d0 & 0x80)) {
    memcpy(&V.palette[32 + 7], DATAW(D_PAL_CYCLE_SPECIAL_STAGE_1) + d0 / 2, 6 * 2);
    return;
  }
  const uint16_t *a1 = DATAW(D_PAL_CYCLE_SPECIAL_STAGE_2) + (V.palss_index + (d0 >= 0x8A)) * 21;
  d0 &= 0x7F;
  if (d0 & 1) {
    d0 &= (uint8_t)~1;
    memcpy(&V.palette[48 + 7], a1, 6 * 2);
  }
  a1 += 6;
  int dst = 32 + 13;
  if (d0 >= 0xA) d0 -= 0xA, dst = 48 + 13;
  memcpy(&V.palette[dst], a1 + d0 * 3 / 2, 3 * 2);
}

static void SS_BGAnimate(void) {
  static const int8_t wobble[10][2] = {{8, 2}, {4, -1}, {2, 3}, {8, -1}, {4, 2}, {2, 3}, {8, -3}, {4, 2}, {2, 3}, {2, -1}};
  static const uint8_t bubble_blocks[10] = {0x28, 0x18, 0x10, 0x28, 0x18, 0x10, 0x30, 0x18, 8, 0x10};
  static const uint8_t cloud_blocks[7] = {0x30, 0x30, 0x30, 0x28, 0x18, 0x18, 0x18};
  uint16_t d0 = V.ssbganim;
  if (!d0) V.bgscrposy_vdp = V.cam[1].y.w.i = 0;
  const uint8_t *blocks;
  int n;
  int16_t vals[10];
  if (d0 < 8) {
    /* fish: bubbles wobbling */
    if (d0 == 6) {
      V.cam[3].x.w.i++;
      V.cam[1].y.w.i++;
      V.bgscrposy_vdp = V.cam[1].y.w.i;
    }
    for (int i = 0; i < 10; i++) {
      ss_bubbles[i][0] = (int16_t)((int32_t)CalcSine((uint8_t)ss_bubbles[i][1], NULL) * wobble[i][0] >> 8);
      ss_bubbles[i][1] = (int16_t)(ss_bubbles[i][1] + wobble[i][1]);
      vals[i] = ss_bubbles[i][0];
    }
    blocks = bubble_blocks, n = 10;
  } else {
    /* birds: clouds drifting, faster nearer the bottom */
    if (d0 == 0xC) {
      V.cam[3].x.w.i--;
      int32_t d2 = 0x18000;
      for (int i = 0; i < 7; i++, d2 -= 0x2000) ss_clouds[i] -= d2;
    }
    for (int i = 0; i < 7; i++) vals[i] = (int16_t)(ss_clouds[i] >> 16);
    blocks = cloud_blocks, n = 7;
  }
  int16_t fg = (int16_t)-V.cam[3].x.w.i;
  int line = -V.cam[1].y.w.i & 0xFF;
  for (int b = 0; b < n; b++)
    for (int k = 0; k < blocks[b]; k++, line = (line + 1) & 0xFF) hscroll[line].fg = fg, hscroll[line].bg = vals[b];
}

/* ------------------------------------------------------------------ the layout */
static void SS_Load(void) {
  static const uint32_t layouts[6] = {D_SS_1, D_SS_2, D_SS_3, D_SS_4, D_SS_5, D_SS_6};
  static const uint32_t starts[6] = {D_START_SPECIAL_STAGES_SS1, D_START_SPECIAL_STAGES_SS2, D_START_SPECIAL_STAGES_SS3,
                                     D_START_SPECIAL_STAGES_SS4, D_START_SPECIAL_STAGES_SS5, D_START_SPECIAL_STAGES_SS6};
  int d0;
  for (;;) {
    /* the next stage whose emerald Sonic has not got yet */
    d0 = V.lastspecial;
    if (++V.lastspecial >= 6) V.lastspecial = 0;
    if (V.emeralds == 6) break;
    bool got = false;
    for (int i = 0; i < V.emeralds; i++) got |= V.emldlist[i] == d0;
    if (!got) break;
  }
  const uint16_t *p = DATAW(starts[d0]);
  player->x.w.i = (int16_t)p[0];
  player->y.w.i = (int16_t)p[1];
  memset(sslayout, 0, sizeof sslayout);
  const uint8_t *src = DATA(layouts[d0]);
  for (int r = 0; r < 0x40; r++) memcpy(&sslayout[SS_ACTUAL + r * ss_rowlength], src + r * 0x40, 0x40);
  memcpy(ss_sprite, ss_mapindex, sizeof ss_sprite);
  memset(ss_anim, 0, sizeof ss_anim);
}

static SSAnim *SS_FindFreeAnimationSlot(void) {
  for (int i = 0; i < 0x20; i++)
    if (!ss_anim[i].id) return &ss_anim[i];
  return NULL;
}

/* the walls turning with the stage, and the blocks that animate together */
static void SS_AnimateBlocks(void) {
  static const uint16_t blink[4] = {0, 1, 0, 0}; /* (each wall colour blinking to the one before it) */
  uint8_t f = (uint8_t)(((uint8_t)(V.ssangle >> 8) >> 2) & 0xF);
  for (int i = 1; i <= SS_WallGreen_8; i++) ss_sprite[i].frame = f;
  if ((int8_t)--V.ani_time[1] < 0) {
    V.ani_time[1] = 8 - 1;
    V.ani_frame[1] = (V.ani_frame[1] + 1) & 3;
  }
  ss_sprite[SS_Ring].frame = V.ani_frame[1];
  if ((int8_t)--V.ani_time[2] < 0) {
    V.ani_time[2] = 8 - 1;
    V.ani_frame[2] = (V.ani_frame[2] + 1) & 1;
  }
  static const uint8_t alternating[10] = {SS_GOAL, SS_RedWhite, SS_UP, SS_DOWN, SS_Emerald1_Blue, SS_Emerald2_Yellow,
                                          SS_Emerald3_Pink, SS_Emerald4_Green, SS_Emerald5_Red, SS_Emerald6_Grey};
  for (int i = 0; i < 10; i++) ss_sprite[alternating[i]].frame = V.ani_frame[2];
  if ((int8_t)--V.ani_time[3] < 0) {
    V.ani_time[3] = 5 - 1;
    V.ani_frame[3] = (V.ani_frame[3] + 1) & 3;
  }
  for (int i = 0; i < 4; i++) ss_sprite[SS_Glass1_Blue + i].frame = V.ani_frame[3];
  if ((int8_t)--V.ani_time[0] < 0) {
    V.ani_time[0] = 8 - 1;
    V.ani_frame[0] = (V.ani_frame[0] - 1) & 7;
  }
  /* (SS_Wall_Palettes_VRAM: the pattern normal, blink, normal x5, blink, twice, read from the frame on) */
  for (int c = 0; c < 4; c++)
    for (int i = 0; i < 8; i++) {
      int k = (V.ani_frame[0] + i) & 7;
      bool b = k == 1 || k == 7;
      (void)blink;
      ss_sprite[c * 9 + 2 + i].tile = (uint16_t)(ArtTile_SS_Wall | ((b ? (c - 1) & 3 : c) << 13));
    }
}

static void SS_ExecuteAnimationQueue(void) {
  static const uint8_t ring[5] = {SS_Ring_Ani1, SS_Ring_Ani2, SS_Ring_Ani3, SS_Ring_Ani4, 0};
  static const uint8_t bump[5] = {SS_Bumper_Ani1, SS_Bumper_Ani2, SS_Bumper_Ani1, SS_Bumper_Ani2, 0};
  static const uint8_t emer[5] = {SS_Emerald_Ani1, SS_Emerald_Ani2, SS_Emerald_Ani3, SS_Emerald_Ani4, 0};
  static const uint8_t rev[5] = {SS_R, SS_R_Ani, SS_R, SS_R_Ani, 0};
  static const uint8_t glass[9] = {SS_Glass_Ani1, SS_Glass_Ani2, SS_Glass_Ani3, SS_Glass_Ani4,
                                   SS_Glass_Ani1, SS_Glass_Ani2, SS_Glass_Ani3, SS_Glass_Ani4, 0};
  static const struct {
    const uint8_t *steps;
    uint8_t delay, last; /* last: the block left after it (0: what the steps end on) */
  } kinds[7] = {{0, 0, 0}, {ring, 5, 0}, {bump, 7, SS_Bumper}, {emer, 5, 0}, {rev, 7, SS_R}, {emer, 5, 0}, {glass, 1, 0}};
  for (int i = 0; i < 0x20; i++) {
    SSAnim *a = &ss_anim[i];
    if (!a->id || a->id > 6) continue;
    if ((int8_t)--a->delay >= 0) continue;
    a->delay = kinds[a->id].delay;
    uint8_t d0 = kinds[a->id].steps[a->frame++];
    uint8_t *blk = &sslayout[a->block];
    if (d0) {
      *blk = d0;
      continue;
    }
    if (a->id == 2 || a->id == 4) *blk = kinds[a->id].last;
    else if (a->id == 6) *blk = a->next;
    else *blk = 0;
    if (a->id == 5) player->routine = 4; /* (the emerald got: the stage ends) */
    memset(a, 0, sizeof *a);
  }
}

/* the blocks round Sonic, turned by the stage's angle, as sprites after the objects' */
static void SS_ShowLayout(void) {
  SS_AnimateBlocks();
  SS_ExecuteAnimationQueue();
  int16_t d1, d0 = CalcSine((uint8_t)((V.ssangle >> 8) & 0xFC), &d1);
  int32_t d4 = d0 * ss_blocksize, d5 = d1 * ss_blocksize;
  int16_t d2 = (int16_t)(-(int16_t)((uint16_t)V.cam[0].x.w.i % ss_blocksize) - (ss_matrixsize - 1) * ss_blocksize / 2);
  int16_t d3 = (int16_t)(-(int16_t)((uint16_t)V.cam[0].y.w.i % ss_blocksize) - (ss_matrixsize - 1) * ss_blocksize / 2);
  for (int r = 0; r < ss_matrixsize; r++, d3 = (int16_t)(d3 + ss_blocksize)) {
    int32_t x = (int32_t)(int16_t)-d0 * d3 + (int32_t)d1 * d2, y = (int32_t)d0 * d2 + (int32_t)d1 * d3;
    for (int c = 0; c < ss_matrixsize; c++, x += d5, y += d4) {
      ss_matrix[r][c][0] = (int16_t)(x >> 8);
      ss_matrix[r][c][1] = (int16_t)(y >> 8);
    }
  }
  const uint8_t *a0 = sslayout + ((uint16_t)V.cam[0].y.w.i / ss_blocksize) * ss_rowlength +
                      (uint16_t)V.cam[0].x.w.i / ss_blocksize;
  for (int r = 0; r < ss_matrixsize; r++, a0 += ss_rowlength - ss_matrixsize)
    for (int c = 0; c < ss_matrixsize; c++) {
      uint8_t b = *a0++;
      if (!b || b > SS_Glass_Ani4) continue;
      uint16_t sx = (uint16_t)(ss_matrix[r][c][0] + 128 + 320 / 2);
      if (sx < 128 - 16 || sx >= 128 + 320 + 16) continue;
      uint16_t sy = (uint16_t)(ss_matrix[r][c][1] + 128 + 224 / 2);
      if (sy < 128 - 16 || sy >= 128 + 224 + 16) continue;
      const SSSprite *s = &ss_sprite[b];
      const uint8_t *p = map_frame(map_get(s->map), s->frame);
      if (p[0]) BuildSpr_Pieces(p + 2, p[0], (int16_t)sx, (int16_t)sy, 0, s->tile);
    }
  BuildSpr_End();
}

/* ------------------------------------------------------------------ Sonic */
#define sonss_touchedblock_id 0x30
#define sonss_touchedblock 0x32
#define sonss_timeout_updown 0x36
#define sonss_timeout_r 0x37
#define sonss_exittimer 0x38
#define sonss_ghoststate 0x3A
#define sonss_maxspeed 0x800
#define sonss_acceleration 0xC
#define sonss_deceleration 0x40
#define sonss_jumpspeed 0x680
#define sonss_gravity (0x38 - 0xE)

static bool ss_solid(Obj *o, int at) {
  uint8_t b = sslayout[at];
  if (!b || b == SS_1Up || (b >= SS_Ring && b < SS_Glass_Ani1)) return false;
  OB(o, sonss_touchedblock_id) = b;
  OUW(o, sonss_touchedblock) = (uint16_t)at;
  return true;
}

/* a wall in the 2 by 2 blocks at (y, x) (16.16)? (the last one found is the one touched) */
static bool SonicSS_FindWall(Obj *o, int32_t y, int32_t x) {
  int at = (uint16_t)((y >> 16) + 20 + ss_blocksize * 2) / ss_blocksize * ss_rowlength +
           (uint16_t)((x >> 16) + 20) / ss_blocksize;
  if (at < 0 || at + ss_rowlength + 1 >= (int)sizeof sslayout) return false;
  bool d5 = false;
  d5 |= ss_solid(o, at);
  d5 |= ss_solid(o, at + 1);
  d5 |= ss_solid(o, at + ss_rowlength);
  d5 |= ss_solid(o, at + ss_rowlength + 1);
  return d5;
}

static void SS_FixCamera(Obj *o) {
  if ((uint16_t)o->x.w.i >= 320 / 2) V.cam[0].x.w.i = (int16_t)(o->x.w.i - 320 / 2);
  if ((uint16_t)o->y.w.i >= 224 / 2) V.cam[0].y.w.i = (int16_t)(o->y.w.i - 224 / 2);
}

static void SonicSS_Move(Obj *o) {
  int16_t d0;
  if (V.jpadhold2 & btnL) {
    o->status |= 1;
    d0 = o->inertia;
    if (d0 <= 0) {
      d0 = (int16_t)(d0 - sonss_acceleration);
      if (d0 <= -sonss_maxspeed) d0 = -sonss_maxspeed;
    } else d0 = (int16_t)(d0 - sonss_deceleration);
    o->inertia = d0;
  }
  if (V.jpadhold2 & btnR) {
    o->status &= (uint8_t)~1;
    d0 = o->inertia;
    if (d0 >= 0) {
      d0 = (int16_t)(d0 + sonss_acceleration);
      if (d0 >= sonss_maxspeed) d0 = sonss_maxspeed;
    } else d0 = (int16_t)(d0 + sonss_deceleration);
    o->inertia = d0;
  }
  if (!(V.jpadhold2 & (btnL | btnR)) && (d0 = o->inertia) != 0) {
    /* slowing to a stop */
    if (d0 > 0) d0 = (uint16_t)d0 < sonss_acceleration ? 0 : (int16_t)(d0 - sonss_acceleration);
    else d0 = (uint16_t)d0 >= 0x10000 - sonss_acceleration ? 0 : (int16_t)(d0 + sonss_acceleration);
    o->inertia = d0;
  }
  /* along the floor, whichever way is down */
  int16_t cos, sin = CalcSine((uint8_t)-(((V.ssangle >> 8) + 0x20) & 0xC0), &cos);
  int32_t dx = (int32_t)cos * o->inertia, dy = (int32_t)sin * o->inertia;
  o->x.l += dx;
  o->y.l += dy;
  if (SonicSS_FindWall(o, o->y.l, o->x.l)) {
    o->x.l -= dx;
    o->y.l -= dy;
    o->inertia = 0;
  }
}

static void SonicSS_Jump(Obj *o) {
  if (!(V.jpadpress2 & btnABC)) return;
  int16_t cos, sin = CalcSine((uint8_t)(-((V.ssangle >> 8) & 0xFC) - 0x40), &cos);
  o->vx = (int16_t)((int32_t)cos * sonss_jumpspeed >> 8);
  o->vy = (int16_t)((int32_t)sin * sonss_jumpspeed >> 8);
  o->status |= status_in_air;
}

/* gravity towards the stage's down: stopped by walls, landing on the floor */
static void SonicSS_Fall(Obj *o) {
  int32_t d2 = o->y.l, d3 = o->x.l;
  int16_t cos, sin = CalcSine((uint8_t)((V.ssangle >> 8) & 0xFC), &cos);
  int32_t d0 = (int32_t)sin * sonss_gravity + ((int32_t)o->vx << 8);
  int32_t d1 = (int32_t)cos * sonss_gravity + ((int32_t)o->vy << 8);
  d3 += d0;
  if (SonicSS_FindWall(o, d2, d3)) {
    d3 -= d0;
    d0 = 0;
    o->vx = 0;
    o->status &= (uint8_t)~status_in_air;
    d2 += d1;
    if (SonicSS_FindWall(o, d2, d3)) {
      o->vy = 0;
      return;
    }
  } else {
    d2 += d1;
    if (!SonicSS_FindWall(o, d2, d3)) {
      o->vx = (int16_t)(d0 >> 8);
      o->vy = (int16_t)(d1 >> 8);
      o->status |= status_in_air;
      return;
    }
    d1 = 0;
    o->vy = 0;
    o->status &= (uint8_t)~status_in_air;
  }
  o->vx = (int16_t)(d0 >> 8);
  o->vy = (int16_t)(d1 >> 8);
}

/* the block Sonic is over: rings, a 1-up, the emerald, ghost blocks */
static void SonicSS_ChkItems_NonSolid(Obj *o) {
  int at = (uint16_t)(o->y.w.i + 80) / ss_blocksize * ss_rowlength + (uint16_t)(o->x.w.i + 32) / ss_blocksize;
  if (at < 0 || at >= (int)sizeof sslayout) return;
  uint8_t d4 = sslayout[at];
  SSAnim *a;
  if (!d4) {
    if (!OB(o, sonss_ghoststate)) return;
    if (OB(o, sonss_ghoststate) == 2) {
      /* through the ghost blocks and past the trigger: they are solid now */
      for (int r = 0; r < 0x40; r++)
        for (int c = 0; c < 0x40; c++) {
          uint8_t *b = &sslayout[SS_ACTUAL + r * ss_rowlength + c];
          if (*b == SS_Ghost) *b = SS_RedWhite;
        }
    }
    OB(o, sonss_ghoststate) = 0;
    return;
  }
  if (d4 == SS_Ring) {
    if ((a = SS_FindFreeAnimationSlot())) a->id = 1, a->block = (uint16_t)at;
    CollectRing();
    if (V.rings >= 50 && !(V.lifecount & 1)) {
      V.lifecount |= 1;
      V.continues++;
    }
    return;
  }
  if (d4 == SS_1Up) {
    if ((a = SS_FindFreeAnimationSlot())) a->id = 3, a->block = (uint16_t)at;
    V.lives++;
    V.f_lifecount++;
    return;
  }
  if (d4 >= SS_Emerald1_Blue && d4 <= SS_Emerald6_Grey) {
    if ((a = SS_FindFreeAnimationSlot())) a->id = 5, a->block = (uint16_t)at;
    if (V.emeralds != 6) V.emldlist[V.emeralds++] = (uint8_t)(d4 - SS_Emerald1_Blue);
    return;
  }
  if (d4 == SS_Ghost) OB(o, sonss_ghoststate) = 1;
  if (d4 == SS_InvGhostTrigger && OB(o, sonss_ghoststate) == 1) OB(o, sonss_ghoststate) = 2;
}

/* the block Sonic ran into: bumpers, GOAL, UP and DOWN, R, glass */
static void SonicSS_ChkItems_Solid(Obj *o) {
  uint8_t d0 = OB(o, sonss_touchedblock_id);
  uint16_t at = OUW(o, sonss_touchedblock);
  SSAnim *a;
  if (!d0) {
    if ((int8_t)--OB(o, sonss_timeout_updown) < 0) OB(o, sonss_timeout_updown) = 0;
    if ((int8_t)--OB(o, sonss_timeout_r) < 0) OB(o, sonss_timeout_r) = 0;
    return;
  }
  switch (d0) {
  case SS_Bumper: {
    /* bounced off, away from its middle */
    int16_t bx = (int16_t)((at & (ss_rowlength - 1)) * ss_blocksize - 20);
    int16_t by = (int16_t)(((at >> 7) & (ss_rowlength - 1)) * ss_blocksize - 20 - ss_blocksize * 2);
    int16_t cos, sin = CalcSine(CalcAngle((int16_t)(bx - o->x.w.i), (int16_t)(by - o->y.w.i)), &cos);
    o->vx = (int16_t)((int32_t)cos * -0x700 >> 8);
    o->vy = (int16_t)((int32_t)sin * -0x700 >> 8);
    o->status |= status_in_air;
    if ((a = SS_FindFreeAnimationSlot())) a->id = 2, a->block = at;
    return;
  }
  case SS_GOAL: o->routine += 2; return;
  case SS_UP:
    if (OB(o, sonss_timeout_updown)) return;
    OB(o, sonss_timeout_updown) = ss_timeout;
    if (V.ssrotate & 0x40) {
      V.ssrotate = (int16_t)(V.ssrotate * 2);
      sslayout[at] = SS_DOWN;
    }
    return;
  case SS_DOWN:
    if (OB(o, sonss_timeout_updown)) return;
    OB(o, sonss_timeout_updown) = ss_timeout;
    if (!(V.ssrotate & 0x40)) {
      V.ssrotate = (int16_t)(V.ssrotate >> 1);
      sslayout[at] = SS_UP;
    }
    return;
  case SS_R:
    if (OB(o, sonss_timeout_r)) return;
    OB(o, sonss_timeout_r) = ss_timeout;
    if ((a = SS_FindFreeAnimationSlot())) a->id = 4, a->block = at;
    V.ssrotate = (int16_t)-V.ssrotate;
    return;
  case SS_Glass1_Blue:
  case SS_Glass2_Green:
  case SS_Glass3_Yellow:
  case SS_Glass4_Pink:
    /* each touch: the next colour, the last one gone */
    if ((a = SS_FindFreeAnimationSlot())) {
      a->id = 6;
      a->block = at;
      uint8_t n = (uint8_t)(sslayout[at] + 1);
      a->next = n > SS_Glass4_Pink ? 0 : n;
    }
    return;
  }
}

void SonicSpecial(Obj *o) {
  switch (o->routine) {
  case 0:
    o->routine += 2;
    o->height = 14;
    o->width = 7;
    o->map = map_get(MAP_Sonic);
    o->gfx = ArtTile_Sonic;
    o->render = sprite_cam_field;
    o->priority = 0;
    o->anim = id_Roll;
    o->status |= status_rolling | status_in_air;
    /* fall through */
  case 2:
    OB(o, sonss_touchedblock_id) = 0;
    if (!(o->status & status_in_air)) SonicSS_Jump(o);
    SonicSS_Move(o);
    SonicSS_Fall(o);
    SonicSS_ChkItems_NonSolid(o);
    SonicSS_ChkItems_Solid(o);
    SpeedToPos(o);
    SS_FixCamera(o);
    V.ssangle = (int16_t)(V.ssangle + V.ssrotate);
    Sonic_Animate(o);
    Sonic_LoadGfx(o);
    DisplaySprite(o);
    return;
  case 4:
    /* the stage spinning faster and faster, then away */
    V.ssrotate = (int16_t)(V.ssrotate + ss_rotatespeed);
    if (V.ssrotate == 0x60 * ss_rotatespeed) V.gamemode = id_Level;
    if (V.ssrotate >= 2 * 0x60 * ss_rotatespeed) {
      V.ssrotate = 0;
      V.ssangle = 0x4000;
      o->routine += 2;
      OW(o, sonss_exittimer) = 60;
    }
    V.ssangle = (int16_t)(V.ssangle + V.ssrotate);
    break;
  case 6:
    if (!--OW(o, sonss_exittimer)) V.gamemode = id_Level;
    break;
  }
  Sonic_Animate(o);
  Sonic_LoadGfx(o);
  SS_FixCamera(o);
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ the results */
#define ssr_mainX 0x30

void SSResult(Obj *o) {
  static const struct {
    int16_t x, mainx, y;
    uint8_t routine, frame;
  } items[5] = {
    {0x20, 0x120, 0xC4, 2, 0}, {0x320, 0x120, 0x118, 2, 1}, {0x360, 0x120, 0x128, 2, 2},
    {0x1EC, 0x11C, 0xC4, 2, 3}, {0x3A0, 0x120, 0x138, 2, 6},
  };
  switch (o->routine) {
  case 0: {
    if (plc_pending()) return;
    int n = V.rings >= 50 ? 5 : 4;
    for (int i = 0; i < n; i++) {
      Obj *a1 = o + i;
      a1->id = id_SSResult;
      a1->x.w.i = items[i].x;
      OW(a1, ssr_mainX) = items[i].mainx;
      obScreenY(a1) = items[i].y;
      a1->routine = items[i].routine;
      a1->frame = items[i].frame;
      a1->map = map_get(MAP_SSR);
      a1->gfx = ArtTile_Title_Card | Tile_Prio;
      a1->render = sprite_cam_screen;
    }
    uint8_t f = 7; /* "SPECIAL STAGE", "CHAOS EMERALDS" or "SONIC GOT THEM ALL" */
    if (V.emeralds) {
      f = 0;
      if (V.emeralds == 6) {
        f = 8;
        o->x.w.i = 0x18;
        OW(o, ssr_mainX) = 0x118;
      }
    }
    o->frame = f;
  }
    /* fall through */
  case 2: {
    int16_t d1 = 0x10, d0 = OW(o, ssr_mainX);
    if (d0 == o->x.w.i) {
      if (o->frame == 2) {
        o->routine += 2;
        set_timew(o, 3 * 60);
        OBJ(32)->id = id_SSRChaos;
        goto wait;
      }
    } else {
      if (d0 < o->x.w.i) d1 = -d1;
      o->x.w.i = (int16_t)(o->x.w.i + d1);
    }
    if (o->x.w.i >= 0 && (uint16_t)o->x.w.i < 0x80 + 320 + 64) DisplaySprite(o);
    return;
  }
  case 4:
  case 8:
  case 0xC:
  case 0x10:
  wait:
    set_timew(o, (uint16_t)(timew(o) - 1));
    if (!timew(o)) o->routine += 2;
    DisplaySprite(o);
    return;
  case 6:
    DisplaySprite(o);
    V.f_endactbonus = 1;
    if (V.ringbonus) {
      V.ringbonus -= 10;
      AddPoints(10);
      return;
    }
    o->routine += 2;
    set_timew(o, 3 * 60);
    if (V.rings >= 50) {
      set_timew(o, 1 * 60);
      o->routine += 4;
    }
    return;
  case 0xA:
  case 0x12:
    V.f_restart = 1;
    DisplaySprite(o);
    return;
  case 0xE:
    /* the continue got: its icon blinking */
    OBJ(23 + 4)->frame = 4;
    OBJ(23 + 4)->routine = 0x14;
    o->routine += 2;
    set_timew(o, 6 * 60);
    DisplaySprite(o);
    return;
  case 0x14:
    if (!(vblank_byte & 0xF)) o->frame ^= 1;
    DisplaySprite(o);
    return;
  }
}

void SSRChaos(Obj *o) {
  static const int16_t pos[6] = {0x110, 0x128, 0xF8, 0x140, 0xE0, 0x158};
  if (o->routine == 0) {
    if (!V.emeralds) {
      DeleteObject(o);
      return;
    }
    for (int i = 0; i < V.emeralds; i++) {
      Obj *a1 = o + i;
      a1->id = id_SSRChaos;
      a1->x.w.i = pos[i];
      obScreenY(a1) = 0xF0;
      a1->frame = a1->anim = V.emldlist[i];
      a1->routine += 2;
      a1->map = map_get(MAP_SSRC);
      a1->gfx = ArtTile_SS_Results_Emeralds | Tile_Prio;
      a1->render = sprite_cam_screen;
    }
  }
  /* flashing: every other frame the blank frame */
  uint8_t d0 = o->frame;
  o->frame = 6;
  if (d0 == 6) o->frame = o->anim;
  DisplaySprite(o);
}

/* ------------------------------------------------------------------ the game mode */
static void ss_frame(void) {
  MoveSonicInDemo();
  V.jpadhold2 = V.jpadhold1, V.jpadpress2 = V.jpadpress1;
  ExecuteObjects();
  BuildSprites();
  SS_ShowLayout();
  SS_BGAnimate();
}

void GM_Special(void) {
  PaletteWhiteOut();
  ClearScreen();
  ss_fg_shown = ss_bg_shown = -1;
  vdp_load_art(ART_Nem_TitleCard, ArtTile_Title_Card); /* (left from the level: the results' letters) */
  QuickPLC(plcid_SpecialStage);
  clear_level_ram();
  V.f_wtr_state = 0;
  V.f_restart = 0;
  memset(ss_bubbles, 0, sizeof ss_bubbles);
  memset(ss_clouds, 0, sizeof ss_clouds);
  PalLoad_Fade(palid_Special);
  SS_Load();
  V.cam[0].x.l = V.cam[0].y.l = 0;
  player->id = id_SonicSpecial;
  PalCycle_SS();
  V.ssangle = 0;
  V.ssrotate = ss_rotatespeed;
  V.btnpushtime1 = 0;
  V.btnpushtime2 = (uint8_t)(((const uint8_t *)DATA(D_DEMO_INTRO_SPECIAL_STAGE))[1] - 1);
  V.rings = 0;
  V.lifecount = 0;
  V.debuguse = 0;
  V.generictimer = 1800;
  PaletteWhiteIn();
  for (;;) {
    PauseGame();
    V.vblank_routine = id_VBlank_SpecialStage;
    WaitForVBlank();
    ss_frame();
    if (V.f_demo && !V.generictimer) {
      V.gamemode = id_Sega;
      return;
    }
    if (V.gamemode != id_Special) break;
  }
  if (V.f_demo) {
    if (V.gamemode == id_Level) V.gamemode = id_Sega;
    return;
  }
  V.gamemode = id_Level;
  if (ZONE_ACT >= 0x502 + 1) set_zone_act(0);
  /* to white */
  V.generictimer = 60;
  V.pfade_start = 0, V.pfade_size = 0x3F;
  V.palchgspeed = 0;
  while (V.generictimer) {
    V.vblank_routine = id_VBlank_Continue;
    WaitForVBlank();
    ss_frame();
    if (--V.palchgspeed < 0) {
      V.palchgspeed = 2;
      WhiteOut_ToWhite();
    }
  }
  /* the results */
  ClearScreen();
  vdp_load_art(ART_Nem_TitleCard, ArtTile_Title_Card);
  Hud_Base();
  PalLoad(palid_SSResult);
  NewPLC(plcid_Main);
  AddPLC(plcid_SSResult);
  V.f_scorecount = 1;
  V.f_endactbonus = 1;
  V.ringbonus = (uint16_t)(V.rings * 10);
  memset(objs, 0, sizeof objs);
  OBJ(23)->id = id_SSResult;
  do {
    PauseGame();
    V.vblank_routine = id_VBlank_TitleCards;
    WaitForVBlank();
    ExecuteObjects();
    BuildSprites();
    RunPLC();
  } while (!V.f_restart || plc_pending());
  PaletteWhiteOut();
}
