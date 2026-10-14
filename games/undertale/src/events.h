/* Undertale: the events, by id. */
#ifndef UT_EVENTS_H
#define UT_EVENTS_H
enum {
  EV_NONE,
  EV_SIGN,          /* a sign or anything that only says its text (self->arg: the text, in events.c) */
  EV_PHONE_MENU,    /* CELL: calling Toriel */
  EV_SAVE,          /* a SAVE Point */
  EV_COUNT
};
#endif
