/* Undertale: events, the game's cutscenes and talks.
 *
 * An event is a C function called once a frame until it says it is done;
 * the macros below make it read like the game's scripts (each SAY waits for
 * its text, each WAIT for its frames). Its locals don't last over a wait:
 * what has to is kept in c->v. One event runs at a time; while it does,
 * Frisk doesn't move unless it lets him. */
#ifndef UT_EVENT_H
#define UT_EVENT_H
#include <stdbool.h>
#include "text.h"
#include "world.h"

typedef struct Ev {
  int id, line, t;
  int v[6];
  Obj *self;
} Ev;

#define EV_BEGIN switch (c->line) { case 0:
#define EV_END } return 1
#define EV_YIELD do { c->line = __LINE__; return 0; case __LINE__:; } while (0)
#define WAIT_UNTIL(cond) do { c->line = __LINE__; case __LINE__: if (!(cond)) return 0; } while (0)
#define WAIT(n) do { c->t = (n); c->line = __LINE__; case __LINE__: if (c->t-- > 0) return 0; } while (0)
#define SAY(face, s) do { text_open(face, s); WAIT_UNTIL(!text_active()); } while (0)
#define SAYF(face, ...) do { text_openf(face, __VA_ARGS__); WAIT_UNTIL(!text_active()); } while (0)
#define NARRATE(s) SAY(FACE_NONE, s)
#define CHOICE() text_choice()
/* an object walks to (x, y) at sp pixels a frame; WALK waits for it */
#define WALK_TO(o, X, Y, SP) do { (o)->tx = (int16_t)(X), (o)->ty = (int16_t)(Y), (o)->speed = (uint8_t)(SP), (o)->walking = 1; } while (0)
#define WALK(o, X, Y, SP) do { WALK_TO(o, X, Y, SP); WAIT_UNTIL(!(o)->walking); } while (0)
/* Frisk walks to (x, y), as a cutscene moves him */
#define PL_WALK(X, Y, SP) do { pl_walk_to(X, Y, SP); WAIT_UNTIL(!pl_walking()); } while (0)
#define BATTLE(id) do { battle_start(id); WAIT_UNTIL(battle_over()); } while (0)
#define FADE_TO(r, X, Y, D) do { world_fade_to(r, X, Y, D); WAIT_UNTIL(!world_fading()); } while (0)

void event_start(int id, Obj *self);
bool event_running(void);
void event_update(void);
int event_run(Ev *c);     /* events.c: the events, by id */
void pl_walk_to(int x, int y, int sp);
bool pl_walking(void);
#endif
