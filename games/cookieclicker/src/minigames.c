/* The minigames' shared parts: their effects on the rest of the game (Game.effs), their frames,
 * their resets; and the hooks the rest of the game calls. */
#include <math.h>
#include <string.h>
#include "game.h"

/* effects the wrinklers read: 1 how much they eat, 2 how often they come */
static float wrinkler_effs[3] = {1, 1, 1};
float minigame_eff(int which) { return wrinkler_effs[which]; }

void minigame_effs(void) {
  for (int i = 0; i < 16; i++) G.effs[i] = 1;
  wrinkler_effs[1] = wrinkler_effs[2] = 1;
  extern void garden_effs(float *effs, float *wrinkler);
  garden_effs(G.effs, wrinkler_effs);
}

void minigames_logic(void) {
  if (S.ascension_mode == 1) return;
  extern void grimoire_logic(void);
  extern void pantheon_logic(void);
  extern void garden_logic(void);
  if (S.buildings[B_WIZARD_TOWER].level > 0) grimoire_logic();
  if (S.buildings[B_TEMPLE].level > 0) pantheon_logic();
  if (S.buildings[B_FARM].level > 0) garden_logic();
}

void reset_minigames(bool hard) {
  extern void grimoire_reset(bool hard);
  extern void pantheon_reset(bool hard);
  extern void garden_reset(bool hard);
  grimoire_reset(hard);
  pantheon_reset(hard);
  garden_reset(hard);
}

void building_levelled(int b) {
  extern void grimoire_on_level(void);
  if (b == B_WIZARD_TOWER) grimoire_on_level();
}

void buff_died(Buff *b) { (void)b; }

/* the cookies the garden's plants drop: their price scales with production (that many minutes of it) */
double garden_drop_price(int u, double price) {
  static const uint16_t drops[] = {U_ELDERWORT_BISCUITS, U_BAKEBERRY_COOKIES, U_DUKETATER_COOKIES,
                                   U_GREEN_YEAST_DIGESTIVES, U_FERN_TEA, U_ICHOR_SYRUP, U_WHEAT_SLIMS};
  for (unsigned i = 0; i < sizeof drops / sizeof drops[0]; i++)
    if (drops[i] == u) return price * G.cookies_ps * 60;
  return price;
}

/* ------------------------------------------------------------------ their panels */
extern void grimoire_draw(void);
extern void grimoire_keys(uint32_t pressed);
extern int grimoire_tip(int x, int y, int w, bool draw);

bool minigame_available(int b) { return b == B_WIZARD_TOWER && S.buildings[b].level > 0 && S.ascension_mode != 1; }
const char *minigame_name(int b) {
  return b == B_WIZARD_TOWER ? "Grimoire" : b == B_TEMPLE ? "Pantheon" : b == B_FARM ? "Garden" : "Stock Market";
}
void draw_minigame(int b) {
  if (b == B_WIZARD_TOWER) grimoire_draw();
}
void minigame_keys(int b, uint32_t pressed) {
  if (b == B_WIZARD_TOWER) grimoire_keys(pressed);
}
/* the tooltip of what is chosen in the panel: its height (0: none) */
int minigame_tip(int b, int x, int y, int w, bool draw) {
  if (b == B_WIZARD_TOWER) return grimoire_tip(x, y, w, draw);
  return 0;
}
