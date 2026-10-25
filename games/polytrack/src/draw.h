/* The screens (draw.c): the title, the tracks, the car's colour, how to play, the race's timer and speed, the pause,
 * the finish. PolyTrack's dark blue slanted boxes, each screen with a line of plain keys at the bottom. */
#ifndef PT_DRAW_H
#define PT_DRAW_H
#include "car.h"
#include "render.h"
#include "save.h"

enum {
  V_TITLE, V_TRACKS, V_CAR, V_GUIDE, V_HELP, V_RACE, V_PAUSE, V_LEAVE, V_RESULT, V_QUIT, V_SCREENS
};
enum { PAUSE_RESUME, PAUSE_RESTART, PAUSE_HELP, PAUSE_QUIT, PAUSE_ROWS };
enum { MENU_PLAY, MENU_CAR, MENU_HELP, MENU_ROWS };
enum { RES_AGAIN, RES_NEXT, RES_TRACKS, RES_ROWS };
#define PT_HELP_PAGES 3

typedef struct {
  uint8_t screen, sel, page, under;  /* under: where Back goes from how to play */
  uint8_t tab;                       /* the tracks' tab: Summer, Winter, Desert */
  uint32_t clock;                    /* ms, for what moves */
  bool save_failed;
  /* the race */
  uint8_t started, new_record, has_ghost;
  uint32_t split_at;                 /* the clock when a checkpoint was taken (its times show 3.5 s) */
  uint16_t split_cp;
  int32_t split_diff;                /* ms against the record, or a big number when there is none */
  uint32_t still_ms;                 /* under 50 km/h since */
  uint32_t finish_at;                /* the clock at the finish */
  int32_t finish_diff;
  uint8_t slow;                      /* the picture's detail went down */
} pt_ui;

extern const char *const pt_env_name[3];
/* the whole screen */
void pt_render(const pt_file *f, const car_t *car, const camera_t *cam, const car_look *look, const car_look *ghost,
               const pt_ui *ui);
int track_of_tab(int tab, int row);  /* the track at a row of a tab, or -1 */
int tab_rows(int tab);
#endif
