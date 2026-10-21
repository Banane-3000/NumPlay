/* The level's animated patterns (AnimateLevelGfx): Green Hill's waterfall and flowers, Marble's
 * lava, magma and torches, Scrap Brain's smoke, the ending's flowers, and the giant ring's art
 * copied in as it appears. Most are frames of art pointed at in place; Marble's magma is made
 * here, each row of its patterns a moving window over a wider picture. */
#include "s1.h"
#include "game.h"
#include "objs.h"

#define ART(a) ((const uint8_t *)DATA(D_TILES) + (uint32_t)DATAW(D_ARTS)[(a) * 2] * 32)
#define ART_N(a) (DATAW(D_ARTS)[(a) * 2 + 1])

/* n patterns of art from byte offset off, to tile t */
static void tiles(int t, const uint8_t *art, uint32_t off, int n) {
  for (int i = 0; i < n; i++) vram[t + i] = art + off + (uint32_t)i * 32;
}

static uint8_t magma[16][32];
const uint8_t *ending_flowers; /* (the ending's flower art, set by GM_Ending) */

static void AniArt_GiantRing(void) {
  if (!V.gfxbigring) return;
  V.gfxbigring = (uint16_t)(V.gfxbigring - 14 * 32);
  tiles(ArtTile_Giant_Ring + V.gfxbigring / 32, ART(ART_Art_BigRing), V.gfxbigring, 14);
}

static void AniArt_GHZ(void) {
  uint8_t(*l)[2] = V.lani;
  if ((int8_t)--l[0][1] < 0) {
    l[0][1] = 6 - 1;
    uint8_t f = l[0][0]++;
    tiles(ArtTile_GHZ_Waterfall, ART(ART_Art_GhzWater), (f & 1) * 8 * 32, 8);
    return;
  }
  if ((int8_t)--l[1][1] < 0) {
    l[1][1] = 16 - 1;
    uint8_t f = l[1][0]++;
    tiles(ArtTile_GHZ_Big_Flower_1, ART(ART_Art_GhzFlower1), (f & 1) * 16 * 32, 16);
    return;
  }
  if ((int8_t)--l[2][1] < 0) {
    static const uint8_t seq[4] = {0, 1, 2, 1};
    l[2][1] = 8 - 1;
    uint8_t d0 = seq[l[2][0]++ & 3];
    if (!(d0 & 1)) l[2][1] = 128 - 1;
    tiles(ArtTile_GHZ_Small_Flower, ART(ART_Art_GhzFlower2), d0 * 384u, 12);
  }
}

static void AniArt_MZ(void) {
  uint8_t(*l)[2] = V.lani;
  if ((int8_t)--l[0][1] < 0) {
    l[0][1] = 20 - 1;
    uint8_t f = (uint8_t)(l[0][0] + 1);
    if (f == 3) f = 0;
    l[0][0] = f;
    tiles(ArtTile_MZ_Animated_Lava, ART(ART_Art_MzLava1), f * 8 * 32u, 8);
  }
  if ((int8_t)--l[1][1] < 0) {
    l[1][1] = 2 - 1;
    const uint8_t *src = ART(ART_Art_MzLava2) + l[0][0] * 0x200u;
    l[1][0]++;
    uint8_t d3 = OSC_B(0xA);
    for (int c = 0; c < 4; c++, d3 += 4) {
      int k = d3 & 15;
      for (int r = 0; r < 32; r++)
        for (int i = 0; i < 4; i++) magma[c * 4 + r / 8][(r & 7) * 4 + i] = src[r * 16 + ((k + i) & 15)];
    }
    for (int t = 0; t < 16; t++) vram[ArtTile_MZ_Animated_Magma + t] = magma[t];
    return;
  }
  if ((int8_t)--l[2][1] < 0) {
    l[2][1] = 8 - 1;
    uint8_t f = l[3][0];
    l[3][0] = (uint8_t)((l[3][0] + 1) & 3);
    tiles(ArtTile_MZ_Torch, ART(ART_Art_MzTorch), f * 6 * 32u, 6);
  }
}

/* the smoke: puff; or the sky clear, and a wait */
static bool sbz_puff(int t, uint8_t *time, uint8_t *frame, uint8_t *wait, uint8_t pause) {
  if ((int8_t)--*time >= 0) return false;
  *time = 8 - 1;
  const uint8_t *a = ART(ART_Art_SbzSmoke);
  int f = (*frame)++ & 7;
  if (f) {
    tiles(t, a, (f - 1) * 12 * 32u, 12);
    return true;
  }
  *wait = pause;
  tiles(t, a, 0, 6);
  tiles(t + 6, a, 0, 6);
  return true;
}

static void AniArt_SBZ(void) {
  uint8_t(*l)[2] = V.lani;
  if (l[2][0]) l[2][0]--;
  else if (sbz_puff(ArtTile_SBZ_Smoke_Puff_1, &l[0][1], &l[0][0], &l[2][0], 3 * 60)) return;
  if (l[2][1]) {
    l[2][1]--;
    return;
  }
  sbz_puff(ArtTile_SBZ_Smoke_Puff_2, &l[1][1], &l[1][0], &l[2][1], 2 * 60);
}

static void AniArt_Ending(void) {
  uint8_t(*l)[2] = V.lani;
  if (!ending_flowers) return;
  if ((int8_t)--l[1][1] < 0) {
    l[1][1] = 8 - 1;
    uint8_t f = l[1][0]++ & 1;
    tiles(ArtTile_GHZ_Big_Flower_1, ART(ART_Art_GhzFlower1), f * 16 * 32u, 16);
    tiles(ArtTile_GHZ_Big_Flower_2, ending_flowers, f * 16 * 32u, 16);
    return;
  }
  if ((int8_t)--l[2][1] < 0) {
    static const uint8_t seq[8] = {0, 0, 0, 1, 2, 2, 2, 1};
    l[2][1] = 8 - 1;
    uint8_t d0 = seq[l[2][0]++ & 7];
    tiles(ArtTile_GHZ_Small_Flower, ART(ART_Art_GhzFlower2), d0 * 384u, 12);
    return;
  }
  static const uint8_t seq34[4] = {0, 1, 2, 1};
  if ((int8_t)--l[4][1] < 0) {
    l[4][1] = 15 - 1;
    uint8_t d0 = seq34[l[4][0]++ & 3];
    tiles(ArtTile_GHZ_Flower_3, ending_flowers, 0x400 + d0 * 0x200u, 16);
    return;
  }
  if ((int8_t)--l[5][1] < 0) {
    l[5][1] = 12 - 1;
    uint8_t d0 = seq34[l[5][0]++ & 3];
    tiles(ArtTile_GHZ_Flower_4, ending_flowers, 0xA00 + d0 * 0x200u, 16);
  }
}

void AnimateLevelGfx(void) {
  if (V.f_pause) return;
  AniArt_GiantRing();
  switch (V.zone) {
  case id_GHZ: AniArt_GHZ(); break;
  case id_MZ: AniArt_MZ(); break;
  case id_SBZ: AniArt_SBZ(); break;
  case 6: AniArt_Ending(); break;
  }
}
