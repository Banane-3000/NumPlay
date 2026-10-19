/* Doom on the calculator: the screen.
 *
 * Doom's 320 x 200 screen is made a band of NW_BAND_W columns at a time (pd_render.cpp), each one whole and
 * in 8 bits as the game draws it, then turned into the display's colours through the palette (with its red,
 * gold and green flashes) and sent. The calculator's screen is 320 x 240: every fifth row is drawn twice,
 * which gives back the taller pixels Doom had on the monitors of its day.
 *
 * The panel refreshes itself from its own memory, a column at a time across the screen (in landscape),
 * 40 times a second or so. A band sent while the refresh goes over it would show the top of the new frame
 * over the bottom of the old one, and a refresh that goes over bands of two frames would show both: the
 * frame tears. So the bands go in the order the refresh takes them, each one once the refresh has gone past
 * it and before the next refresh comes to it: every refresh shows one whole frame. The refresh's period and
 * phase are measured with eadk_display_wait_for_vblank (nw_sync_*). */
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
#include "nw_render.h"

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
/* the refresh (below) */
int nw_scan_right_to_left; /* (the options: the panel's refresh goes right to left, or with nw_sync_off, no
                              syncing at all) */
int nw_sync_off;
static uint32_t nw_sync_when; /* (when it was last measured, in ms) */
static int nw_band_index;
static void nw_sync_measure(void);

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
    nw_band_index = 0;
    /* (the refresh measured again now and then: the panel's clock is not the processor's) */
    if (!nw_sync_off && (!nw_sync_when || plat_millis() - nw_sync_when > 4000)) nw_sync_measure();
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
/* how long frames take to make (the emulator, tools/emu.py, reads perf_frames, perf_max and perf_slow; the
 * rest are for tests: the work of each frame in ms, by 1 ms, and the most frames in a row over a tic) */
uint32_t perf_frames, perf_max, perf_slow, perf_hist[64];
extern uint32_t nw_slept_ms;
void nw_frame_end(void) {
    static uint32_t last_ms, last_slept;
    uint32_t now = plat_millis();
    uint32_t work = (now - last_ms) - (nw_slept_ms - last_slept);
    if (perf_frames) {
        if (work > perf_max) perf_max = work;
        if (work > 28) perf_slow++;
        perf_hist[work < 63 ? work : 63]++;
    }
    perf_frames++;
    last_ms = now;
    last_slept = nw_slept_ms;
}
#endif

/* ------------------------------------------------------------------ the refresh
 * Times are in 1/256 ms. The refresh starts at nw_sync_t0 + k * nw_sync_period; its pass over band j (in its
 * order) is the j-th fifth of the period. A frame keeps to one refresh r: band j goes once refresh r is past
 * it (after r's start + (j + 1) fifths) and before refresh r + 1 comes to it (before r's start + 1 + j fifths),
 * with a margin each side for the clock (1 ms steps) and the panel's blanking time. */
#define NW_SYNC_MARGIN (2 * 256)
static int32_t nw_sync_period, nw_sync_t0;
static int32_t nw_band_ms, nw_push_ms; /* (how long a band takes to make and to send, on average) */
static int32_t nw_frame_r; /* the refresh this frame keeps to (its start) */
static uint32_t nw_band_start;

static int32_t nw_now(void) { return (int32_t)(plat_millis() * 256 + 128); }

/* the refresh's period and phase: from the time of a few refreshes (none, if there are no refreshes to see) */
static void nw_sync_measure(void) {
    nw_sync_when = plat_millis();
    if (!plat_vblank()) {
        nw_sync_period = 0;
        return;
    }
    uint32_t a = plat_millis();
    int n = nw_sync_period ? 1 : 8;
    for (int i = 0; i < n; i++) plat_vblank();
    uint32_t b = plat_millis();
    if (n == 8) nw_sync_period = (int32_t)((b - a) * 256 / 8);
    nw_sync_t0 = (int32_t)(b * 256 + 128);
    nw_sync_when = b;
    /* (a refresh faster than 100 times a second, or none: the panel does not tell) */
    if (nw_sync_period < 10 * 256) nw_sync_period = 0;
}

/* the start of the last refresh at or before t */
static int32_t nw_refresh_before(int32_t t) {
    int32_t d = t - nw_sync_t0;
    int32_t k = d >= 0 ? d / nw_sync_period : -((-d + nw_sync_period - 1) / nw_sync_period);
    return nw_sync_t0 + k * nw_sync_period;
}

/* band j's window under the refresh starting at r */
static int32_t nw_window_open(int32_t r, int j) { return r + (j + 1) * nw_sync_period / 5 + NW_SYNC_MARGIN; }
static int32_t nw_window_close(int32_t r, int j) {
    return r + nw_sync_period + j * nw_sync_period / 5 - NW_SYNC_MARGIN - nw_push_ms;
}

static void nw_wait_until(int32_t t) {
    while (nw_now() < t) {
    }
}

/* band j is made: when to send it */
static void nw_sync_band(int j) {
    int32_t now = nw_now(), fifth = nw_sync_period / 5;
    if (j == 0) {
        /* the first refresh whose window for band 0 is open now or later, or the one after it if the frame
         * would be late for the last band (the bands to come taking as long as they do on average) */
        int32_t r = nw_refresh_before(now) - nw_sync_period;
        while (nw_window_close(r, 0) < now) r += nw_sync_period;
        int32_t p = now > nw_window_open(r, 0) ? now : nw_window_open(r, 0);
        for (int k = 1; k < 5; k++) {
            p += nw_band_ms + nw_push_ms;
            if (p < nw_window_open(r, k)) p = nw_window_open(r, k);
        }
        if (p > nw_window_close(r, 4)) r += nw_sync_period;
        nw_frame_r = r;
    }
    int32_t open = nw_window_open(nw_frame_r, j);
    if (now < open) {
        nw_wait_until(open);
    } else if (now > nw_window_close(nw_frame_r, j)) {
        /* late: at least not while the refresh goes over the band */
        int32_t r = nw_refresh_before(now);
        int32_t busy0 = r + j * fifth - NW_SYNC_MARGIN - nw_push_ms, busy1 = r + (j + 1) * fifth + NW_SYNC_MARGIN;
        if (now > busy0 && now < busy1) nw_wait_until(busy1);
    }
}

void nw_band_begin(uint8_t *band, int x0, int x1) {
    nw_band = band;
    nw_band_start = plat_millis();
    (void)x0, (void)x1;
}

/* the band to the screen: Doom's rows 5k to 5k + 4 are the screen's 6k to 6k + 5 (the last one twice) */
#ifdef HOST
uint8_t host_doom_screen[SCREENWIDTH * SCREENHEIGHT]; /* (tests: the frame in Doom's own pixels) */
#endif

void nw_band_end(int x0, int x1) {
    int w = x1 - x0;
#ifdef HOST
    for (int y = 0; y < SCREENHEIGHT; y++)
        memcpy(host_doom_screen + y * SCREENWIDTH + x0, nw_band + y * NW_BAND_W, w);
#endif
    int j = nw_band_index++;
    int32_t made = (int32_t)(plat_millis() - nw_band_start) * 256;
    nw_band_ms += (made - nw_band_ms) / 4;
    if (nw_sync_period && !nw_sync_off) nw_sync_band(j);
    uint32_t t = plat_millis();
    uint16_t lines[NW_BAND_W * 24];
    for (int y = 0; y < SCREENHEIGHT; y += 20) {
        const uint8_t *src = nw_band + y * NW_BAND_W;
        uint16_t *d = lines;
        for (int k = 0; k < 4; k++) {
            for (int r = 0; r < 5; r++, src += NW_BAND_W, d += w)
                for (int i = 0; i < w; i++) d[i] = palette[src[i]];
            memcpy(d, d - w, w * sizeof *d);
            d += w;
        }
        plat_push(x0, y / 5 * 6, w, 24, lines);
    }
    int32_t pushed = (int32_t)(plat_millis() - t) * 256;
    nw_push_ms += (pushed - nw_push_ms) / 4;
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
