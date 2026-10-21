/* The HUD: SCORE, TIME and RINGS (the words flash red when there are no rings or the time nears
 * ten minutes) and the lives counter. Their numbers are art: each digit's two tiles are put into
 * video memory where the HUD's sprite shows them, as the original writes them there in VBlank.
 * The clock runs here too: at 9:59 the time is over. */
#include "s1.h"
#include "game.h"

extern const uint8_t vdp_blank_pattern[32];
#define ART(a) ((const uint8_t *)DATA(D_TILES) + (uint32_t)DATAW(D_ARTS)[(a) * 2] * 32)

void HUD(Obj *o) {
  if (o->routine == 0) {
    o->routine = 2;
    o->x.w.i = 0x80 + 0x10;
    obScreenY(o) = 0x80 + 0x88;
    o->map = map_get(MAP_HUD);
    o->gfx = ArtTile_HUD;
    o->render = 0;
    o->priority = 0;
  }
  if (V.rings) {
    o->frame = 0;
    DisplaySprite(o);
    return;
  }
  uint8_t f = 0;
  if (!(V.framecount & 8)) {
    f = 1;
    if (V.timemin == 9) f += 2;
  }
  o->frame = f;
  DisplaySprite(o);
}

/* an 8x16 digit (tile index d of the HUD numbers, and the next) at tile t */
static void put16(int t, int d) {
  const uint8_t *a = ART(ART_Art_Hud);
  if (d < 0) vram[t] = vram[t + 1] = vdp_blank_pattern;
  else vram[t] = a + d * 32, vram[t + 1] = a + (d + 1) * 32;
}

static const uint32_t powers[6] = {100000, 10000, 1000, 100, 10, 1};

/* n digits of v at tile t, two tiles each; lead: leading zeros shown */
static void digits16(int t, uint32_t v, int first, int n, bool lead, bool clear) {
  bool started = lead;
  for (int i = 0; i < n; i++) {
    uint32_t p = powers[first + i];
    int d = 0;
    while (v >= p) v -= p, d++;
    if (d) started = true;
    if (started) put16(t + i * 2, d * 2);
    else if (clear) put16(t + i * 2, -1);
  }
}

static void Hud_Lives(void) {
  const uint8_t *a = ART(ART_Art_LivesNums);
  uint32_t v = V.lives;
  int tens = 0;
  while (v >= 10) v -= 10, tens++;
  vram[ArtTile_Lives_Counter_Num] = tens ? a + tens * 32 : vdp_blank_pattern;
  vram[ArtTile_Lives_Counter_Num + 2] = a + v * 32;
}

void Hud_Base(void) {
  Hud_Lives();
  static const int8_t base[15] = {0x16, -1, -1, -1, -1, -1, -1, 0, 0, 0x14, 0, 0, -1, -1, 0};
  for (int i = 0; i < 15; i++) put16(ArtTile_HUDScore_E + i * 2, base[i]);
}

static void Hud_ResetRings(void) {
  static const int8_t base[3] = {-1, -1, 0};
  for (int i = 0; i < 3; i++) put16(ArtTile_HUDRings + i * 2, base[i]);
}

void HUD_Update(void) {
  if (V.f_scorecount) {
    V.f_scorecount = 0;
    digits16(ArtTile_HUDScore, V.score, 0, 6, false, false);
  }
  if (V.f_ringcount) {
    if (V.f_ringcount & 0x80) Hud_ResetRings();
    V.f_ringcount = 0;
    digits16(ArtTile_HUDRings, V.rings, 3, 3, false, false);
  }
  if (V.f_timecount && !V.f_pause) {
    if (V.timemin == 9 && V.timesec == 59 && V.timecent == 59) {
      V.f_timecount = 0;
      KillSonic(player, player);
      V.f_timeover = 1;
      return;
    }
    if (++V.timecent >= 60) {
      V.timecent = 0;
      if (++V.timesec >= 60) {
        V.timesec = 0;
        if (++V.timemin >= 9) V.timemin = 9;
      }
      digits16(ArtTile_HUDTimeMins, V.timemin, 5, 1, true, false);
      digits16(ArtTile_HUDTimeSecs, V.timesec, 4, 2, true, false);
    }
  }
  if (V.f_lifecount) {
    V.f_lifecount = 0;
    Hud_Lives();
  }
  if (V.f_endactbonus) {
    V.f_endactbonus = 0;
    digits16(ArtTile_Bonuses, V.timebonus, 2, 4, false, true);
    digits16(ArtTile_Bonuses + 8, V.ringbonus, 2, 4, false, true);
  }
}

/* the continue screen's countdown */
void ContScrCounter(uint32_t v) { digits16(ArtTile_Continue_Number, v, 4, 2, true, false); }
