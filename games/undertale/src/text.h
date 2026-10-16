/* Undertale: text, typed a letter at a time.
 *
 * The game's texts are written with a few marks:
 *   \n       a new line
 *   |        a new page (after Z)
 *   ^N       a pause of N times 5 frames (^1 .. ^9)
 *   `N       the face's expression N (FACE_* in face.c)
 *   #Y #R #B #O #G #P #L #X   yellow, red, blue, orange, green, purple, light blue, white again
 *   $        the fallen human's name
 *   @A@B     at the end of a page: a choice between A and B (text_choice() says which)
 *   ~        the page closes itself once typed (no Z)
 *
 * A Writer types into a rectangle; the dialogue box is one, with a face or not, at the top or the bottom of
 * the screen. */
#ifndef UT_TEXT_H
#define UT_TEXT_H
#include <stdbool.h>
#include <stdint.h>

enum { FACE_NONE, FACE_TORIEL, FACE_FLOWEY, FACE_SANS, FACE_PAPYRUS, FACE_UNDYNE, FACE_ALPHYS, FACE_ASGORE, FACE_MAX };

typedef struct {
  const char *src;          /* the whole text */
  int page;                 /* where the page being typed starts in src */
  int shown;                /* letters of the page shown */
  int len;                  /* the page's length (up to | or the end) */
  int pause;                /* frames left before the next letter */
  int x, y, line_h;
  int font;
  int speed;                /* frames between letters */
  int t;
  uint8_t face_expr;
  bool done;                /* the page is all typed */
  bool finished;            /* every page read */
  bool auto_close;
  int shake;                /* letters shaking (pixels) */
  int choice, choices;      /* the page's choice: selected, how many (0: none) */
  int choice_at;            /* where the choice's options start in src */
} Writer;

void writer_start(Writer *w, const char *s, int x, int y, int font);
/* one frame: Z goes on, X shows the page at once; returns true while it isn't finished */
bool writer_update(Writer *w, uint32_t pressed);
void writer_draw(const Writer *w, uint16_t color);
int writer_talking(const Writer *w);   /* letters are being typed (a face moves its mouth) */

/* the dialogue box */
void text_open(int face, const char *s);
void text_openf(int face, const char *fmt, ...);
bool text_active(void);
void text_update(uint32_t pressed);
void text_draw(void);
int text_choice(void);     /* 0 or 1: the last choice made (a list's: its index, -1 Back) */
void text_open_menu(const char *const *opts, int n);   /* a list of options in the box */
void text_set_top(int top);   /* -1: by where Frisk is (the game's way), 0 bottom, 1 top */
void text_wrap_width(int w);   /* where the next writer's lines wrap (pixels from its left) */
int text_menu_sel(void);   /* (the tests) a list's selection, 100 + a page's choice, -1 neither open */
int text_menu_cols(void);   /* (the list open: its columns) */
extern char g_name[8];     /* the fallen human's name */
#endif
