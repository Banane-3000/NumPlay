/* Undertale: the events, by id. */
#ifndef UT_EVENTS_H
#define UT_EVENTS_H
/* who is who in a room (Obj.name), for the events to find */
enum {
  ACTOR_NONE, ACTOR_TORIEL, ACTOR_FLOWEY, ACTOR_DUMMY, ACTOR_DOOR, ACTOR_SPIKES, ACTOR_SPIKES2, ACTOR_FROGGIT,
  ACTOR_PILLAR, ACTOR_NAPSTA, ACTOR_ROCK, ACTOR_ROCK2, ACTOR_ROCK3, ACTOR_SWITCH, ACTOR_SWITCH2, ACTOR_SWITCH3,
  ACTOR_CANDY, ACTOR_TRIGGER, ACTOR_TRIGGER2, ACTOR_ITEM, ACTOR_CHAIR, ACTOR_PIE, ACTOR_ANY
};

enum {
  EV_NONE,
  EV_SIGN,          /* a sign or anything that only says its text (self->arg: the text, in events.c) */
  EV_PHONE_MENU,    /* CELL: calling Toriel */
  EV_SAVE,          /* a SAVE Point */
  EV_ENCOUNTER,     /* a random encounter: v[0] its formation */
  EV_FLOWEY_MEET, EV_RUINS1_TORIEL, EV_RUINS2_TORIEL, EV_RUINS3_TORIEL, EV_RUINS3_SWITCH, EV_RUINS4_TORIEL,
  EV_RUINS4_DUMMY, EV_RUINS5_TORIEL, EV_RUINS6_TORIEL, EV_RUINS6_PILLAR, EV_RUINS6_REVEAL, EV_RUINS7_CALL,
  EV_RUINS7_FROGGIT,
  /* ruins2.c */
  EV_FALLING, EV_COLOR_SWITCH, EV_CANDY, EV_CALL, EV_ROCK_TALK, EV_ROCK_OFF, EV_NAPSTA_SLEEP, EV_NAPSTA_LIE, EV_WEB,
  EV_FROGGIT, EV_ITEM, EV_ONE_SWITCH, EV_VEGGIE, EV_TORIEL_TREE,
  /* ruins3.c */
  EV_HOME_TOUR, EV_TORIEL_CHAIR, EV_BED, EV_STAIRS, EV_BASEMENT_TALK, EV_TORIEL_DOOR, EV_FLOWEY_END, EV_RUINS_DOOR,
  EV_COUNT
};
/* Toriel's calls on the way (EV_CALL's v[0]) */
enum { CALL_FLAVOR, CALL_FLAVOR2, CALL_ALLERGY, CALL_ITEMS };
/* a text when Frisk comes in (a room's setup) */
void say_on_entry(const char *text);
#endif
