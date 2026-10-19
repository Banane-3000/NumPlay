/* Doom on the calculator: the renderer, as the BSP pass (r_segs.c, r_things.c, r_plane.c) and the back end
 * that draws the screen (pd_render.cpp) share it.
 *
 * There is no room for a frame buffer. The BSP pass records the frame as vanilla would draw it: every column
 * of wall and sky, every column of floor and ceiling (with vanilla's visplanes, split as R_CheckPlane does),
 * the drawsegs that clip sprites with their silhouettes, and the vissprites. The screen is then drawn a
 * vertical band of NW_BAND_W columns at a time, in the order the panel refreshes them, each band whole before
 * it is sent: its floors and ceilings as vanilla's spans, its walls, then sprites, masked walls and the
 * weapon in vanilla's order with vanilla's clipping (R_DrawMasked). A frame too busy for the records is drawn
 * again for each band, the view rendered for that band's columns only. */
#ifndef NW_RENDER_H
#define NW_RENDER_H
#include "doomtype.h"
#include "m_fixed.h"
#ifdef __cplusplus
extern "C" {
#endif

#define NW_BAND_W 64

/* the columns this pass records (the whole view, or one band when a frame is drawn band by band: the BSP
 * still goes over the whole view, so that everything is as vanilla has it, but only the band is kept) */
extern int nw_strip_x0, nw_strip_x1;
/* the band being drawn */
extern int nw_band_x0, nw_band_x1;

/* the frame's table of wall texture heights (dc_texturemid), shared by the columns of a wall's tier */
int nw_add_tmid(fixed_t texturemid);
extern int nw_tmid_cur;

/* walls and sky (dc_x, dc_yl, dc_yh, dc_iscale, dc_source, dc_colormap_index, nw_tmid_cur), and floors and
 * ceilings: recorded by the BSP pass */
void nw_record_wall(void);
void nw_record_plane(int x, int yl, int yh, int plane);
/* the pass ran out of room for something: the frame is drawn band by band */
void nw_record_overflow(void);

/* drawsegs that matter to R_DrawMasked: with a silhouette or a masked middle texture */
typedef struct {
    const void *curline;
    int16_t x1, x2;
    fixed_t scale1, scale2, scalestep;
    fixed_t bsilheight, tsilheight;
    int16_t topclip, botclip; /* where column x1 would be in nw_openings, or one of these: */
    int16_t masked; /* the masked texture's nw_masked_t in nw_openings, or NW_OPEN_NONE */
    uint8_t silhouette;
} nw_drawseg_t;
#define NW_OPEN_NONE (-32768)
#define NW_OPEN_MIN (-32767) /* every column -1: minfloorceilingcliparray */
#define NW_OPEN_MAX (-32766) /* every column viewheight: maxfloorceilingcliparray */
#define NW_DRAWSEG_MAX 100 /* (E1M1 to E1M9 have up to 86) */
#define NW_OPENINGS (3328) /* (up to 2878 bytes) */
extern nw_drawseg_t nw_drawsegs[NW_DRAWSEG_MAX];
extern int nw_drawseg_count;
extern uint8_t nw_openings[NW_OPENINGS];
/* a masked seg's texture: before its columns (x0 on) in nw_openings */
typedef struct {
    fixed_t texturemid;
    int16_t texture, x0;
    int8_t lightnum;
    uint8_t pad[3];
} nw_masked_t;

/* band time (r_things.c): sprites, masked walls and the weapon of the band, in vanilla's order */
void nw_sort_vissprites(void);
void nw_draw_masked(void);
/* ... their columns go to the back end, a sprite or a masked wall at a time */
void nw_masked_column(int x, int yl, int yh, int base_offset, int fuzz);
void nw_masked_flush(void);
extern fixed_t nw_masked_texturemid;

/* fuzz (the shimmer of invisible things): vanilla's fuzzpos where each fuzzy sprite and the weapon start,
 * from the frame's sprites counted in vanilla's order before the bands; nw_fuzz_at goes on from there */
extern int nw_fuzz_at;
int nw_fuzz_prepass(int fuzzpos);

/* a pass over the BSP starts: nothing recorded yet */
void nw_clear_vissprites(void);
void nw_clear_drawsegs(void);
/* dc_source, a texture's column: its single patch column if it has one (0: nothing to draw) */
int nw_resolve_column(void);

#ifdef __cplusplus
}
#endif
#endif
