/* Undertale: the menus (title, naming, the C menu, SAVE). */
#ifndef UT_MENU_H
#define UT_MENU_H
#include <stdint.h>
void menu_title_open(void);
void menu_open(void);
void menu_save_open(void);
void menu_gameover_open(void);
void menu_end_open(const char *text);   /* the end: its words, a page at a time */
void menu_update(uint32_t keys, uint32_t pressed);
void menu_draw(void);
void game_new(const char *name);
#endif
