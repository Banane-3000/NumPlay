/* The Pantheon (the temple's minigame): three slots, diamond, ruby and jade, for eleven spirits. */
#include <math.h>
#include <string.h>
#include "game.h"

void pantheon_logic(void) {
  if (S.swap_t > 0) S.swap_t--;
}
void pantheon_reset(bool hard) {
  if (hard) memset(S.gods, 0, sizeof S.gods), S.swaps = 3, S.swap_t = 0;
}
/* Holobore leaves its slot when a golden cookie is clicked */
bool force_unslot_god(int god) {
  for (int i = 0; i < 3; i++)
    if (S.gods[i] == god + 1 && hasgod(god)) {
      S.gods[i] = 0;
      G.recalculate = true;
      return true;
    }
  return false;
}
void use_swap(int32_t t) {
  if (S.swaps > 0) S.swaps--;
  S.swap_t = t;
}
