/* The rooms' objects and their random encounters. */
#include <stddef.h>
#include "battle.h"
#include "event.h"
#include "events.h"
#include "game.h"
#include "hotland.h"
#include "monsters.h"
#include "newhome.h"
#include "ruins.h"
#include "snowdin.h"
#include "waterfall.h"
#include "world.h"

void room_setup(int room, int from) {
  ruins1_setup(room, from);
  ruins2_setup(room, from);
  ruins3_setup(room, from);
  snow1_setup(room, from);
  snow2_setup(room, from);
  town_setup(room, from);
  wf1_setup(room, from);
  wf2_setup(room, from);
  wf3_setup(room, from);
  ht1_setup(room, from);
  ht2_setup(room, from);
  ht3_setup(room, from);
  res_setup(room, from);
  core_setup(room, from);
  nh_setup(room, from);
}

/* each frame Frisk can walk: what a room watches (a rock on its switch, a cracked floor) */
void room_update(void) {
  ruins2_update();
  snow1_update();
  snow2_update();
  wf1_update();
  wf3_update();
  ht1_update();
  ht2_update();
  ht3_update();
  core_update();
}

/* what a room draws on its floor, under everything (world.c) */
void room_draw_floor(void) {
  snow1_draw();
  snow2_draw();
  ht1_draw();
  ht2_draw();
  ht3_draw();
  core_draw();
}

/* what a room draws over everything (a clock) */
void room_draw_top(void) {
  ht2_draw_top();
  ht3_draw_top();
}

/* a pushed rock that may go where Frisk can't (a bridge seed into the water), and what happens once it has */
bool room_rock_enters(Obj *o, int x, int y) { return wf_seed_enters(o, x, y); }
void room_rock_moved(Obj *o) { wf_seed_moved(o); }

int room_speed(int x, int y) { return ht3_speed(x, y); }

bool room_ice(int x, int y) {
  const RoomInfo *ri = world_room_info();
  if (!ri || !ri->ice) return false;
  int cx = (x - ri->x) >> 2, cy = (y - ri->y) >> 2;
  if (cx < 0 || cy < 0 || cx >= ri->cw || cy >= ri->ch) return false;
  return ri->ice[cy * ((ri->cw + 7) >> 3) + (cx >> 3)] >> (cx & 7) & 1;
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
  /* Snowdin's and Waterfall's are snowmon.c's and wfmon.c's (who comes depends on what's happened) */
  int snow = e ? -1 : snow_encounter(g_room);
  if (!e && snow < 0) snow = wf_encounter(g_room);
  if (!e && snow < 0) return;
  if (--steps_left > 0) return;
  steps_left = 100 + rnd_n(200);
  event_start(EV_ENCOUNTER, NULL);
  event_cur_arg(e ? e->forms[rnd_n(e->n)] : snow);
}
