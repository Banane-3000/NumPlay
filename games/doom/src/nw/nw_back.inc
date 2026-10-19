// Doom on the calculator: the renderer's back end (included by pd_render.cpp; nw/nw_render.h tells the
// story). The screen is drawn a band of NW_BAND_W columns at a time, each band whole: floors and ceilings,
// walls, then sprites, masked walls and the weapon in vanilla's order, then the status bar, messages and
// menus, and sent to the display in the order its panel refreshes them (nw/i_video.c).

extern "C" {
#if HOST
uint32_t nw_peak[4]; // (tests: the most plane records, drawsegs, bytes of openings and vissprites a frame had)
uint32_t nw_last_walls, nw_last_planes; // (and the records of the last frame)
#endif
int nw_strip_x0 = 0, nw_strip_x1 = SCREENWIDTH;
int nw_band_x0 = 0, nw_band_x1 = SCREENWIDTH;
int nw_tmid_cur;
fixed_t nw_masked_texturemid;
int nw_fuzz_at;
uint32_t nw_strips;
void nw_clear_drawsegs(void);
void nw_clear_vissprites(void);
int nw_fuzz_prepass(int fuzzpos);
// nw/i_video.c: the band to the display, and which way the panel refreshes
void nw_band_end(int x0, int x1);
extern int nw_scan_right_to_left;
}

// ---------------------------------------------------------------------------------------------------------------
// what the BSP pass records

extern "C" int nw_add_tmid(fixed_t texturemid) {
    (void) texturemid; // (the columns keep their own: pd_column has room)
    return 0;
}

extern "C" void nw_record_overflow(void) {
    if (!nw_overflowed) nw_col_overflows++;
    nw_overflowed = true;
}

// floors and ceilings: a column of a visplane (from the end of list_buffer down)
struct nw_plane_rec {
    uint32_t x : 9;
    uint32_t yl : 8;
    uint32_t yh : 8;
    uint32_t plane : 7;
};
#define NW_PLANE(k) (((nw_plane_rec *) (list_buffer + NW_POOL))[-1 - (k)])
static int nw_plane_count;
// (the most records since the flats' places were last worked out: what is above them is overwritten)
static int nw_wall_hwm, nw_plane_hwm;

// room for walls and planes, the masked columns of a band and a flat
static inline bool nw_room(int walls, int planes) {
    return (walls + NW_SCRATCH) * (int) sizeof(pd_column) + planes * (int) sizeof(nw_plane_rec) + 4096 <= NW_POOL;
}

extern "C" void nw_record_plane(int x, int yl, int yh, int plane) {
    if (yh < yl || x < nw_strip_x0 || x >= nw_strip_x1) return;
    if (!nw_room(render_col_count, nw_plane_count + 1)) {
        nw_record_overflow();
        return;
    }
    nw_plane_rec &r = NW_PLANE(nw_plane_count++);
    r.x = (uint32_t) x;
    r.yl = (uint32_t) yl;
    r.yh = (uint32_t) yh;
    r.plane = (uint32_t) plane;
}

// a record of dc_source's column at x (the fraction of texturemid below a quarter of a unit is the same for all
// the walls, the view's own, and for all the posts of a sprite: *low)
static int32_t nw_wall_low, nw_masked_low;
#if HOST
extern "C" { uint32_t nw_low_mismatch; } // (tests: a texturemid with another fraction: never)
#endif
static void nw_fill(pd_column &c, int x, int yl, int yh, fixed_t texturemid, int32_t *low) {
    c.yl = (uint8_t) yl;
    c.yh = (uint8_t) yh;
    c.scale = (uint32_t) dc_iscale > DDA_MAX ? DDA_MAX : (uint32_t) dc_iscale;
    c.colormap_index = (uint32_t) dc_colormap_index;
    int32_t f = texturemid & 0x3fff;
    if (f && *low < 0) *low = f;
#if HOST
    if (f && f != *low) nw_low_mismatch++;
#endif
    c.lowsel = f != 0;
    c.tm = (texturemid - f) >> 14;
    c.col = (uint8_t) dc_source.col;
    c.fd_num = (uint8_t) dc_source.fd_num;
    c.x_lo = (uint32_t) x & 0xff;
    c.x_hi = (uint32_t) x >> 8;
}

// walls and sky: one column, as vanilla's R_DrawColumn would draw it
#if HOST
// (tests: DUMPCOL="x tic" prints the columns at x in that tic's frame, as the reference does)
static int nw_dump_x = -2, nw_dump_tic;
static bool nw_dumping(int x) {
    if (nw_dump_x == -2) {
        const char *e = getenv("DUMPCOL");
        nw_dump_x = e ? atoi(e) : -1;
        nw_dump_tic = e && strchr(e, ' ') ? atoi(strchr(e, ' ')) : -1;
    }
    return x == nw_dump_x && leveltime == nw_dump_tic && gamestate == GS_LEVEL;
}
#endif

extern "C" void nw_record_wall(void) {
#if HOST
    if (nw_dumping(dc_x)) printf("COL x=%d yl=%d yh=%d iscale=%d texturemid=%d id=%d col=%d E%dM%d\n", dc_x, dc_yl, dc_yh, dc_iscale, dc_texturemid, dc_source.real_id, dc_source.col, gameepisode, gamemap);
#endif
    if (dc_yh < dc_yl || dc_x < nw_strip_x0 || dc_x >= nw_strip_x1) return;
    if (render_col_count == RENDER_COL_MAX || !nw_room(render_col_count + 1, nw_plane_count)) {
        nw_record_overflow();
        return;
    }
    if (render_col_count >= (int) nw_col_peak) nw_col_peak = render_col_count + 1;
    nw_fill(render_cols[render_col_count++], dc_x, dc_yl, dc_yh, dc_texturemid, &nw_wall_low);
}

// a pass over the BSP starts: nothing recorded yet (the drawsegs and vissprites are cleared with vanilla's)
static void nw_pass_start() {
    if (render_col_count > nw_wall_hwm) nw_wall_hwm = render_col_count;
    if (nw_plane_count > nw_plane_hwm) nw_plane_hwm = nw_plane_count;
    render_col_count = 0;
    nw_plane_count = 0;
    nw_overflowed = false;
    nw_wall_low = -1;
}

// ---------------------------------------------------------------------------------------------------------------
// floors and ceilings in the band: the visplane of each pixel marked, then runs of the same visplane along
// each row, which are vanilla's spans; a span coming from the band on the left starts where it started there
// (its texture position worked out at its start, as R_MapPlane does)

#define nw_vbit (nw_shared.band.vbit)
static int8_t nw_carry_plane[SCREENHEIGHT];
static int16_t nw_carry_start[SCREENHEIGHT];
// (right to left: the carries at the left edge of each band but the first, worked out first, from the left)
#define nw_edge_plane (nw_shared.band.edge_plane)
#define nw_edge_start (nw_shared.band.edge_start)

static void nw_mark_planes(int x0, int x1) {
    memset(nw_vbit, 0, sizeof nw_vbit);
    for (int i = 0; i < nw_plane_count; i++) {
        const nw_plane_rec &r = NW_PLANE(i);
        int x = r.x;
        if (x < x0 || x >= x1) continue;
        uint8_t *p = render_frame_buffer + r.yl * NW_STRIDE + x;
        uint8_t *vb = nw_vbit + r.yl * (NW_BAND_W / 8) + ((x - x0) >> 3);
        uint8_t bit = (uint8_t) (1u << ((x - x0) & 7));
        for (int y = r.yl; y <= (int) r.yh; y++) {
            *p = (uint8_t) r.plane;
            p += NW_STRIDE;
            *vb |= bit;
            vb += NW_BAND_W / 8;
        }
    }
}

// the runs of the band's rows: to flat_runs (drawn when full, and at the end) if draw, and the carries
static void nw_plane_runs(int x0, int x1, bool draw) {
    int numvisplanes = lastvisplane - visplanes;
    if (draw) {
        memset(visplane_heads, -1, numvisplanes * sizeof(visplane_heads[0]));
        nw_flat_run_count = 0;
    }
    for (int y = 0; y < viewheight; y++) {
        const uint8_t *vb = nw_vbit + y * (NW_BAND_W / 8);
        const uint8_t *row = render_frame_buffer + y * NW_STRIDE;
        int carry_plane = nw_carry_plane[y];
        int carry_start = nw_carry_start[y];
        nw_carry_plane[y] = -1;
        int x = x0;
        while (x < x1) {
            int b = x - x0;
            if (!(vb[b >> 3] & (1u << (b & 7)))) {
                if (!vb[b >> 3] && !(b & 7)) x += 8;
                else x++;
                continue;
            }
            int plane = row[x];
            int s = x;
            do {
                x++;
                b = x - x0;
            } while (x < x1 && (vb[b >> 3] & (1u << (b & 7))) && row[x] == plane);
            int start = s == x0 && carry_plane == plane ? carry_start : s;
            if (x == x1) {
                nw_carry_plane[y] = (int8_t) plane;
                nw_carry_start[y] = (int16_t) start;
            }
            if (!draw) continue;
            if (nw_flat_run_count == NW_FLAT_RUNS) nw_flush_planes(numvisplanes);
            int16_t fr_pos = nw_flat_run_count++;
            flat_run &fr = flat_runs[fr_pos];
            fr.x_start = (int16_t) s;
            fr.x_true = (int16_t) start;
            fr.x_end = (int16_t) x;
            fr.y = (uint8_t) y;
            fr.next = visplane_heads[plane];
            visplane_heads[plane] = fr_pos;
        }
    }
    if (draw && nw_flat_run_count) nw_flush_planes(numvisplanes);
}

// the view's columns in the screen's columns x0 to x1 (low detail: one for each two)
static void nw_view_cols(int x0, int x1, int *v0, int *v1) {
    int a = (x0 - viewwindowx) >> detailshift, b = (x1 - viewwindowx) >> detailshift;
    *v0 = a < 0 ? 0 : a > viewwidth ? viewwidth : a;
    *v1 = b < 0 ? 0 : b > viewwidth ? viewwidth : b;
}

// where the band's view pixels go: in place, or in low detail at the band's left, to be doubled
static uint8_t *nw_view_buffer(uint8_t *band, int x0, int v0) {
    return band + viewwindowy * NW_STRIDE + (detailshift ? -v0 : viewwindowx - x0);
}

// before drawing right to left: each band's carries from its left neighbour, worked out left to right
static void nw_plane_edges(uint8_t *band) {
    memset(nw_carry_plane, -1, sizeof nw_carry_plane);
    for (int k = 0; k < SCREENWIDTH / NW_BAND_W - 1; k++) {
        int v0, v1;
        nw_view_cols(k * NW_BAND_W, (k + 1) * NW_BAND_W, &v0, &v1);
        if (v0 < v1) {
            render_frame_buffer = nw_view_buffer(band, k * NW_BAND_W, v0);
            nw_mark_planes(v0, v1);
            nw_plane_runs(v0, v1, false);
        }
        memcpy(nw_edge_plane[k], nw_carry_plane, sizeof nw_carry_plane);
        memcpy(nw_edge_start[k], nw_carry_start, sizeof nw_carry_start);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// walls in the band: by framedrawable, each texture or patch decoded once

static void nw_draw_fd(int fd_num, int head) {
    int id = framedrawables[fd_num].real_id;
    if (id > 0) {
        draw_composite_columns(id, head);
    } else if (id < 0) {
        int16_t heads[WHD_PATCH_MAX_WIDTH];
        uint8_t heights[WHD_PATCH_MAX_WIDTH];
        draw_patch_columns(-id, head, heads, heights, 0);
    }
}

// the columns in nw_bidx, by framedrawable
static void nw_draw_batch(int n) {
    memset(fd_heads, -1, sizeof(fd_heads));
    for (int j = n - 1; j >= 0; j--) {
        const pd_column &c = COL_REC(j);
        nw_blink[j] = fd_heads[c.fd_num];
        fd_heads[c.fd_num] = (int16_t) j;
    }
    for (int fd_num = 0; fd_num < num_framedrawables; fd_num++) {
        if (fd_heads[fd_num] != -1) nw_draw_fd(fd_num, fd_heads[fd_num]);
    }
}

static void nw_band_walls(int x0, int x1) {
    nw_tm_low = nw_wall_low < 0 ? 0 : nw_wall_low;
    int n = 0;
    for (int i = 0; i < render_col_count; i++) {
        int x = COL_X(render_cols[i]);
        if (x < x0 || x >= x1) continue;
        nw_bidx[n++] = (int16_t) i;
        if (n == NW_BATCH) {
            nw_draw_batch(n);
            n = 0;
        }
    }
    if (n) nw_draw_batch(n);
}

// ---------------------------------------------------------------------------------------------------------------
// sprites, masked walls and the weapon (r_things.c works out which columns, in vanilla's order): the columns
// of one at a time gathered after the walls' records, then drawn

static int nw_scratch_base, nw_scratch_count;
// vanilla's fuzzpos, from frame to frame
static int8_t fuzzpos;

extern "C" void nw_masked_column(int x, int yl, int yh, int base_offset, int fuzz) {
    if (fuzz) {
        // vanilla's R_DrawFuzzColumn: its rows, and fuzzpos going on in vanilla's order (counted, not drawn,
        // outside the band)
        if (!yl) yl = 1;
        if (yh == viewheight - 1) yh = viewheight - 2;
        if (yh < yl) return;
        if (x >= nw_band_x0 && x < nw_band_x1) {
            const lighttable_t *darken_map = xcolormaps + 256 * 6;
            uint8_t *p = render_frame_buffer + yl * NW_STRIDE + x;
            int pos = nw_fuzz_at;
            for (int y = yl; y <= yh; y++, p += NW_STRIDE) {
                *p = darken_map[p[fuzzoffset[pos] > 0 ? NW_STRIDE : -NW_STRIDE]];
                if (++pos == FUZZTABLE) pos = 0;
            }
        }
        nw_fuzz_at = (nw_fuzz_at + yh - yl + 1) % FUZZTABLE;
        return;
    }
    if (x < nw_band_x0 || x >= nw_band_x1) return;
#if HOST
    if (nw_dumping(x)) printf("COL x=%d yl=%d yh=%d iscale=%d texturemid=%d (masked)\n", x, yl, yh, dc_iscale,
                              nw_masked_texturemid - (base_offset << FRACBITS));
#endif
    if (nw_scratch_count == NW_SCRATCH) nw_masked_flush();
    if (!nw_scratch_count) nw_masked_low = -1;
    nw_fill(render_cols[nw_scratch_base + nw_scratch_count++], x, yl, yh,
            nw_masked_texturemid - (base_offset << FRACBITS), &nw_masked_low);
}

extern "C" void nw_masked_flush(void) {
    if (!nw_scratch_count) return;
    for (int j = 0; j < nw_scratch_count; j++) nw_bidx[j] = (int16_t) (nw_scratch_base + j);
    nw_tm_low = nw_masked_low < 0 ? 0 : nw_masked_low;
    int n = nw_scratch_count;
    nw_scratch_count = 0;
    nw_draw_batch(n);
}

// ---------------------------------------------------------------------------------------------------------------
// screens without the 3D view: their pictures for the band's columns

// a 320 x 200 picture's columns x0 to x1 (each decoded top to bottom, once)
static void nw_draw_splash(int patch_num, int x0, int x1) {
    patch_decode_info pdi;
    get_patch_decoder(patch_num, &pdi);
    const uint16_t *col_offsets = pdi.col_offsets;
    const uint8_t *patch_decoder_table = get_patch_decoder_table(patch_num, pdi.decoder);
    int h = patch_height(pdi.patch);
    for (int col = x0; col < x1; col++) {
        uint16_t col_offset = col_offsets[col];
        if (0xff == (col_offset >> 8)) col_offset = col_offsets[col_offset & 0xff];
        th_bit_input bi;
        if (patch_byte_addressed(pdi.patch)) {
            th_bit_input_init(&bi, pdi.patch + pdi.data_index + col_offset);
        } else {
            th_bit_input_init_bit_offset(&bi, pdi.patch + pdi.data_index, col_offset);
        }
        uint8_t *dest = render_frame_buffer + col;
        uint8_t prev_pixel = 0;
        for (int y = 0; y < h && y < SCREENHEIGHT; y++, dest += NW_STRIDE) {
            uint16_t p = th_decode_table_special_16(pdi.decoder, patch_decoder_table, &bi);
            prev_pixel = p < 256 ? (uint8_t) p : (uint8_t) (prev_pixel + (p & 0xff) - 3);
            *dest = prev_pixel;
        }
    }
}

// a flat's pixels, decoded if they are not kept (flat[x * 64 + y])
static const uint8_t *nw_flat(const char *name) {
    int flatnum = W_GetNumForName(name) - firstflat;
    for (int slot = 0; slot < cached_flat_slots; slot++) {
        if (FLAT_SLOT_PIC(slot) == flatnum) return FLAT_SLOT_ADDR(slot);
    }
    return decode_flat_to_slot(0, flatnum);
}

// a flat tiled over the band (behind the ending's text, and darker behind the keys page)
static void nw_tile_flat(const char *name, int x0, int x1, int light = 0) {
    const uint8_t *flat_data = nw_flat(name);
    const lighttable_t *map = light ? xcolormaps + 256 * light : nullptr;
    for (int y = 0; y < SCREENHEIGHT; y++) {
        uint8_t *dest = render_frame_buffer + y * NW_STRIDE;
        const uint8_t *src = flat_data + (y & 63);
        for (int x = x0; x < x1; x++) dest[x] = map ? map[src[(x & 63) * 64]] : src[(x & 63) * 64];
    }
}

static void nw_draw_background(int x0, int x1, int help) {
    int patch_num = 0;
    if (help) {
        switch (help & 127) {
            case 1: patch_num = W_GetNumForName("HELP1"); break;
            case 2: patch_num = W_GetNumForName("HELP2"); break;
            case 3: patch_num = W_GetNumForName("HELP"); break;
            case 4: nw_tile_flat("FLOOR4_8", x0, x1, 22); return;
        }
    } else {
        switch (gamestate) {
            case GS_INTERMISSION: patch_num = wi_background_patch_num; break;
            case GS_DEMOSCREEN: patch_num = W_GetNumForName(pagename); break;
            case GS_FINALE:
                if (finalestage == F_STAGE_TEXT) {
                    nw_tile_flat(finaleflat, x0, x1);
                    return;
                } else if (finalestage == F_STAGE_ARTSCREEN) {
                    const char *ln = F_ArtScreenLumpName();
                    if (ln) patch_num = W_GetNumForName(ln);
                }
                break;
            default: break;
        }
    }
    if (patch_num) {
        nw_draw_splash(patch_num, x0, x1);
    } else {
        for (int y = 0; y < SCREENHEIGHT; y++) memset(render_frame_buffer + y * NW_STRIDE + x0, 0, x1 - x0);
    }
}

#if HOST
// (tests: FUZZRESET=1 starts each demo with vanilla's fuzz where a new game has it, as the reference, which
// plays one demo at a time, does)
extern "C" void host_demo_start(void) {
    if (getenv("FUZZRESET")) fuzzpos = 0;
}
#endif

// ---------------------------------------------------------------------------------------------------------------
// the frame

// the status bar, messages, the menu... over the picture, gathered once a frame
static vpatchlist_t nw_patches[400];

// flats are kept in what the records leave of list_buffer: the blocks written over since the last time
// forgotten, the free ones this frame's slots
static void nw_flats_ready() {
    int lo = ((render_col_count > nw_wall_hwm ? render_col_count : nw_wall_hwm) + NW_SCRATCH) * (int) sizeof(pd_column);
    int hi = NW_POOL - (nw_plane_count > nw_plane_hwm ? nw_plane_count : nw_plane_hwm) * (int) sizeof(nw_plane_rec);
    nw_wall_hwm = render_col_count;
    nw_plane_hwm = nw_plane_count;
    const int blocks = NW_POOL / 4096;
    int slots = 0;
    for (int b = blocks - 1; b >= 0; b--) {
        if (b * 4096 < lo || (b + 1) * 4096 > hi) {
            cached_flat_picnum[b] = 0xff;
        } else {
            nw_slot_addr[slots] = list_buffer + b * 4096;
            nw_slot_block[slots++] = (uint8_t) b;
        }
    }
    cached_flat_picnum[blocks] = 0xff;
    if (!slots) {
        // (no whole block: the 4 KB under the floors and ceilings, for this frame only)
        nw_slot_addr[0] = list_buffer + ((hi - 4096) & ~3);
        nw_slot_block[0] = (uint8_t) blocks;
        slots = 1;
    }
    cached_flat_slots = (uint8_t) slots;
}

// the 3D view's columns x0 to x1
static void nw_draw_view(int x0, int x1) {
    nw_mark_planes(x0, x1);
    nw_plane_runs(x0, x1, true);
    nw_band_walls(x0, x1);
    nw_draw_masked();
}

// low detail: the view's columns v0 to v1, drawn at the band's left, each made two of the screen's (as
// R_DrawColumnLow and R_DrawSpanLow draw them), from the right so that none is written over before it is read
static void nw_double_columns(uint8_t *band, int x0, int v0, int v1) {
    for (int y = 0; y < viewheight; y++) {
        uint8_t *row = band + (viewwindowy + y) * NW_STRIDE;
        for (int v = v1 - 1; v >= v0; v--) {
            uint8_t *d = row + viewwindowx + 2 * v - x0;
            d[0] = d[1] = row[v - v0];
        }
    }
}

// the smaller screen sizes: vanilla's R_FillBackScreen, a flat around the view (the bevel's patches are drawn
// with the others, pd_end_frame)
static void nw_draw_border(int x0, int x1) {
    const uint8_t *flat_data = nw_flat(gamemode == commercial ? "GRNROCK" : "FLOOR7_2");
    int l = viewwindowx, r = viewwindowx + scaledviewwidth;
    for (int y = 0; y < SCREENHEIGHT - ST_HEIGHT; y++) {
        uint8_t *dest = render_frame_buffer + y * NW_STRIDE;
        const uint8_t *src = flat_data + (y & 63);
        bool beside = y >= viewwindowy && y < viewwindowy + viewheight;
        for (int x = x0; x < x1; x++) {
            if (beside && x >= l && x < r) x = r;
            if (x >= x1) break;
            dest[x] = src[(x & 63) * 64];
        }
    }
}

static void nw_draw_bezel() {
    if (scaledviewwidth == SCREENWIDTH) return;
    int n = scaledviewwidth / 8 - 1;
    V_DrawPatchN(viewwindowx, viewwindowy - 8, VPATCH_HANDLE(VPATCH_NAME(BRDR_T)), n);
    V_DrawPatchN(viewwindowx, viewwindowy + viewheight, VPATCH_HANDLE(VPATCH_NAME(BRDR_B)), n);
    for (int y = 0; y < viewheight; y += 8) {
        V_DrawPatch(viewwindowx - 8, viewwindowy + y, VPATCH_HANDLE(VPATCH_NAME(BRDR_L)));
        V_DrawPatch(viewwindowx + scaledviewwidth, viewwindowy + y, VPATCH_HANDLE(VPATCH_NAME(BRDR_R)));
    }
    V_DrawPatch(viewwindowx - 8, viewwindowy - 8, VPATCH_HANDLE(VPATCH_NAME(BRDR_TL)));
    V_DrawPatch(viewwindowx + scaledviewwidth, viewwindowy - 8, VPATCH_HANDLE(VPATCH_NAME(BRDR_TR)));
    V_DrawPatch(viewwindowx - 8, viewwindowy + viewheight, VPATCH_HANDLE(VPATCH_NAME(BRDR_BL)));
    V_DrawPatch(viewwindowx + scaledviewwidth, viewwindowy + viewheight, VPATCH_HANDLE(VPATCH_NAME(BRDR_BR)));
}

static void nw_draw_band(uint8_t *band, int x0, int x1, bool view, int help, bool strips) {
    nw_band_x0 = x0;
    nw_band_x1 = x1;
    render_frame_buffer = band - x0;
    I_VideoBuffer = render_frame_buffer;
    nw_band_begin(band, x0, x1);
    if (view) {
        if (automapactive && !am_overlay) {
            nw_direct = true;
            AM_Drawer();
            nw_direct = false;
        } else {
            // (the view's own columns: nw_band_x0 and nw_band_x1 are those while it is drawn)
            int v0, v1;
            nw_view_cols(x0, x1, &v0, &v1);
            if (v0 < v1) {
                uint8_t *vbuf = nw_view_buffer(band, x0, v0);
                nw_band_x0 = v0;
                nw_band_x1 = v1;
                render_frame_buffer = vbuf;
                if (strips) {
                    // this band's view: the BSP pass again, its columns alone kept
                    pd_begin_frame();
                    nw_strip_x0 = v0;
                    nw_strip_x1 = v1;
                    R_RenderPlayerView(&players[displayplayer]);
                    render_frame_buffer = vbuf;
                    nw_flats_ready();
                    if (nw_overflowed) {
                        // (not even the band's pieces all found room: what has none is left black)
                        for (int y = 0; y < viewheight; y++) memset(vbuf + y * NW_STRIDE + v0, 0, v1 - v0);
                    }
                    // (the fuzz of the sprites, the same as in the first pass, is where that pass's count left it)
                    nw_strips++;
                }
                nw_scratch_base = render_col_count;
                nw_draw_view(v0, v1);
                nw_strip_x0 = 0;
                nw_strip_x1 = SCREENWIDTH;
                if (detailshift) nw_double_columns(band, x0, v0, v1);
                nw_band_x0 = x0;
                nw_band_x1 = x1;
                render_frame_buffer = band - x0;
            }
            if (scaledviewwidth != SCREENWIDTH) nw_draw_border(x0, x1);
            if (automapactive) {
                // (the map over the view)
                nw_direct = true;
                AM_Drawer();
                nw_direct = false;
            }
        }
        int below = scaledviewwidth == SCREENWIDTH ? viewheight : SCREENHEIGHT - ST_HEIGHT;
        for (int y = below; y < SCREENHEIGHT; y++) memset(render_frame_buffer + y * NW_STRIDE + x0, 0, x1 - x0);
    } else {
        nw_draw_background(x0, x1, help);
        if (gamestate == GS_FINALE && finalestage == F_STAGE_TEXT && !help) {
            nw_direct = true;
            F_TextWrite();
            nw_direct = false;
        } else if ((help & 127) == 4) {
            nw_direct = true;
            M_DrawKeys();
            nw_direct = false;
        }
    }
    V_DrawPatchListBand(nw_patches);
    nw_band_end(x0, x1);
}

// the frame's bands, in the order the panel refreshes them
static bool nw_frame_view, nw_frame_strips;
static int nw_frame_help;

static void nw_draw_frame() {
    bool view = nw_frame_view, strips = nw_frame_strips;
    int help = nw_frame_help;
    nw_frame_begin();
    uint8_t __aligned(8) band[NW_BAND_W * SCREENHEIGHT];
    const int bands = SCREENWIDTH / NW_BAND_W;
    if (nw_scan_right_to_left) {
        // (a frame drawn band by band has its visplanes in each band's pass: the spans' starts in the bands
        // on the left are not known, and are taken where the band starts)
        bool edges = view && (!automapactive || am_overlay) && !strips;
        if (edges) nw_plane_edges(band);
        for (int k = bands - 1; k >= 0; k--) {
            if (edges && k) {
                memcpy(nw_carry_plane, nw_edge_plane[k - 1], sizeof nw_carry_plane);
                memcpy(nw_carry_start, nw_edge_start[k - 1], sizeof nw_carry_start);
            } else {
                memset(nw_carry_plane, -1, sizeof nw_carry_plane);
            }
            nw_draw_band(band, k * NW_BAND_W, (k + 1) * NW_BAND_W, view, help, strips);
        }
    } else {
        memset(nw_carry_plane, -1, sizeof nw_carry_plane);
        for (int k = 0; k < bands; k++) nw_draw_band(band, k * NW_BAND_W, (k + 1) * NW_BAND_W, view, help, strips);
    }
    nw_band_x0 = 0;
    nw_band_x1 = SCREENWIDTH;
    nw_frame_end();
}

// ---------------------------------------------------------------------------------------------------------------
// vanilla's screen melt (f_wipe.c) from the screen shown to the new one: the panel keeps the old screen, each
// of its columns (two pixels wide) slides down, and the new screen's rows show above it; the new screen is the
// frame drawn when it started, drawn again from its records each time (nw/i_video.c moves the columns)

extern "C" {
int16_t nw_melt_from[SCREENWIDTH / 2], nw_melt_to[SCREENWIDTH / 2];
int nw_melting;
int M_Random(void);
}
static int16_t nw_melt_y[SCREENWIDTH / 2];
static bool nw_melt_going;

// a tic or more of it; true when it is done
extern "C" int nw_melt(int tics) {
    const int height = SCREENHEIGHT;
    if (!nw_melt_going) {
        // wipe_initMelt: the columns' starting places (for all 320 of them, though half are used)
        nw_melt_going = true;
        int y = -(M_Random() % 16);
        nw_melt_y[0] = (int16_t) y;
        for (int i = 1; i < SCREENWIDTH; i++) {
            int r = (M_Random() % 3) - 1;
            y += r;
            if (y > 0) y = 0;
            else if (y == -16) y = -15;
            if (i < SCREENWIDTH / 2) nw_melt_y[i] = (int16_t) y;
        }
    }
    // wipe_doMelt
    bool done = true;
    for (int i = 0; i < SCREENWIDTH / 2; i++) nw_melt_from[i] = nw_melt_y[i] > 0 ? nw_melt_y[i] : 0;
    while (tics--) {
        for (int i = 0; i < SCREENWIDTH / 2; i++) {
            if (nw_melt_y[i] < 0) {
                nw_melt_y[i]++;
                done = false;
            } else if (nw_melt_y[i] < height) {
                int dy = nw_melt_y[i] < 16 ? nw_melt_y[i] + 1 : 8;
                if (nw_melt_y[i] + dy >= height) dy = height - nw_melt_y[i];
                nw_melt_y[i] = (int16_t) (nw_melt_y[i] + dy);
                done = false;
            }
        }
    }
    for (int i = 0; i < SCREENWIDTH / 2; i++) nw_melt_to[i] = nw_melt_y[i] > 0 ? nw_melt_y[i] : 0;
    nw_melting = 1;
    nw_draw_frame();
    nw_melting = 0;
    if (done) nw_melt_going = false;
    return done;
}

void pd_end_frame(int wipe_start) {
    int help = inhelpscreens;
    bool view = gamestate == GS_LEVEL && !help;
#if HOST
    // (tests: NWSTRIPS=1 draws every frame band by band, NWRTL=1 right to left)
    static int force_strips = -1;
    if (force_strips < 0) {
        force_strips = getenv("NWSTRIPS") != nullptr;
        nw_scan_right_to_left = getenv("NWRTL") != nullptr;
    }
    if (force_strips) nw_overflowed = true;
#endif
    bool view3d = view && (!automapactive || am_overlay);
    bool strips = view3d && nw_overflowed;
    nw_flats_ready();
    int fuzz_end = fuzzpos;
#if HOST
    {
        extern int nw_opening_count, nw_vissprite_count;
        if (view && !strips) {
            if (nw_plane_count > (int) nw_peak[0]) nw_peak[0] = nw_plane_count;
            if (nw_drawseg_count > (int) nw_peak[1]) nw_peak[1] = nw_drawseg_count;
            if (nw_opening_count > (int) nw_peak[2]) nw_peak[2] = nw_opening_count;
            if (nw_vissprite_count > (int) nw_peak[3]) nw_peak[3] = nw_vissprite_count;
        }
        nw_last_walls = view && !strips ? render_col_count : 0;
        nw_last_planes = view && !strips ? nw_plane_count : 0;
    }
    if (view && getenv("FUZZTRACE")) printf("FUZZ %d %d %d%d\n", leveltime, fuzzpos, gameepisode, gamemap);
#endif
    if (view3d) {
        nw_col_frames[render_col_count / 250 < 40 ? render_col_count / 250 : 39]++;
        fuzz_end = nw_fuzz_prepass(fuzzpos);
    }

    // what is drawn over the picture, gathered once
    nw_patches[0].header.max = count_of(nw_patches) - 1;
    V_BeginPatchList(nw_patches);
    if (view3d) nw_draw_bezel();
    if (!help) {
        if (gamestate == GS_LEVEL) ST_Drawer(viewheight == SCREENHEIGHT, true);
        else if (gamestate == GS_INTERMISSION) WI_Drawer();
    } else if (gamestate == GS_LEVEL) {
        ST_doPaletteStuff();
    }
    ST_FpsDrawer(-1);
    if (gamestate == GS_LEVEL && gametic && !inhelpscreens) HU_Drawer();
    if (paused) {
        int y = automapactive ? 4 : viewwindowy + 4;
        V_DrawPatchDirect(viewwindowx + (scaledviewwidth - 68) / 2, y, VPATCH_HANDLE(VPATCH_NAME(M_PAUSE)));
    }
    M_Drawer();

    nw_frame_view = view;
    nw_frame_strips = strips;
    nw_frame_help = help;
    fuzzpos = (int8_t) fuzz_end;
    // (a wipe: the old screen stays, and melts into this one, nw_melt)
    if (!wipe_start) nw_draw_frame();
}
