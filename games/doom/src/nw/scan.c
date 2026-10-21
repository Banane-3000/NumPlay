/* Doom on the NumWorks calculator: a build for measuring the frame rate in the emulator (make EXTRA="-DNW_WARP=11
 * -DNW_SCAN=1", tools/fps.py). The level it starts in is seen from all over: a frame from the middle of each
 * subsector looking 8 ways round, with god mode on, as tests/play.c --scan does on a computer. NW_SCAN=2 draws
 * in low detail. When every view has been drawn, nw_scan_done is set. */
#if NW_SCAN
#include "doomtype.h"
#include "doom/doomstat.h"
#include "doom/p_local.h"
#include "nw.h"

/* the view drawn (the emulator reads them) */
__attribute__((used)) volatile int nw_scan_ss = -1, nw_scan_dir;
__attribute__((used)) volatile uint32_t nw_scan_done;
static fixed_t scan_x, scan_y;

static int scan_middle(int i, fixed_t *x, fixed_t *y) {
    subsector_t *sub = &subsectors[i];
    seg_t *line = subsector_firstline(sub), *limit = subsector_linelimit(sub);
    int64_t sx = 0, sy = 0;
    int n = 0;
    for (; line < limit; line += seg_next_step(line), n++) {
        sx += vertex_x(seg_v1(line));
        sy += vertex_y(seg_v1(line));
    }
    if (!n) return 0;
    *x = (fixed_t)(sx / n);
    *y = (fixed_t)(sy / n);
    sector_t *s = subsector_sector(sub);
    return R_PointInSubsector(*x, *y) == sub && sector_ceilingheight(s) - sector_floorheight(s) >= 56 * FRACUNIT;
}

/* the next view */
static void scan_next(void) {
    if (nw_scan_ss >= 0 && ++nw_scan_dir < 8) return;
    nw_scan_dir = 0;
    do {
        if (++nw_scan_ss >= (int)numsubsectors) {
            nw_scan_done = 1;
            nw_scan_ss = 0;
        }
    } while (!scan_middle(nw_scan_ss, &scan_x, &scan_y));
}

/* each tic: the player where the view is */
void nw_scan_tic(void) {
    player_t *p = &players[consoleplayer];
    mobj_t *mo = p->mo;
    if (gamestate != GS_LEVEL || !mo) return;
    if (nw_scan_ss < 0) scan_next();
    p->cheats |= CF_GODMODE;
    P_UnsetThingPosition(mo);
    mo->xy.x = scan_x;
    mo->xy.y = scan_y;
    P_SetThingPosition(mo);
    mo->z = sector_floorheight(mobj_sector(mo));
    p->viewz = mo->z + VIEWHEIGHT;
    mobj_full(mo)->angle = (angle_t)nw_scan_dir * ANG45;
}

/* each frame drawn: the next view */
void nw_scan_frame(void) {
    if (gamestate == GS_LEVEL && nw_scan_ss >= 0) scan_next();
}
#endif
