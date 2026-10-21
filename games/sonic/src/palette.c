/* Palettes: loading them (into the colours shown, the fade-in target or their underwater twins),
 * fading to and from black and white a step a frame as the original does (blue first in, red first
 * out), and each zone's colour cycles: Green Hill's water, Labyrinth's waterfalls and conveyor belts,
 * Star Light's lights, Spring Yard's signs and Scrap Brain's blinking machinery. */
#include "s1.h"

static const struct {
  uint32_t data;
  uint8_t dest, n;
} pal_index[] = {
  {D_PAL_SEGA_BACKGROUND, 0, N_PAL_SEGA_BACKGROUND},
  {D_PAL_TITLE_SCREEN, 0, N_PAL_TITLE_SCREEN},
  {D_PAL_LEVEL_SELECT, 0, N_PAL_LEVEL_SELECT},
  {D_PAL_SONIC, 0, N_PAL_SONIC},
  {D_PAL_GREEN_HILL_ZONE, 16, N_PAL_GREEN_HILL_ZONE},
  {D_PAL_LABYRINTH_ZONE, 16, N_PAL_LABYRINTH_ZONE},
  {D_PAL_MARBLE_ZONE, 16, N_PAL_MARBLE_ZONE},
  {D_PAL_STAR_LIGHT_ZONE, 16, N_PAL_STAR_LIGHT_ZONE},
  {D_PAL_SPRING_YARD_ZONE, 16, N_PAL_SPRING_YARD_ZONE},
  {D_PAL_SBZ_ACT_1, 16, N_PAL_SBZ_ACT_1},
  {D_PAL_SPECIAL_STAGE, 0, N_PAL_SPECIAL_STAGE},
  {D_PAL_LABYRINTH_ZONE_UNDERWATER, 0, N_PAL_LABYRINTH_ZONE_UNDERWATER},
  {D_PAL_SBZ_ACT_3, 16, N_PAL_SBZ_ACT_3},
  {D_PAL_SBZ_ACT_3_UNDERWATER, 0, N_PAL_SBZ_ACT_3_UNDERWATER},
  {D_PAL_SBZ_ACT_2, 16, N_PAL_SBZ_ACT_2},
  {D_PAL_SONIC_LZ_UNDERWATER, 0, N_PAL_SONIC_LZ_UNDERWATER},
  {D_PAL_SONIC_SBZ3_UNDERWATER, 0, N_PAL_SONIC_SBZ3_UNDERWATER},
  {D_PAL_SPECIAL_STAGE_RESULTS, 0, N_PAL_SPECIAL_STAGE_RESULTS},
  {D_PAL_SPECIAL_STAGE_CONTINUE_BONUS, 0, N_PAL_SPECIAL_STAGE_CONTINUE_BONUS},
  {D_PAL_ENDING, 0, N_PAL_ENDING},
};

static void load(int id, uint16_t *to) {
  const uint16_t *src = DATAW(pal_index[id].data);
  int n = pal_index[id].n, d = pal_index[id].dest;
  for (int i = 0; i < n && d + i < 64; i++) to[d + i] = src[i];
}
void PalLoad(int id) { load(id, V.palette); }
void PalLoad_Fade(int id) { load(id, V.palette_fading); }
void PalLoad_Fade_Water(int id) { load(id, V.palette_water); }
void PalLoad_Water(int id) { load(id, V.palette_water_fading); }

/* ------------------------------------------------------------------ fading */
static void add_color(uint16_t *a, uint16_t target) {
  uint16_t c = *a;
  if (c == target) return;
  if ((uint16_t)(c + 0x200) <= target) *a = (uint16_t)(c + 0x200);
  else if ((uint16_t)(c + 0x20) <= target) *a = (uint16_t)(c + 0x20);
  else *a = (uint16_t)(c + 2);
}
static void dec_color(uint16_t *a) {
  uint16_t c = *a;
  if (!c) return;
  if (c & 0x00E) *a = (uint16_t)(c - 2);
  else if (c & 0x0E0) *a = (uint16_t)(c - 0x20);
  else if (c & 0xE00) *a = (uint16_t)(c - 0x200);
}
static void white_dec(uint16_t *a, uint16_t target) {
  uint16_t c = *a;
  if (c == target) return;
  if (c >= 0x200 && (uint16_t)(c - 0x200) >= target) *a = (uint16_t)(c - 0x200);
  else if (c >= 0x20 && (uint16_t)(c - 0x20) >= target) *a = (uint16_t)(c - 0x20);
  else *a = (uint16_t)(c - 2);
}
static void white_add(uint16_t *a) {
  uint16_t c = *a;
  if (c == 0xEEE) return;
  if ((c & 0x00E) != 0x00E) *a = (uint16_t)(c + 2);
  else if ((c & 0x0E0) != 0x0E0) *a = (uint16_t)(c + 0x20);
  else if ((c & 0xE00) != 0xE00) *a = (uint16_t)(c + 0x200);
}

void FadeIn_FromBlack(void) {
  int s = V.pfade_start / 2, n = V.pfade_size + 1;
  for (int i = 0; i < n && s + i < 64; i++) add_color(&V.palette[s + i], V.palette_fading[s + i]);
  if (V.zone == id_LZ)
    for (int i = 0; i < n && s + i < 64; i++) add_color(&V.palette_water[s + i], V.palette_water_fading[s + i]);
}
void FadeOut_ToBlack(void) {
  int s = V.pfade_start / 2, n = V.pfade_size + 1;
  for (int i = 0; i < n && s + i < 64; i++) dec_color(&V.palette[s + i]);
  for (int i = 0; i < n && s + i < 64; i++) dec_color(&V.palette_water[s + i]);
}
void WhiteIn_FromWhite(void) {
  int s = V.pfade_start / 2, n = V.pfade_size + 1;
  for (int i = 0; i < n && s + i < 64; i++) white_dec(&V.palette[s + i], V.palette_fading[s + i]);
  if (V.zone == id_LZ)
    for (int i = 0; i < n && s + i < 64; i++) white_dec(&V.palette_water[s + i], V.palette_water_fading[s + i]);
}
void WhiteOut_ToWhite(void) {
  int s = V.pfade_start / 2, n = V.pfade_size + 1;
  for (int i = 0; i < n && s + i < 64; i++) white_add(&V.palette[s + i]);
  for (int i = 0; i < n && s + i < 64; i++) white_add(&V.palette_water[s + i]);
}

static void fade_loop(void (*step)(void)) {
  for (int i = 0; i < 22; i++) {
    V.vblank_routine = id_VBlank_PaletteFade;
    WaitForVBlank();
    step();
    RunPLC();
  }
}
void PalFadeIn_Alt(void) {
  int s = V.pfade_start / 2;
  for (int i = 0; i <= V.pfade_size && s + i < 64; i++) V.palette[s + i] = 0;
  fade_loop(FadeIn_FromBlack);
}
void PaletteFadeIn(void) {
  V.pfade_start = 0, V.pfade_size = 0x3F;
  PalFadeIn_Alt();
}
void PaletteFadeOut(void) {
  V.pfade_start = 0, V.pfade_size = 0x3F;
  fade_loop(FadeOut_ToBlack);
}
void PaletteWhiteIn(void) {
  V.pfade_start = 0, V.pfade_size = 0x3F;
  for (int i = 0; i < 64; i++) V.palette[i] = 0xEEE;
  fade_loop(WhiteIn_FromWhite);
}
void PaletteWhiteOut(void) {
  V.pfade_start = 0, V.pfade_size = 0x3F;
  fade_loop(WhiteOut_ToWhite);
}

/* ------------------------------------------------------------------ colour cycles */
static uint16_t lz_conveyor; /* (the original keeps it in v_pal_buffer) */

static void cycle4(const uint16_t *src, uint16_t *to, uint16_t period) {
  if ((int16_t)--V.pcyc_time >= 0) return;
  V.pcyc_time = (uint16_t)(period - 1);
  int n = V.pcyc_num++ & 3;
  for (int i = 0; i < 4; i++) to[i] = src[n * 4 + i];
}

void PalCycle_Title(void) { cycle4(DATAW(D_PAL_CYCLE_TITLE_SCREEN_WATER), &V.palette[32 + 8], 6); }

static void cycle_lz(void) {
  if ((int16_t)--V.pcyc_time < 0) {
    V.pcyc_time = 2;
    int n = V.pcyc_num++ & 3;
    const uint16_t *src = DATAW(V.act == 3 ? D_PAL_CYCLE_SBZ3_WATERFALL : D_PAL_CYCLE_LZ_WATERFALL);
    for (int i = 0; i < 4; i++) V.palette[32 + 0xB + i] = V.palette_water[32 + 0xB + i] = src[n * 4 + i];
  }
  static const uint8_t seq[8] = {1, 0, 0, 1, 0, 0, 1, 0};
  if (!seq[V.framecount & 7]) return;
  int d = V.f_conveyrev ? -1 : 1, n = (lz_conveyor & 3) + d;
  if ((unsigned)n >= 3) n = d < 0 ? 2 : 0;
  lz_conveyor = (uint16_t)n;
  const uint16_t *dry = DATAW(D_PAL_CYCLE_LZ_CONVEYOR_BELT), *wet = DATAW(D_PAL_CYCLE_LZ_CONVEYOR_BELT_UNDERWATER);
  for (int i = 0; i < 3; i++) V.palette[48 + 0xB + i] = dry[n * 3 + i], V.palette_water[48 + 0xB + i] = wet[n * 3 + i];
}

static void cycle_slz(void) {
  if ((int16_t)--V.pcyc_time >= 0) return;
  V.pcyc_time = 7;
  int n = V.pcyc_num + 1;
  if (n >= 6) n = 0;
  V.pcyc_num = (uint16_t)n;
  const uint16_t *src = DATAW(D_PAL_CYCLE_SLZ) + n * 3;
  V.palette[32 + 0xB] = src[0];
  V.palette[32 + 0xD] = src[1];
  V.palette[32 + 0xE] = src[2];
}

static void cycle_syz(void) {
  if ((int16_t)--V.pcyc_time >= 0) return;
  V.pcyc_time = 5;
  int n = V.pcyc_num++ & 3;
  const uint16_t *by = DATAW(D_PAL_CYCLE_SYZ1) + n * 4, *rw = DATAW(D_PAL_CYCLE_SYZ2) + n * 2;
  for (int i = 0; i < 4; i++) V.palette[48 + 7 + i] = by[i];
  V.palette[48 + 0xB] = rw[0];
  V.palette[48 + 0xD] = rw[1];
}

static uint8_t sbz_buf[0x30];
static void cycle_sbz(void) {
  static const struct {
    uint8_t time, colours;
    uint32_t src;
    uint8_t dest;
  } list1[] = {
    {8, 8, D_PAL_CYCLE_SBZ_1, 32 + 8}, {14, 8, D_PAL_CYCLE_SBZ_2, 32 + 9}, {15, 8, D_PAL_CYCLE_SBZ_3, 48 + 7},
    {12, 8, D_PAL_CYCLE_SBZ_5, 48 + 8}, {8, 8, D_PAL_CYCLE_SBZ_6, 48 + 9}, {29, 16, D_PAL_CYCLE_SBZ_7, 48 + 0xF},
    {4, 3, D_PAL_CYCLE_SBZ_8, 48 + 0xC}, {4, 3, D_PAL_CYCLE_SBZ_8 + 2, 48 + 0xD}, {4, 3, D_PAL_CYCLE_SBZ_8 + 4, 48 + 0xE},
  }, list2[] = {
    {8, 8, D_PAL_CYCLE_SBZ_1, 32 + 8}, {14, 8, D_PAL_CYCLE_SBZ_2, 32 + 9}, {10, 8, D_PAL_CYCLE_SBZ_9, 48 + 8},
    {8, 8, D_PAL_CYCLE_SBZ_6, 48 + 9}, {4, 3, D_PAL_CYCLE_SBZ_8, 48 + 0xC}, {4, 3, D_PAL_CYCLE_SBZ_8 + 2, 48 + 0xD},
    {4, 3, D_PAL_CYCLE_SBZ_8 + 4, 48 + 0xE},
  };
  const __typeof__(list1[0]) *list = V.act ? list2 : list1;
  int n = V.act ? (int)(sizeof list2 / sizeof list2[0]) : (int)(sizeof list1 / sizeof list1[0]);
  for (int i = 0; i < n; i++) {
    uint8_t *b = &sbz_buf[i * 2];
    if ((int8_t)--b[0] >= 0) continue;
    b[0] = (uint8_t)(list[i].time - 1);
    uint8_t c = (uint8_t)(b[1] + 1);
    if (c >= list[i].colours) c = 0;
    b[1] = c;
    V.palette[list[i].dest] = *(const uint16_t *)DATA(list[i].src + (c & 15) * 2u);
  }
  if ((int16_t)--V.pcyc_time >= 0) return;
  const uint16_t *src = DATAW(D_PAL_CYCLE_SBZ_4);
  V.pcyc_time = 1;
  if (V.act) src = DATAW(D_PAL_CYCLE_SBZ_10), V.pcyc_time = 0;
  int d = V.f_conveyrev ? 1 : -1, c = (V.pcyc_num & 3) + d;
  if ((unsigned)c >= 3) c = d < 0 ? 2 : 0;
  V.pcyc_num = (uint16_t)c;
  for (int i = 0; i < 3; i++) V.palette[32 + 0xC + i] = src[c + i];
}

void PaletteCycle(void) {
  switch (V.zone) {
  case id_GHZ: case 6: cycle4(DATAW(D_PAL_CYCLE_GHZ), &V.palette[32 + 8], 6); break;
  case id_LZ: cycle_lz(); break;
  case id_SLZ: cycle_slz(); break;
  case id_SYZ: cycle_syz(); break;
  case id_SBZ: cycle_sbz(); break;
  default: break;
  }
}

void palette_cycles_reset(void) {
  lz_conveyor = 0;
  memset(sbz_buf, 0, sizeof sbz_buf);
}
