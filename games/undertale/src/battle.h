/* Undertale: battles (encounters). */
#ifndef UT_BATTLE_H
#define UT_BATTLE_H
#include <stdbool.h>
#include <stdint.h>

void battle_start(int id);
bool battle_over(void);
int battle_result(void);   /* BR_*: how the last one ended */
void battle_update(uint32_t keys, uint32_t pressed);
void battle_draw(void);
enum { BR_WON, BR_SPARED, BR_FLED, BR_DIED, BR_SCRIPTED };
#endif
