/* Battles (battle.h). */
#include "battle.h"
#include "game.h"
static bool over = true;
void battle_start(int id) { (void)id; over = true; }
bool battle_over(void) { return over; }
int battle_result(void) { return BR_SPARED; }
void battle_update(uint32_t keys, uint32_t pressed) { (void)keys, (void)pressed; }
void battle_draw(void) {}
