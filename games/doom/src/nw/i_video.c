/* Doom on the calculator: the screen.
 *
 * Doom's 320 x 200 screen is made a band of 40 rows at a time (pd_render.cpp), each one in 8 bits as the
 * game draws it, then turned into the display's colours through the palette (with its red, gold and green
 * flashes) and pushed. The calculator's screen is 320 x 240: every fifth row is drawn twice, which gives
 * back the taller pixels Doom had on the monitors of its day. */
#include <stdio.h>
#include <string.h>

#include "doomtype.h"
#include "i_system.h"
#include "i_video.h"
#include "tables.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "plat.h"
#include "nw.h"

/* what the engine expects to find */
boolean screenvisible = true;
boolean screensaver_mode = false;
isb_int8_t usegamma = 0;
pixel_t *I_VideoBuffer;
int usemouse = 0;
float mouse_acceleration = 2.0f;
int mouse_threshold = 10;
uint8_t next_video_type, next_frame_index, next_overlay_index;
semaphore_t render_frame_ready, display_frame_freed;

static uint16_t palette[256];
static int8_t next_pal = 0; /* the palette to use from the next frame, -1 when it is the one in use */
static int8_t last_gamma = -1;

void pd_init(void);
void I_InitGraphics(void) { pd_init(); }
void I_ShutdownGraphics(void) {}
void I_StartFrame(void) {}
void I_UpdateNoBlit(void) {}
void I_FinishUpdate(void) {}
void I_SetWindowTitle(const char *title) {}
void I_GraphicsCheckCommandLine(void) {}
void I_CheckIsScreensaver(void) {}
void I_DisplayFPSDots(boolean dots_on) {}
void I_BindVideoVariables(void) {}
int I_GetPaletteIndex(int r, int g, int b) { return 0; }
void I_SetPaletteNum(int doompalette) { next_pal = (int8_t)doompalette; }

static inline uint16_t rgb565(int r, int g, int b) { return (uint16_t)((r & 0xF8) << 8 | (g & 0xFC) << 3 | b >> 3); }

/* PLAYPAL keeps only the first palette: the others (pain, pickups, the radiation suit) are made from it as
 * Doom's own tool made them */
static void make_palette(int num) {
    static const uint8_t *playpal;
    static boolean calculated;
    if (!playpal) {
        lumpindex_t l = W_GetNumForName("PLAYPAL");
        playpal = W_CacheLumpNum(l, PU_STATIC);
        calculated = W_LumpLength(l) == 768;
    }
    const uint8_t *p = playpal + (calculated ? 0 : num * 768);
    int mul = 0, r0 = 0, g0 = 0, b0 = 0;
    if (calculated && num) {
        if (num < 9) mul = num * 65536 / 9, r0 = 255;
        else if (num < 13) mul = (num - 8) * 65536 / 8, r0 = 215, g0 = 186, b0 = 69;
        else mul = 65536 / 8, g0 = 256;
    }
    for (int i = 0; i < 256; i++, p += 3) {
        int r = p[0], g = p[1], b = p[2];
        if (mul) {
            r += ((r0 - r) * mul) >> 16;
            g += ((g0 - g) * mul) >> 16;
            b += ((b0 - b) * mul) >> 16;
        }
        if (usegamma) {
            r = gammatable[usegamma - 1][r];
            g = gammatable[usegamma - 1][g];
            b = gammatable[usegamma - 1][b];
        }
        palette[i] = rgb565(r, g, b);
    }
}

void nw_frame_begin(void) {
    if (next_pal >= 0 || last_gamma != usegamma) {
        static int8_t current;
        if (next_pal >= 0) current = next_pal;
        make_palette(current);
        next_pal = -1;
        last_gamma = usegamma;
    }
}
#ifdef HOST
void host_frame(void); /* tests/play.c: time goes by, screenshots */
void nw_frame_end(void) { host_frame(); }
#else
void nw_frame_end(void) {}
#endif

void nw_band_begin(uint8_t *band, int y0, int y1) {
    nw_band = band;
    nw_band_y0 = y0;
    nw_band_y1 = y1;
}

/* the band to the screen: Doom's rows 5k to 5k + 4 are the screen's 6k to 6k + 5 (the last one twice) */
void nw_band_end(void) {
    uint16_t lines[SCREENWIDTH * 6];
    for (int y = nw_band_y0; y < nw_band_y1; y += 5) {
        const uint8_t *src = nw_band + (y - nw_band_y0) * SCREENWIDTH;
        uint16_t *d = lines;
        for (int j = 0; j < 5; j++) {
            for (int i = 0; i < SCREENWIDTH; i++) d[i] = palette[src[i]];
            src += SCREENWIDTH;
            d += SCREENWIDTH;
        }
        memcpy(d, d - SCREENWIDTH, SCREENWIDTH * sizeof *d);
        plat_push(0, y / 5 * 6, SCREENWIDTH, 6, lines);
    }
}

/* ------------------------------------------------------------------ errors */
#ifdef HOST
void nw_show_error(const char *msg) { fprintf(stderr, "I_Error: %s\n", msg); }
#else
#include <eadk.h>
void nw_show_error(const char *msg) {
    plat_fill(0, 0, PLAT_W, PLAT_H, 0);
    char line[52];
    int y = 90;
    eadk_display_draw_string("Doom stopped:", (eadk_point_t){8, 70}, false, 0xF800, 0);
    for (const char *s = msg; *s && y < 200; y += 14) {
        int n = 0;
        while (s[n] && s[n] != '\n' && n < 51) n++;
        memcpy(line, s, n);
        line[n] = 0;
        eadk_display_draw_string(line, (eadk_point_t){8, (uint16_t)y}, false, 0xFFFF, 0);
        s += n;
        if (*s == '\n') s++;
    }
    eadk_display_draw_string("Press any key to quit", (eadk_point_t){8, 210}, false, 0x8410, 0);
    while (plat_keys()) plat_sleep(10);
    while (!plat_keys()) plat_sleep(10);
}
#endif
