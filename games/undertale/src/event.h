/* Undertale: events, the game's cutscenes and talks.
 *
 * An event is a C function called once a frame until it says it is done;
 * the macros below make it read like the game's scripts (each SAY waits for
 * its text, each WAIT for its frames). They are protothreads on GCC's labels
 * as values (a wait can be anywhere, in a switch too). The function's locals
 * don't last over a wait: what has to is kept in c->v. One event runs at a
 * time; while it does, Frisk doesn't move unless it lets him. */
#ifndef UT_EVENT_H
#define UT_EVENT_H
#include <stdbool.h>
#include "text.h"
#include "world.h"

typedef struct Ev {
  int id, t;
  void *at;                  /* where it waits (NULL: from the start) */
  bool free;                 /* Frisk walks while it runs (FREE()), the others stop him */
  int v[6];
  Obj *self;
} Ev;

#define EV_CAT2(a, b) a##b
#define EV_CAT(a, b) EV_CAT2(a, b)
#define EV_BEGIN do { if (c->at) goto *c->at; } while (0); {
#define EV_END } return 1
#define EV_FALL __attribute__((fallthrough))
#define EV_WAIT_(cond, n) do { c->at = &&EV_CAT(ev_at_, n); EV_CAT(ev_at_, n): if (!(cond)) return 0; } while (0)
#define WAIT_UNTIL(cond) EV_WAIT_(cond, __COUNTER__)
#define EV_WAITN_(N, n) do { c->t = (N); c->at = &&EV_CAT(ev_at_, n); EV_CAT(ev_at_, n): if (c->t-- > 0) return 0; } while (0)
#define WAIT(N) EV_WAITN_(N, __COUNTER__)
#define SAY(face, s) do { text_open(face, s); WAIT_UNTIL(!text_active()); } while (0)
#define SAYF(face, ...) do { text_openf(face, __VA_ARGS__); WAIT_UNTIL(!text_active()); } while (0)
#define NARRATE(s) SAY(FACE_NONE, s)
#define CHOICE() text_choice()
#define FREE(b) (c->free = (b))
/* a list to choose from in the dialogue box (the phone's): CHOOSE waits, CHOSEN() is -1 for Back */
#define CHOOSE(opts, n) do { text_open_menu(opts, n); WAIT_UNTIL(!text_active()); } while (0)
#define CHOSEN() text_choice()
/* an object walks to (x, y) at sp pixels a frame; WALK waits for it */
#define WALK_TO(o, X, Y, SP) do { (o)->tx = (int16_t)(X), (o)->ty = (int16_t)(Y), (o)->speed = (uint8_t)(SP), (o)->walking = 1; } while (0)
#define WALK(o, X, Y, SP) do { WALK_TO(o, X, Y, SP); WAIT_UNTIL(!(o)->walking); } while (0)
/* Frisk walks to (x, y), as a cutscene moves him */
#define PL_WALK(X, Y, SP) do { pl_walk_to(X, Y, SP); WAIT_UNTIL(!pl_walking()); } while (0)
#define BATTLE(id) do { battle_start(id); WAIT_UNTIL(battle_over()); } while (0)
#define FADE_TO(r, X, Y, D) do { world_fade_to(r, X, Y, D); WAIT_UNTIL(!world_fading()); } while (0)

void event_start(int id, Obj *self);
bool event_running(void);
bool event_blocking(void);   /* running, and Frisk can't walk */
void event_update(void);
void event_cancel(void);
void event_cur_arg(int v);   /* the event just started: its v[0] */
extern int g_exclaim;        /* frames of the "!" over Frisk (an encounter) */
int event_run(Ev *c);     /* events.c: the events, by id */
void pl_walk_to(int x, int y, int sp);
bool pl_walking(void);
#endif
