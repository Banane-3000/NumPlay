/* The Game Boy's picture: VRAM, OAM, palettes, and the line renderer (lcd.c). */
#ifndef __LCD_H__
#define __LCD_H__

#include "defs.h"

struct obj
{
	byte y;
	byte x;
	byte pat;
	byte flags;
};

struct lcd
{
	byte *vbank[2]; /* 8 KB each; the DMG has one, both point to it */
	/* (from here on, what a save state keeps) */
	union
	{
		byte mem[256];
		struct obj obj[40];
	} oam;
	byte pal[128]; /* CGB palette RAM: 8 BG palettes, then 8 OBJ ones, 4 colours of 2 bytes */
	int wline;     /* the window's own line counter */
	int wy_hit;    /* WY matched LY in this frame: the window may show */
};
#define LCD_STATE_AT(l) ((byte *)&(l).oam)
#define LCD_STATE_SIZE (sizeof(struct lcd) - offsetof(struct lcd, oam))

extern struct lcd lcd;

void lcd_begin();
void lcd_refreshline();
void pal_write(int i, byte b);
void pal_dmg_dirty();
void pal_dirty();
void lcd_reset();

#define LCDC_BIT_BG_EN    (1<<0) /* BG display off or on - CGB: always on */
#define LCDC_BIT_OBJ_EN   (1<<1) /* OBJs off or on */
#define LCDC_BIT_OBJ_SIZE (1<<2) /* OBJ size: 0:8x8 1:8x16 */
#define LCDC_BIT_BG_MAP   (1<<3) /* BG code area 0:9800-9bff, 1:9c00-9fff */
#define LCDC_BIT_TILE_SEL (1<<4) /* BG char data 0:8800-97ff, 1:8000-8fff */
#define LCDC_BIT_WIN_EN   (1<<5) /* WIN off or on */
#define LCDC_BIT_WIN_MAP  (1<<6) /* WIN code area 0:9800-9bff, 1:9c00-9fff */
#define LCDC_BIT_LCD_EN   (1<<7) /* LCD controller off or on */


#endif
