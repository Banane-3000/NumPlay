/* The Game Boy's picture, one line at a time, as the PPU builds it: the background and the
 * window from the tile maps, then the sprites, 10 a line at most, sorted the DMG's way (lower X
 * first) or the CGB's (OAM order), each pixel resolved against the background's priority. Lines
 * are handed to Retro-Go's screen in RGB565 as soon as they are drawn.
 *
 * Written for Retro-Go on NumPlay in place of gnuboy's lcd.c, whose 256 KB cache of decoded
 * tiles doesn't fit the calculator: tiles are decoded as they are drawn. GPL-3.0-or-later. */
#include <stddef.h>
#include <string.h>
#include "defs.h"
#include "regs.h"
#include "hw.h"
#include "mem.h"
#include "lcd.h"
#include "gb.h"

struct lcd lcd;
int gb_draw = 1;

static un16 pal565[64]; /* the 8 BG palettes' colours (4 each), then the 8 OBJ palettes' */
static un16 line[160];

/* the DMG palettes: for the background, then the two sprite palettes, lightest first */
static const un32 dmg_colors[GB_PAL_COUNT][3][4] = {
	{{0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F}, {0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F},
	 {0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F}}, /* DMG */
	{{0xC4CFA1, 0x8B956D, 0x4D533C, 0x1F1F1F}, {0xC4CFA1, 0x8B956D, 0x4D533C, 0x1F1F1F},
	 {0xC4CFA1, 0x8B956D, 0x4D533C, 0x1F1F1F}}, /* Pocket */
	{{0x6CF1C3, 0x00B581, 0x008D61, 0x004F3B}, {0x6CF1C3, 0x00B581, 0x008D61, 0x004F3B},
	 {0x6CF1C3, 0x00B581, 0x008D61, 0x004F3B}}, /* Light */
	{{0xFFFFFF, 0x7BFF31, 0x0063C5, 0x000000}, {0xFFFFFF, 0xFF8484, 0x943A3A, 0x000000},
	 {0xFFFFFF, 0xFF8484, 0x943A3A, 0x000000}}, /* GBC, its colours for a black and white game */
	{{0xF7E7C6, 0xD68E49, 0xA63725, 0x331E50}, {0xF7E7C6, 0xD68E49, 0xA63725, 0x331E50},
	 {0xF7E7C6, 0xD68E49, 0xA63725, 0x331E50}}, /* SGB */
};
static int dmg_palette;

static un16 rgb565(un32 c)
{
	return (un16)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
}

void gb_set_palette(int p)
{
	dmg_palette = p >= 0 && p < GB_PAL_COUNT ? p : 0;
	pal_dirty();
}

/* the DMG's three palette registers into colours */
void pal_dmg_dirty()
{
	if (hw.cgb) return;
	const un32 (*c)[4] = dmg_colors[dmg_palette];
	for (int i = 0; i < 4; i++)
	{
		pal565[i] = rgb565(c[0][(R_BGP >> (2 * i)) & 3]);
		pal565[32 + i] = rgb565(c[1][(R_OBP0 >> (2 * i)) & 3]);
		pal565[36 + i] = rgb565(c[2][(R_OBP1 >> (2 * i)) & 3]);
	}
}

/* one CGB colour: 15 bits, red lowest */
static void updatepalette(int i)
{
	int c = (lcd.pal[i << 1] | lcd.pal[(i << 1) | 1] << 8) & 0x7FFF;
	int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
	pal565[i] = (un16)(r << 11 | (g << 1 | g >> 4) << 5 | b);
}

void pal_write(int i, byte b)
{
	i &= 127;
	if (lcd.pal[i] == b) return;
	lcd.pal[i] = b;
	if (hw.cgb) updatepalette(i >> 1);
}

void pal_dirty()
{
	if (hw.cgb)
		for (int i = 0; i < 64; i++) updatepalette(i);
	else
		pal_dmg_dirty();
}

void lcd_begin()
{
	lcd.wline = 0;
	lcd.wy_hit = 0;
}

void lcd_reset()
{
	memset(LCD_STATE_AT(lcd), 0, LCD_STATE_SIZE);
	for (int i = 0; i < 2; i++)
		if (lcd.vbank[i]) memset(lcd.vbank[i], 0, 8192);
	lcd_begin();
	pal_dirty();
}

/* every byte with its bits in the other order */
#define R2(n) n, n + 2 * 64, n + 1 * 64, n + 3 * 64
#define R4(n) R2(n), R2(n + 2 * 16), R2(n + 1 * 16), R2(n + 3 * 16)
#define R6(n) R4(n), R4(n + 2 * 4), R4(n + 1 * 4), R4(n + 3 * 4)
static const byte reversed[256] = {R6(0), R6(2), R6(1), R6(3)};

/* n pixels of a tile map, from its pixel (sx, sy): colour (bits 0-1), CGB palette (2-4) and the
   CGB "BG over sprites" flag (bit 7) */
static void tiles(byte *out, int n, int map, int sx, int sy)
{
	const byte *v0 = lcd.vbank[0], *v1 = lcd.vbank[1];
	const byte *row = v0 + map + ((sy >> 3) & 31) * 32, *attrs = v1 + map + ((sy >> 3) & 31) * 32;
	int signed_ids = !(R_LCDC & LCDC_BIT_TILE_SEL), ty = sy & 7, tx = (sx >> 3) & 31, skip = sx & 7;
	while (n > 0)
	{
		int t = row[tx], attr = hw.cgb ? attrs[tx] : 0;
		int at = (signed_ids ? 0x1000 + (n8)t * 16 : t * 16) + ((attr & 0x40) ? 7 - ty : ty) * 2;
		const byte *data = (attr & 0x08) ? v1 : v0;
		int lo = data[at], hi = data[at + 1], extra = ((attr & 7) << 2) | (attr & 0x80);
		if (attr & 0x20) /* flipped: the lowest bit is the leftmost pixel */
			lo = reversed[lo], hi = reversed[hi];
		for (int i = skip; i < 8 && n > 0; i++, n--)
			*out++ = (byte)((((hi << i) >> 6) & 2) | (((lo << i) >> 7) & 1) | extra);
		skip = 0;
		tx = (tx + 1) & 31;
	}
}

/* what a line was drawn with: kept for each line of the last frame drawn, so that the picture can
   be drawn again under Retro-Go's menus (gb_redraw) */
struct line_regs
{
	byte lcdc, scx, scy, wx, bgp, obp0, obp1, win, wl, drawn;
};
static struct line_regs snap[144];

static void draw_line(int L, const struct line_regs *r)
{
	int lcdc = r->lcdc, wx = r->wx - 7;
	un16 dmg[12]; /* (the DMG's palettes as they were on this line) */
	const un16 *bgpal = pal565, *objpal = pal565 + 32;
	if (!hw.cgb)
	{
		const un32 (*c)[4] = dmg_colors[dmg_palette];
		for (int i = 0; i < 4; i++)
		{
			dmg[i] = rgb565(c[0][(r->bgp >> (2 * i)) & 3]);
			dmg[4 + i] = rgb565(c[1][(r->obp0 >> (2 * i)) & 3]);
			dmg[8 + i] = rgb565(c[2][(r->obp1 >> (2 * i)) & 3]);
		}
		bgpal = dmg, objpal = dmg + 4;
	}

	byte bg[160];
	if (!hw.cgb && !(lcdc & LCDC_BIT_BG_EN))
	{
		memset(bg, 0, sizeof bg);
		un16 white = rgb565(dmg_colors[dmg_palette][0][0]);
		for (int x = 0; x < 160; x++) line[x] = white;
	}
	else
	{
		int split = r->win ? (wx < 0 ? 0 : wx) : 160;
		if (split > 0)
			tiles(bg, split, (lcdc & LCDC_BIT_BG_MAP) ? 0x1C00 : 0x1800, r->scx, (r->scy + L) & 255);
		if (split < 160)
			tiles(bg + split, 160 - split, (lcdc & LCDC_BIT_WIN_MAP) ? 0x1C00 : 0x1800, split - wx, r->wl);
		for (int x = 0; x < 160; x++) line[x] = bgpal[bg[x] & 0x1F];
	}

	if (lcdc & LCDC_BIT_OBJ_EN)
	{
		int h = (lcdc & LCDC_BIT_OBJ_SIZE) ? 16 : 8, ns = 0;
		byte sel[10], taken[160];
		/* the first 10 sprites in OAM on this line */
		for (int i = 0; i < 40 && ns < 10; i++)
		{
			int y = lcd.oam.obj[i].y - 16;
			if (L >= y && L < y + h) sel[ns++] = (byte)i;
		}
		/* the DMG draws the one further left on top, then the first in OAM */
		if (!hw.cgb)
			for (int i = 1; i < ns; i++)
				for (int j = i; j > 0 && lcd.oam.obj[sel[j]].x < lcd.oam.obj[sel[j - 1]].x; j--)
				{
					byte t = sel[j];
					sel[j] = sel[j - 1];
					sel[j - 1] = t;
				}
		memset(taken, 0, sizeof taken);
		for (int k = 0; k < ns; k++)
		{
			const struct obj *o = &lcd.oam.obj[sel[k]];
			int f = o->flags, sx = o->x - 8, row = L - (o->y - 16), t = o->pat;
			if (f & 0x40) row = h - 1 - row;
			if (h == 16) t &= 0xFE;
			const byte *data = (hw.cgb && (f & 0x08)) ? lcd.vbank[1] : lcd.vbank[0];
			int lo = data[t * 16 + row * 2], hi = data[t * 16 + row * 2 + 1];
			int base = hw.cgb ? (f & 7) * 4 : (f & 0x10) ? 4 : 0;
			for (int i = 0; i < 8; i++)
			{
				int x = sx + i, b = (f & 0x20) ? i : 7 - i;
				if (x < 0 || x >= 160) continue;
				int c = ((hi >> b) & 1) << 1 | ((lo >> b) & 1);
				if (!c || taken[x]) continue;
				taken[x] = 1;
				int under = bg[x];
				if ((under & 3) && ((hw.cgb && (lcdc & LCDC_BIT_BG_EN) && (under & 0x80)) ||
				                    ((f & 0x80) && (!hw.cgb || (lcdc & LCDC_BIT_BG_EN)))))
					continue; /* behind the background */
				line[x] = objpal[base + c];
			}
		}
	}
	rg_line(L, line);
}

void lcd_refreshline()
{
	int L = R_LY, lcdc = R_LCDC;
	if (!(lcdc & LCDC_BIT_LCD_EN) || L >= 144) return;

	/* the window: once LY has met WY in a frame, it shows on every line it is on for, and
	   counts those lines itself */
	if (L == R_WY) lcd.wy_hit = 1;
	int win = (lcdc & LCDC_BIT_WIN_EN) && lcd.wy_hit && R_WX <= 166 && (hw.cgb || (lcdc & LCDC_BIT_BG_EN));
	struct line_regs r = {(byte)lcdc, R_SCX, R_SCY, R_WX, R_BGP, R_OBP0, R_OBP1, (byte)win, (byte)lcd.wline, 1};
	if (win) lcd.wline++;
	if (!gb_draw) return;
	if (L == 0) memset(snap, 0, sizeof snap);
	snap[L] = r;
	draw_line(L, &r);
}

/* the last frame drawn, again (lines the LCD didn't draw are white) */
void gb_redraw(void)
{
	for (int L = 0; L < 144; L++)
	{
		if (snap[L].drawn)
			draw_line(L, &snap[L]);
		else
		{
			un16 white = hw.cgb ? 0xFFFF : rgb565(dmg_colors[dmg_palette][0][0]);
			for (int x = 0; x < 160; x++) line[x] = white;
			rg_line(L, line);
		}
	}
}
