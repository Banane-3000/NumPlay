/* The rooms' objects and their random encounters. */
#include <stddef.h>
#include "battle.h"
#include "event.h"
#include "events.h"
#include "game.h"
#include "monsters.h"
#include "ruins.h"
#include "world.h"

void room_setup(int room, int from) {
  ruins1_setup(room, from);
  ruins2_setup(room, from);
  ruins3_setup(room, from);
}

/* each frame Frisk can walk: what a room watches (a rock on its switch, a cracked floor) */
void room_update(void) {
  ruins2_update();
}

/* random encounters: a room's formations and how many steps between them (the game counts its steps,
 * then a "!" shows over Frisk) */
typedef struct {
  uint8_t room;
  uint8_t forms[6];
  uint8_t n;
} Encounters;
static const Encounters enc[] = {
  {ROOM_RUINS7, {FORM_FROGGIT, FORM_WHIMSUN}, 2},
  {ROOM_RUINS8, {FORM_FROGGIT, FORM_WHIMSUN}, 2},
  {ROOM_RUINS9, {FORM_FROGGIT, FORM_WHIMSUN}, 2},
  {ROOM_RUINS10, {FORM_FROGGIT, FORM_WHIMSUN, FORM_FROGGIT_WHIMSUN, FORM_FROGGIT2, FORM_MOLDSMAL}, 5},
  {ROOM_RUINS11, {FORM_FROGGIT_WHIMSUN, FORM_FROGGIT2, FORM_MOLDSMAL, FORM_MOLDSMAL2, FORM_MOLDSMAL3}, 5},
  {ROOM_RUINS15, {FORM_LOOX, FORM_VEGETOID, FORM_LOOX_VEG_MIGOSP, FORM_VEGETOID_MIGOSP, FORM_MOLDSMAL_MIGOSP, FORM_LOOX2}, 6},
  {ROOM_RUINS16, {FORM_LOOX, FORM_VEGETOID, FORM_MOLDSMAL_MIGOSP, FORM_VEGETOID2}, 4},
  {ROOM_RUINS17A, {FORM_LOOX, FORM_VEGETOID, FORM_MOLDSMAL_MIGOSP}, 3},
  {ROOM_RUINS17B, {FORM_LOOX, FORM_VEGETOID, FORM_MOLDSMAL_MIGOSP}, 3},
  {ROOM_RUINS17C, {FORM_LOOX, FORM_VEGETOID, FORM_MOLDSMAL_MIGOSP}, 3},
  {ROOM_RUINS17D, {FORM_LOOX, FORM_VEGETOID, FORM_MOLDSMAL_MIGOSP}, 3},
};
static int steps_left = 120;
bool g_no_encounters;   /* (the tests' --noenc) */

void encounter_step(void) {
  if (!pl.moving || g_no_encounters) return;
  const Encounters *e = NULL;
  for (unsigned i = 0; i < sizeof enc / sizeof enc[0]; i++)
    if (enc[i].room == g_room) e = &enc[i];
  if (!e) return;
  if (--steps_left > 0) return;
  steps_left = 100 + rnd_n(200);
  event_start(EV_ENCOUNTER, NULL);
  event_cur_arg(e->forms[rnd_n(e->n)]);
}
