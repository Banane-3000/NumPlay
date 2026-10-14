/* The menus (menu.h). */
#include "game.h"
#include "menu.h"
void menu_title_open(void) { game_new("Frisk"); }
void menu_open(void) {}
void menu_save_open(void) {}
void menu_update(uint32_t keys, uint32_t pressed) { (void)keys, (void)pressed; }
void menu_draw(void) {}
