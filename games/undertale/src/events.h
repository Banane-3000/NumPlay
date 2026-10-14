/* Undertale: the events, by id. */
#ifndef UT_EVENTS_H
#define UT_EVENTS_H
enum {
  EV_NONE,
  EV_SIGN,          /* a sign or anything that only says its text (self->arg: the text, in events.c) */
  EV_COUNT
};
#endif
