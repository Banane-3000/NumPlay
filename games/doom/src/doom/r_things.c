//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Refresh of things, i.e. objects represented by sprites.
//

#if NO_DRAW_SPRITES
int no_draw_sprites = 1;
#else
int no_draw_sprites;
#endif
#if NO_DRAW_PSPRITES
int no_draw_psprites = 1;
#else
int no_draw_psprites;
#endif

#include <stdio.h>
#include <stdlib.h>
#if DOOM_TINY
#include <tiny_huff.h>
#endif

#include "deh_main.h"
#include "doomdef.h"

#include "i_swap.h"
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "r_local.h"

#include "doomstat.h"
#if PICO_DOOM

#include "picodoom.h"
#include "v_patch.h"
#endif


#define MINZ                (FRACUNIT*4)
#define BASEYCENTER            (SCREENHEIGHT/2)

//void R_DrawColumn (void);
//void R_DrawFuzzColumn (void);



typedef struct {
    int x1;
    int x2;

    int column;
    int topclip;
    int bottomclip;

} maskdraw_t;


//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//
fixed_t pspritescale;
fixed_t pspriteiscale;

#if !USE_LIGHTMAP_INDEXES
const lighttable_t**	spritelights;
#else
int8_t *spritelights;
#endif

// constant arrays
//  used for psprite clipping and initializing clipping
floor_ceiling_clip_t		minfloorceilingcliparray[SCREENWIDTH];
floor_ceiling_clip_t		maxfloorceilingcliparray[SCREENWIDTH];

//
// INITIALIZATION FUNCTIONS
//

// variables used to look up
//  and range check thing_t sprites patches
#if !USE_WHD
spritedef_t *sprites;
#endif
cardinal_t
numsprites;

#if !USE_WHD
typedef struct sprite_init_state_s {
    spriteframe_t sprtemp[29];
    int maxframe;
    const char *spritename;
} sprite_init_state_t;

//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
static void
R_InstallSpriteLump
        (sprite_init_state_t *init, int lump,
         unsigned frame,
         unsigned rotation,
         boolean flipped) {
    int r;

    if (frame >= 29 || rotation > 8)
        I_Error("R_InstallSpriteLump: "
                "Bad frame characters in lump %i", lump);

    if ((int) frame > init->maxframe)
        init->maxframe = frame;

    if (rotation == 0) {
        // the lump should be used for all rotations
        if (init->sprtemp[frame].rotate == false)
            I_Error("R_InitSprites: Sprite %s frame %c has "
                    "multip rot=0 lump", init->spritename, 'A' + frame);

        if (init->sprtemp[frame].rotate == true)
            I_Error("R_InitSprites: Sprite %s frame %c has rotations "
                    "and a rot=0 lump", init->spritename, 'A' + frame);

        init->sprtemp[frame].rotate = false;
#if DOOM_SMALL
        init->sprtemp[frame].flips = flipped ? 0xff : 0;
#endif
        for (r = 0; r < 8; r++) {
            init->sprtemp[frame].lump[r] = lump - firstspritelump;
#if !DOOM_SMALL
            init->sprtemp[frame].flip[r] = (byte) flipped;
#endif
        }
        return;
    }

    // the lump is only used for one rotation
    if (init->sprtemp[frame].rotate == false) {
        I_Error("R_InitSprites: Sprite %s frame %c has rotations "
                "and a rot=0 lump", init->spritename, 'A' + frame);
    }

    init->sprtemp[frame].rotate = true;

    // make 0 based
    rotation--;
    if (init->sprtemp[frame].lump[rotation] != -1)
        I_Error("R_InitSprites: Sprite %s : %c : %c "
                "has two lumps mapped to it",
                init->spritename, 'A' + frame, '1' + rotation);

    init->sprtemp[frame].lump[rotation] = lump - firstspritelump;
#if !DOOM_SMALL
    init->sprtemp[frame].flip[rotation] = (byte) flipped;
#else
    if (flipped) init->sprtemp[frame].flips |= (1u << rotation);
#endif
}


//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//  (4 chars exactly) to be used.
// Builds the sprite rotation matrixes to account
//  for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant. 
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//  a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//  letter/number appended.
// The rotation character can be 0 to signify no rotations.
//
void R_InitSpriteDefs(const char **namelist) {
    const char **check;
    int i;
    int l;
    int frame;
    int rotation;
    int start;
    int end;
    int patched;

    sprite_init_state_t init;

    // count the number of sprite names
    check = namelist;
    while (*check != NULL)
        check++;

    numsprites = check - namelist;

    if (!numsprites)
        return;

//    printf("SEG LOAD map %d segs x 0x%03x : size = %08x\n", numsegs, (int)sizeof(mapseg_t), numsegs*(int)sizeof(mapseg_t));
    printf("SPRITE LOAD alloc %d sprites x 0x%03x : size = %08x\n", numsprites, (int)sizeof(*sprites), numsprites*(int)sizeof(*sprites));
    sprites = Z_Malloc(numsprites * sizeof(*sprites), PU_STATIC, 0);

    start = firstspritelump - 1;
    end = lastspritelump + 1;

    int totalframes=0;
    // scan all the lump names for each of the names,
    //  noting the highest frame letter.
    // Just compare 4 characters as ints
    for (i = 0; i < numsprites; i++) {
        init.spritename = DEH_String(namelist[i]);
        memset(init.sprtemp, -1, sizeof(init.sprtemp));

        init.maxframe = -1;

#if DOOM_SMALL
        for(int j=0;j< count_of(init.sprtemp); j++) {
            init.sprtemp[j].flips = 0;
        }
#endif

        // scan the lumps,
        //  filling in the frames for whatever is found
        for (l = start + 1; l < end; l++) {
            should_be_const lumpinfo_t *info = lump_info(l);
            if (!strncasecmp(info->name, init.spritename, 4)) {
                frame = info->name[4] - 'A';
                rotation = info->name[5] - '0';

                if (modifiedgame)
                    patched = W_GetNumForName(info->name);
                else
                    patched = l;

                R_InstallSpriteLump(&init, patched, frame, rotation, false);

                if (info->name[6]) {
                    frame = info->name[6] - 'A';
                    rotation = info->name[7] - '0';
                    R_InstallSpriteLump(&init, l, frame, rotation, true);
                }
            }
        }

        // check the frames that were found for completeness
        if (init.maxframe == -1) {
            sprites[i].numframes = 0;
            continue;
        }

        init.maxframe++;

        for (frame = 0; frame < init.maxframe; frame++) {
            switch ((int) init.sprtemp[frame].rotate) {
                case -1:
                    // no rotations were found for that frame at all
                    I_Error("R_InitSprites: No patches found "
                            "for %s frame %c", init.spritename, frame + 'A');
                    break;

                case 0:
                    // only the first rotation is needed
                    break;

                case 1:
                    // must have all 8 frames
                    for (rotation = 0; rotation < 8; rotation++)
                        if (init.sprtemp[frame].lump[rotation] == -1)
                            I_Error("R_InitSprites: Sprite %s frame %c "
                                    "is missing rotations",
                                    init.spritename, frame + 'A');
                    break;
            }
        }

        // allocate space for the frames present and copy init.sprtemp to it
        sprites[i].numframes = init.maxframe;
        sprites[i].spriteframes =
                Z_Malloc(init.maxframe * sizeof(spriteframe_t), PU_STATIC, 0);
        memcpy(sprites[i].spriteframes, init.sprtemp, init.maxframe * sizeof(spriteframe_t));
    }
    printf("SPRITEFRAME LOAD alloc %d frames x 0x%03x : size = %08x\n", totalframes, (int)sizeof(spriteframe_t), totalframes*(int)sizeof(spriteframe_t));
}
#endif

//
// GAME FUNCTIONS
//
#if !NO_VISSPRITES
vissprite_t vissprites[MAXVISSPRITES];
vissprite_t *vissprite_p;
int newvissprite;
#endif


//
// R_InitSprites
// Called at program start.
//
void R_InitSprites() {
    int i;

#if FLOOR_CEILING_CLIP_OFFSET != 1
    for (i = 0; i < SCREENWIDTH; i++) {
        minfloorceilingcliparray[i] = FLOOR_CEILING_CLIP_OFFSET - 1;
    }
#endif

#if !USE_WHD
    R_InitSpriteDefs(sprnames);
#else
    numsprites = NUMSPRITES;
#endif
}


//
// R_ClearSprites
// Called at frame start.
//
#if NUMWORKS
void nw_clear_vissprites(void);
#endif
void R_ClearSprites(void) {
#if !NO_VISSPRITES
    vissprite_p = vissprites;
#elif NUMWORKS
    nw_clear_vissprites();
#endif
}


//
// R_NewVisSprite
//
vissprite_t overflowsprite;

#if !NO_VISSPRITES
vissprite_t *R_NewVisSprite(void) {
    if (vissprite_p == &vissprites[MAXVISSPRITES])
        return &overflowsprite;

    vissprite_p++;
    return vissprite_p - 1;
}
#endif


//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
floor_ceiling_clip_t*		mfloorclip;
floor_ceiling_clip_t*		mceilingclip;

fixed_t spryscale;
fixed_t sprtopscreen;

void R_DrawMaskedColumn(maskedcolumn_t column) {
    int topscreen;
    int bottomscreen;
#if !USE_WHD
    fixed_t basetexturemid;
    basetexturemid = dc_texturemid;
    for (; column->topdelta != 0xff;) {
        // calculate unclipped screen coordinates
        //  for post
        topscreen = sprtopscreen + spryscale * column->topdelta;
        bottomscreen = topscreen + spryscale * column->length;

        dc_yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
        dc_yh = (bottomscreen - 1) >> FRACBITS;

#if !NO_MASKED_FLOOR_CLIP
        if (dc_yh >= mfloorclip[dc_x])
            dc_yh = mfloorclip[dc_x] - 1;
        if (dc_yl <= mceilingclip[dc_x])
            dc_yl = mceilingclip[dc_x] + 1;
#else
        if (dc_yl < 0) dc_yl = 0;
        if (dc_yh >= viewheight) dc_yh = viewheight - 1;
#endif

        if (dc_yl <= dc_yh) {
            dc_source = (const byte *) column + 3;
            dc_texturemid = basetexturemid - (column->topdelta << FRACBITS);
            // dc_source = (byte *)column + 3 - column->topdelta;

            // Drawn by either R_DrawColumn
            //  or (SHADOW) R_DrawFuzzColumn.
#if PD_COLUMNS
            pd_add_column(PDCOL_MASKED);
#endif
#if !NO_DRAW_MASKED
            colfunc();
#endif
        }
        column = (column_t *) ((byte *) column + column->length + 4);
    }
    dc_texturemid = basetexturemid;
#else
    if (column.real_id >= 0) {
        panic_unsupported(); // handled earlier in r_segs.
//        topscreen = sprtopscreen;
//        bottomscreen = topscreen + spryscale * column.height;
//
//        dc_yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
//        dc_yh = (bottomscreen - 1) >> FRACBITS;
//
//        if (dc_yh >= mfloorclip[dc_x] - FLOOR_CEILING_CLIP_OFFSET)
//            dc_yh = mfloorclip[dc_x] - FLOOR_CEILING_CLIP_OFFSET - 1;
//        if (dc_yl <= mceilingclip[dc_x] - FLOOR_CEILING_CLIP_OFFSET)
//            dc_yl = mceilingclip[dc_x] - FLOOR_CEILING_CLIP_OFFSET + 1;
//        dc_source.tex = column.tex_or_patch;
//        dc_source.col = column.col;
//        // single masked column
//        pd_add_column(PDCOL_MASKED);
    } else {
        patch_t *patch = W_CacheLumpNum(-(int)column.real_id, PU_CACHE);
        assert(patch);
        assert(column.col >=0 && column.col < patch_width(patch));
        dc_source = column;
#define MAX_SEGS 64
        uint8_t ysegs[MAX_SEGS*3];
        int seg_count = 0;
        int height = patch_height(patch);
        if (patch_fully_opaque(patch)) {
            int yl, yh;
            // calculate unclipped screen coordinates
            //  for post
            topscreen = sprtopscreen;
            bottomscreen = topscreen + spryscale * height;

            yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
            yh = (bottomscreen - 1) >> FRACBITS;

#if !NO_MASKED_FLOOR_CLIP
            if (yh >= mfloorclip[dc_x])
                yh = mfloorclip[dc_x] - 1;
            if (yl <= mceilingclip[dc_x])
                yl = mceilingclip[dc_x] + 1;
#else
            if (yl < 0) yl = 0;
            if (yh >= viewheight) yh = viewheight - 1;
#endif

            if (yl <= yh) {
                ysegs[0] = yl;
                ysegs[1] = yh;
                ysegs[2] = 0;
                seg_count++;
            }
        } else {
            int yl, yh;
            int last = -1;
            th_backwards_bit_input rbi;
            uint data_index = 3 + patch_has_extra(patch);
            data_index += ((uint8_t*)patch)[data_index*2]; // skip over decoder metadata
            uint16_t *col_offsets = &((uint16_t*)patch)[data_index];
            uint16_t col_offset = col_offsets[column.col];
            uint next_column;
            if (0xff == (col_offset >> 8)) {
                next_column = (col_offset & 0xff);
                assert(next_column < patch_width(patch));
                next_column++;
            } else {
                next_column = column.col + 1;
            }
            col_offset = col_offsets[next_column]; // we work backwards from the next column
            // we have to skip over any columns which aren't stored
            while (0xff == (col_offset >> 8)) {
                assert(next_column < patch_width(patch)); // note < and ++ afterwards; we are allowed to read one beyond width which is the "end" marker column
                col_offset = col_offsets[++next_column];
            }
            data_index = (data_index + patch_width(patch)) * 2 + 2; // + 2 because we have one extra col_data offset
            if (patch_byte_addressed(patch)) {
                th_backwards_bit_input_init(&rbi, patch + data_index + col_offset); // todo read off end potential
            } else {
                th_backwards_bit_input_init_bit_offset(&rbi, patch + data_index, col_offset); // todo read off end potential
            }
            int prev = 0;
            int base_offset = 0;
            do {
                int top = prev + th_read_backwards_bits(&rbi, bitcount8(height - prev));
                if (top == height) break;
                if (top > height) {
                    assert(false);
                }
                base_offset += (top - prev);
                int size = th_read_backwards_bits(&rbi, bitcount8(height - top));
                assert(size >= 1);
                prev = top + size;

                // calculate unclipped screen coordinates
                //  for post
                topscreen = sprtopscreen + spryscale * top;
                bottomscreen = topscreen + spryscale * size;

                yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
                yh = (bottomscreen - 1) >> FRACBITS;

    #if !NO_MASKED_FLOOR_CLIP
                if (yh >= mfloorclip[dc_x])
                    yh = mfloorclip[dc_x] - 1;
                if (yl <= mceilingclip[dc_x])
                    yl = mceilingclip[dc_x] + 1;
    #else
                if (yl < 0) yl = 0;
                if (yh >= viewheight) yh = viewheight - 1;
    #endif

                if (yl <= yh) {

                    if (yl > last) {
                        ysegs[seg_count*3] = yl;
                        ysegs[seg_count*3+1] = yh;
                        assert(base_offset < 256);
                        ysegs[seg_count*3+2] = base_offset;
                        seg_count++;
                    }
                    last = yh;
                }
            } while (true);
        }
        assert(seg_count < MAX_SEGS);
        if (seg_count)
            pd_add_masked_columns(ysegs, seg_count);
    }
#endif
}


//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
void
R_DrawVisSprite
        (vissprite_t *vis,
         int x1,
         int x2) {
    int texturecolumn;
    fixed_t frac;
    should_be_const patch_t *patch;


#if !USE_WHD
    patch = W_CacheLumpNum(vis->patch + firstspritelump, PU_CACHE);
#else
    patch = NULL;
#endif

    byte fuzz;
#if !USE_LIGHTMAP_INDEXES
    dc_colormap = vis->colormap;
    fuzz = !dc_colormap;
#else
    fuzz = vis->colormap < 0;
#if !NO_USE_DC_COLORMAP
    dc_colormap = vis->colormap < 0 ? NULL : colormaps + vis->colormap * 256;
#else
    dc_colormap_index = vis->colormap;
#endif
#endif

#if DOOM_TINY
    dc_translation_index = 0;
#endif
    if (fuzz) {
        // NULL colormap = shadow draw
#if !PD_COLUMNS
        colfunc = fuzzcolfunc;
#endif
    } else if (vis->mobjflags & MF_TRANSLATION) {
#if !PD_COLUMNS
        colfunc = transcolfunc;
#endif
#if !DOOM_TINY
        dc_translation = translationtables - 256 +
                         ((vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT - 8));
#else
        dc_translation_index = (vis->mobjflags & MF_TRANSLATION) >>  MF_TRANSSHIFT;
#endif
    }

    dc_iscale = abs(vis->xiscale) >> detailshift;
    dc_texturemid = vis->texturemid;
    frac = vis->startfrac;
    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);

#if PD_SCALE_SORT
    pd_scale = spryscale;
#endif

#if USE_WHD
    framedrawable_t *fd = lookup_patch(vis->patch);
#endif
#if NUMWORKS
    extern int nw_strip_x0, nw_strip_x1;
#endif
    for (dc_x = vis->x1; dc_x <= vis->x2; dc_x++, frac += vis->xiscale) {
#if NUMWORKS
        if (dc_x < nw_strip_x0 || dc_x >= nw_strip_x1) continue; // (outside the strip being drawn)
#endif
        texturecolumn = frac >> FRACBITS;
#ifdef RANGECHECK
        if (patch && (texturecolumn < 0 || texturecolumn >= patch_width(patch)))
            I_Error("R_DrawSpriteRange: bad texturecolumn");
#endif
#if !USE_WHD
        maskedcolumn_t column = R_GetPatchColumn(patch, texturecolumn);
#else
        maskedcolumn_t column = R_GetPatchColumn(fd, texturecolumn);
#endif
        R_DrawMaskedColumn(column);
    }

#if !PD_COLUMNS
    colfunc = basecolfunc;
#endif
}


#if NUMWORKS
#include "nw/nw_render.h"
// vanilla's vissprites (MAXVISSPRITES of them, the others dropped as vanilla does), drawn band by band
static vissprite_t nw_vissprites[MAXVISSPRITES];
static uint8_t nw_vis_order[MAXVISSPRITES];
int nw_vissprite_count;
void nw_clear_vissprites(void) { nw_vissprite_count = 0; }
static vissprite_t *nw_new_vissprite(void) {
    if (nw_vissprite_count == MAXVISSPRITES)
        return &overflowsprite;
    return &nw_vissprites[nw_vissprite_count++];
}
#endif

//
// R_ProjectSprite
// Generates a vissprite for a thing
//  if it might be visible.
//
void R_ProjectSprite(mobj_t *thing) {
    fixed_t tr_x;
    fixed_t tr_y;

    fixed_t gxt;
    fixed_t gyt;

    fixed_t tx;
    fixed_t tz;

    fixed_t xscale;

    int x1;
    int x2;

    spriteframeref_t sprframe;
    int lump;

    unsigned rot;
    boolean flip;

    int index;

    vissprite_t *vis;

    angle_t ang;
    fixed_t iscale;

    // transform the origin point
    tr_x = thing->xy.x - viewx;
    tr_y = thing->xy.y - viewy;

    gxt = FixedMul(tr_x, viewcos);
    gyt = -FixedMul(tr_y, viewsin);

    tz = gxt - gyt;

    // thing is behind view plane?
    if (tz < MINZ)
        return;

    xscale = FixedDiv(projection, tz);

    gxt = -FixedMul(tr_x, viewsin);
    gyt = FixedMul(tr_y, viewcos);
    tx = -(gyt + gxt);

    // too far off the side?
    if (abs(tx) > (tz << 2))
        return;

    // decide which patch to use for sprite relative to player
#ifdef RANGECHECK
    if ((unsigned int) mobj_sprite(thing) >= (unsigned int) numsprites)
        I_Error("R_ProjectSprite: invalid sprite number %i ",
                mobj_sprite(thing));
#endif

#ifdef RANGECHECK
    if ((mobj_frame(thing) & FF_FRAMEMASK) >= sprite_numframes(mobj_sprite(thing)))
        I_Error("R_ProjectSprite: invalid sprite frame %i : %i ",
                mobj_sprite(thing), mobj_frame(thing));
#endif
    sprframe = sprite_frame(mobj_sprite(thing), mobj_frame(thing) & FF_FRAMEMASK);

    if (spriteframe_rotates(sprframe)) {
        // choose a different rotation based on player view
        ang = R_PointToAngle(thing->xy.x, thing->xy.y);
        rot = (ang - mobj_full(thing)->angle + (unsigned) (ANG45 / 2) * 9) >> 29;
        lump = spriteframe_rotated_pic(sprframe, rot);
        flip = (boolean) spriteframe_rotated_flipped(sprframe, rot);
    } else {
        // use single rotation for all views
        lump = spriteframe_unrotated_pic(sprframe);
        flip = (boolean) spriteframe_unrotated_flipped(sprframe);
    }

    // calculate edges of the shape
    tx -= sprite_offset(lump);
    x1 = (centerxfrac + FixedMul(tx, xscale)) >> FRACBITS;

    // off the right side?
    if (x1 > viewwidth)
        return;

    tx += sprite_width(lump);
    x2 = ((centerxfrac + FixedMul(tx, xscale)) >> FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
        return;

    // store information in a vissprite
#if NUMWORKS
    vis = nw_new_vissprite();
#elif NO_VISSPRITES
    vissprite_t vis_a;
    vis = &vis_a;
#else
    vis = R_NewVisSprite();
#endif
    vis->mobjflags = thing->flags;
    vis->scale = xscale << detailshift;
#if !DOOM_TINY || NUMWORKS
    vis->gx = thing->xy.x;
    vis->gy = thing->xy.y;
    vis->gz = thing->z;
    vis->gzt = thing->z + sprite_topoffset(lump);
    vis->texturemid = vis->gzt - viewz;
#else
    vis->texturemid = thing->z + sprite_topoffset(lump) - viewz;
#endif
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth - 1 : x2;
    iscale = FixedDiv(FRACUNIT, xscale);

    if (flip) {
        vis->startfrac = sprite_width(lump) - 1;
        vis->xiscale = -iscale;
    } else {
        vis->startfrac = 0;
        vis->xiscale = iscale;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);
    vis->patch = lump;

    // get light level
    if (thing->flags & MF_SHADOW) {
        // shadow draw
#if !USE_LIGHTMAP_INDEXES
        vis->colormap = NULL;
#else
        vis->colormap = -1;
#endif
    } else if (fixedcolormap) {
        // fixed map
        vis->colormap = fixedcolormap;
    } else if (mobj_frame(thing) & FF_FULLBRIGHT) {
        // full bright
#if !USE_LIGHTMAP_INDEXES
        vis->colormap = colormaps;
#else
        vis->colormap = 0;
#endif
    } else {
        // diminished light
        index = xscale >> (LIGHTSCALESHIFT - detailshift);

        if (index >= MAXLIGHTSCALE)
            index = MAXLIGHTSCALE - 1;

        vis->colormap = spritelights[index];
    }
#if !NUMWORKS
    R_DrawSpriteEarly(vis);
#endif
}


//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
void R_AddSprites(sector_t *sec) {
    mobj_t *thing;
    int lightnum;

#if !SPRITES_IN_SUBSECTORS
    // BSP is traversed by subsector.
    // A sector might have been split into several
    //  subsectors during BSP building.
    // Thus we check whether its already added.
    if (sector_validcount_update_check(sec, validcount)) return;
#endif

    lightnum = (sec->lightlevel >> LIGHTSEGSHIFT) + extralight;

    if (lightnum < 0)
        spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = scalelight[LIGHTLEVELS - 1];
    else
        spritelights = scalelight[lightnum];

    // Handle all things in sector.
    for (thing = shortptr_to_mobj(sec->thinglist); thing; thing = mobj_snext(thing))
        R_ProjectSprite(thing);
}


//
// R_DrawPSprite
//
void R_DrawPSprite(pspdef_t *psp) {
    fixed_t tx;
    int x1;
    int x2;
    spriteframeref_t sprframe;
    int lump;
    boolean flip;
    vissprite_t *vis;
    vissprite_t avis;

    // decide which patch to use
#ifdef RANGECHECK
    if ((unsigned) psp->state->sprite >= (unsigned int) numsprites)
        I_Error("R_ProjectSprite: invalid sprite number %i ",
                psp->state->sprite);
#endif
#ifdef RANGECHECK
    if ((psp->state->frame & FF_FRAMEMASK) >= sprite_numframes(psp->state->sprite))
        I_Error("R_ProjectSprite: invalid sprite frame %i : %i ",
                psp->state->sprite, psp->state->frame);
#endif
    sprframe = sprite_frame(psp->state->sprite, psp->state->frame & FF_FRAMEMASK);
    lump = spriteframe_unrotated_pic(sprframe);
    flip = (boolean) spriteframe_unrotated_flipped(sprframe);

    // calculate edges of the shape
    tx = psp->sx - (SCREENWIDTH / 2) * FRACUNIT;

    tx -= sprite_offset(lump);
    x1 = (centerxfrac + FixedMul(tx, pspritescale)) >> FRACBITS;

    // off the right side
    if (x1 > viewwidth)
        return;

    tx += sprite_width(lump);
    x2 = ((centerxfrac + FixedMul(tx, pspritescale)) >> FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
        return;

    // store information in a vissprite
    vis = &avis;
    vis->mobjflags = 0;
    vis->texturemid = (BASEYCENTER << FRACBITS) + FRACUNIT / 2 - (psp->sy - sprite_topoffset(lump));
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth - 1 : x2;
    vis->scale = pspritescale << detailshift;

    if (flip) {
        vis->xiscale = -pspriteiscale;
        vis->startfrac = sprite_width(lump) - 1;
    } else {
        vis->xiscale = pspriteiscale;
        vis->startfrac = 0;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);

    vis->patch = lump;

    if (viewplayer->powers[pw_invisibility] > 4 * 32
        || viewplayer->powers[pw_invisibility] & 8) {
        // shadow draw
#if !USE_LIGHTMAP_INDEXES
        vis->colormap = NULL;
#else
        vis->colormap = -1;
#endif
    } else if (fixedcolormap) {
        // fixed color
        vis->colormap = fixedcolormap;
    } else if (psp->state->frame & FF_FULLBRIGHT) {
        // full bright
#if !USE_LIGHTMAP_INDEXES
        vis->colormap = colormaps;
#else
        vis->colormap = 0;
#endif
    } else {
        // local light
        vis->colormap = spritelights[MAXLIGHTSCALE - 1];
    }

#if PICO_DOOM
    pd_flag |= 2;
#endif
    R_DrawVisSprite(vis, vis->x1, vis->x2);
#if PICO_DOOM
    pd_flag &= ~2;
#endif
}


//
// R_DrawPlayerSprites
//
void R_DrawPlayerSprites(void) {
    int i;
    int lightnum;
    pspdef_t *psp;

    // get light level
    lightnum =
            (mobj_sector(viewplayer->mo)->lightlevel >> LIGHTSEGSHIFT)
            + extralight;

    if (lightnum < 0)
        spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = scalelight[LIGHTLEVELS - 1];
    else
        spritelights = scalelight[lightnum];

    // clip to screen bounds
    mfloorclip = maxfloorceilingcliparray;
    mceilingclip = minfloorceilingcliparray;

    // add all active psprites
    for (i = 0, psp = viewplayer->psprites;
         i < NUMPSPRITES;
         i++, psp++) {
        if (psp->state)
            R_DrawPSprite(psp);
    }
}


//
// R_SortVisSprites
//
vissprite_t vsprsortedhead;


#if !NO_VISSPRITES
void R_SortVisSprites(void) {
    int i;
    int count;
    vissprite_t *ds;
    vissprite_t *best;
    vissprite_t unsorted;
    fixed_t bestscale;

    count = vissprite_p - vissprites;

    unsorted.next = unsorted.prev = &unsorted;

    if (!count)
        return;

    for (ds = vissprites; ds < vissprite_p; ds++) {
        ds->next = ds + 1;
        ds->prev = ds - 1;
    }

    vissprites[0].prev = &unsorted;
    unsorted.next = &vissprites[0];
    (vissprite_p - 1)->next = &unsorted;
    unsorted.prev = vissprite_p - 1;

    // pull the vissprites out by scale

    vsprsortedhead.next = vsprsortedhead.prev = &vsprsortedhead;
    for (i = 0; i < count; i++) {
        bestscale = INT_MAX;
        best = unsorted.next;
        for (ds = unsorted.next; ds != &unsorted; ds = ds->next) {
            if (ds->scale < bestscale) {
                bestscale = ds->scale;
                best = ds;
            }
        }
        best->next->prev = best->prev;
        best->prev->next = best->next;
        best->next = &vsprsortedhead;
        best->prev = vsprsortedhead.prev;
        vsprsortedhead.prev->next = best;
        vsprsortedhead.prev = best;
    }
}
#endif

//
// R_DrawSprite
//
void R_DrawSprite(vissprite_t *spr) {
#if !NO_DRAWSEGS
    short clipbot[SCREENWIDTH];
    short cliptop[SCREENWIDTH];
    drawseg_t *ds;
    int x;
    int r1;
    int r2;
    fixed_t scale;
    fixed_t lowscale;
    int silhouette;

    for (x = spr->x1; x <= spr->x2; x++)
        clipbot[x] = cliptop[x] = -2;

    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale
    //  is the clip seg.
    for (ds = ds_p - 1; ds >= drawsegs; ds--) {
        // determine if the drawseg obscures the sprite
        if (ds->x1 > spr->x2
            || ds->x2 < spr->x1
            || (!ds->silhouette
                && !ds->maskedtexturecol)) {
            // does not cover sprite
            continue;
        }

        r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
        r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

        if (ds->scale1 > ds->scale2) {
            lowscale = ds->scale2;
            scale = ds->scale1;
        } else {
            lowscale = ds->scale1;
            scale = ds->scale2;
        }

        if (scale < spr->scale
            || (lowscale < spr->scale
                && !R_PointOnSegSide(spr->gx, spr->gy, ds->curline))) {
            // masked mid texture?
#if !PD_SCALE_SORT
            if (ds->maskedtexturecol)
                R_RenderMaskedSegRange(ds, r1, r2);
#endif
            // seg is behind sprite
            continue;
        }


        // clip this piece of the sprite
        silhouette = ds->silhouette;

        if (spr->gz >= ds->bsilheight)
            silhouette &= ~SIL_BOTTOM;

        if (spr->gzt <= ds->tsilheight)
            silhouette &= ~SIL_TOP;

        if (silhouette == 1) {
            // bottom sil
            for (x = r1; x <= r2; x++)
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];
        } else if (silhouette == 2) {
            // top sil
            for (x = r1; x <= r2; x++)
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
        } else if (silhouette == 3) {
            // both
            for (x = r1; x <= r2; x++) {
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
            }
        }

    }

    // all clipping has been performed, so draw the sprite

    // check for unclipped columns
    for (x = spr->x1; x <= spr->x2; x++) {
        if (clipbot[x] == -2)
            clipbot[x] = viewheight;

        if (cliptop[x] == -2)
            cliptop[x] = -1;
    }

    mfloorclip = clipbot;
    mceilingclip = cliptop;
#else
    mfloorclip = maxfloorceilingcliparray;
    mceilingclip = minfloorceilingcliparray;
#endif
    R_DrawVisSprite(spr, spr->x1, spr->x2);
}


//
// R_DrawMasked
//
void R_DrawMasked(void) {

    if (!no_draw_sprites) {
#if !NO_VISSPRITES
        vissprite_t *spr;
        R_SortVisSprites();
        if (vissprite_p > vissprites) {
            // draw all vissprites back to front
            for (spr = vsprsortedhead.next;
                 spr != &vsprsortedhead;
                 spr = spr->next) {

                R_DrawSprite(spr);
            }
        }
#endif
    }

    // todo graham - need to gather the masked seg ranges
#if !NO_DRAWSEGS
    drawseg_t *ds;
    // render any remaining masked mid textures
    for (ds = ds_p - 1; ds >= drawsegs; ds--)
        if (ds->maskedtexturecol)
            R_RenderMaskedSegRange(ds, ds->x1, ds->x2);
#endif

#if !PICODOOM_RENDER_NEWHOPE
    // draw the psprites on top of everything
    //  but does not draw on side views
    if (!viewangleoffset)
        R_DrawPlayerSprites();
#endif
}


//
// R_DrawSprite
//
void R_DrawSpriteEarly(vissprite_t *spr) {
#if PICO_DOOM
    pd_flag |= 1;
#if !NO_MASKED_FLOOR_CLIP
    mfloorclip = floorclip;
    mceilingclip = ceilingclip;
#endif

    R_DrawVisSprite(spr, spr->x1, spr->x2);
    pd_flag &= ~1;
#endif
}

#if NUMWORKS
// ---------------------------------------------------------------------------------------------------------------
// The calculator: vanilla's R_DrawMasked, for the columns of one band at a time (nw/nw_render.h)

// sprites back to front, as vanilla's selection sort leaves them (the first of equal scales first)
void nw_sort_vissprites(void) {
    for (int i = 0; i < nw_vissprite_count; i++) {
        int j = i;
        while (j > 0 && nw_vissprites[nw_vis_order[j - 1]].scale > nw_vissprites[i].scale) {
            nw_vis_order[j] = nw_vis_order[j - 1];
            j--;
        }
        nw_vis_order[j] = (uint8_t) i;
    }
}

// the clipping of the masked column being drawn, in screen rows (vanilla's mfloorclip and mceilingclip)
static int16_t nw_clipbot[SCREENWIDTH], nw_cliptop[SCREENWIDTH];
static boolean nw_fuzz;

// vanilla's R_DrawMaskedColumn: the posts of dc_x's column within the clip, to the back end
static void nw_draw_masked_column(maskedcolumn_t column) {
    int topscreen, bottomscreen, yl, yh;
    if (column.real_id >= 0) {
        // a masked middle texture's column: its patch's posts, if a single patch makes it
        dc_source = column;
        if (!nw_resolve_column()) return;
        column = dc_source;
    }
    if (column.real_id >= 0) {
        // more than one patch: drawn whole, as RP2040 Doom does
        topscreen = sprtopscreen;
        bottomscreen = topscreen + spryscale * (texture_height(column.real_id) >> FRACBITS);
        yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
        yh = (bottomscreen - 1) >> FRACBITS;
        if (yh >= nw_clipbot[dc_x]) yh = nw_clipbot[dc_x] - 1;
        if (yl <= nw_cliptop[dc_x]) yl = nw_cliptop[dc_x] + 1;
        if (yl <= yh) {
            dc_source = column;
            nw_masked_column(dc_x, yl, yh, 0, nw_fuzz);
        }
        return;
    }
    patch_t *patch = W_CacheLumpNum(-(int) column.real_id, PU_CACHE);
    dc_source = column;
    int height = patch_height(patch);
    if (patch_fully_opaque(patch)) {
        topscreen = sprtopscreen;
        bottomscreen = topscreen + spryscale * height;
        yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
        yh = (bottomscreen - 1) >> FRACBITS;
        if (yh >= nw_clipbot[dc_x]) yh = nw_clipbot[dc_x] - 1;
        if (yl <= nw_cliptop[dc_x]) yl = nw_cliptop[dc_x] + 1;
        if (yl <= yh) nw_masked_column(dc_x, yl, yh, 0, nw_fuzz);
        return;
    }
    // the posts, from the packed column's run data (as RP2040 Doom reads them)
    th_backwards_bit_input rbi;
    uint data_index = 3 + patch_has_extra(patch);
    data_index += ((uint8_t *) patch)[data_index * 2];
    uint16_t *col_offsets = &((uint16_t *) patch)[data_index];
    uint16_t col_offset = col_offsets[column.col];
    uint next_column;
    if (0xff == (col_offset >> 8)) {
        next_column = (col_offset & 0xff) + 1;
    } else {
        next_column = column.col + 1;
    }
    col_offset = col_offsets[next_column];
    while (0xff == (col_offset >> 8)) col_offset = col_offsets[++next_column];
    data_index = (data_index + patch_width(patch)) * 2 + 2;
    if (patch_byte_addressed(patch)) {
        th_backwards_bit_input_init(&rbi, (const uint8_t *) patch + data_index + col_offset);
    } else {
        th_backwards_bit_input_init_bit_offset(&rbi, (const uint8_t *) patch + data_index, col_offset);
    }
    int prev = 0, base_offset = 0;
    do {
        int top = prev + th_read_backwards_bits(&rbi, bitcount8(height - prev));
        if (top >= height) break;
        base_offset += top - prev;
        int size = th_read_backwards_bits(&rbi, bitcount8(height - top));
        prev = top + size;
        topscreen = sprtopscreen + spryscale * top;
        bottomscreen = topscreen + spryscale * size;
        yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
        yh = (bottomscreen - 1) >> FRACBITS;
        if (yh >= nw_clipbot[dc_x]) yh = nw_clipbot[dc_x] - 1;
        if (yl <= nw_cliptop[dc_x]) yl = nw_cliptop[dc_x] + 1;
        if (yl <= yh) nw_masked_column(dc_x, yl, yh, base_offset, nw_fuzz);
    } while (true);
}

// vanilla's R_DrawVisSprite, for columns x1 to x2 of the sprite
static void nw_draw_vis_sprite(vissprite_t *vis, int x1, int x2) {
    nw_fuzz = vis->colormap < 0;
    dc_colormap_index = nw_fuzz ? 0 : vis->colormap;
    dc_iscale = abs(vis->xiscale) >> detailshift;
    dc_texturemid = vis->texturemid;
    nw_masked_texturemid = dc_texturemid;
    fixed_t frac = vis->startfrac + (x1 - vis->x1) * vis->xiscale;
    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
    framedrawable_t *fd = lookup_patch(vis->patch);
    for (dc_x = x1; dc_x <= x2; dc_x++, frac += vis->xiscale) {
        nw_draw_masked_column(R_GetPatchColumn(fd, frac >> FRACBITS));
    }
    nw_masked_flush();
}

// vanilla's R_RenderMaskedSegRange
static void nw_masked_seg_range(nw_drawseg_t *ds, int x1, int x2) {
    if (x1 > x2) return;
    nw_masked_t *m = (nw_masked_t *) (nw_openings + ds->masked);
    int16_t *cols = (int16_t *) (m + 1) - m->x0;
    int lightnum = m->lightnum;
    const int8_t *walllights = scalelight[lightnum < 0 ? 0 : lightnum >= LIGHTLEVELS ? LIGHTLEVELS - 1 : lightnum];
    fixed_t step = ds->scalestep;
    spryscale = ds->scale1 + (x1 - ds->x1) * step;
    for (int x = x1; x <= x2; x++) {
        nw_cliptop[x] = ds->topclip == NW_OPEN_MAX ? viewheight
                        : ds->topclip == NW_OPEN_MIN ? -1
                        : nw_openings[ds->topclip + x - ds->x1] - FLOOR_CEILING_CLIP_OFFSET;
        nw_clipbot[x] = ds->botclip == NW_OPEN_MAX ? viewheight
                        : ds->botclip == NW_OPEN_MIN ? -1
                        : nw_openings[ds->botclip + x - ds->x1] - FLOOR_CEILING_CLIP_OFFSET;
    }
    dc_texturemid = m->texturemid;
    nw_masked_texturemid = dc_texturemid;
    nw_fuzz = false;
    framedrawable_t *fd = lookup_masked_texture(m->texture);
    int width_mask = whd_textures[m->texture].width - 1;
    if (fixedcolormap) dc_colormap_index = fixedcolormap;
    for (dc_x = x1; dc_x <= x2; dc_x++, spryscale += step) {
        if (cols[dc_x] == SHRT_MAX) continue;
        if (!fixedcolormap) {
            unsigned index = spryscale >> LIGHTSCALESHIFT;
            if (index >= MAXLIGHTSCALE) index = MAXLIGHTSCALE - 1;
            dc_colormap_index = walllights[index];
        }
        sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
        dc_iscale = 0xffffffffu / (unsigned) spryscale;
        nw_draw_masked_column(R_GetMaskedColumn(fd, cols[dc_x] & width_mask));
        cols[dc_x] = SHRT_MAX;
    }
    nw_masked_flush();
}

// vanilla's R_DrawSprite: the sprite clipped by the drawsegs in front of it (the masked ones behind it drawn
// first), for the band's columns; from x_from on for the clipping (a fuzzy sprite counts its pixels from its
// first column, its fuzz going on from there)
static void nw_draw_sprite(vissprite_t *spr, int x_from) {
    int a = spr->x1 > nw_band_x0 ? spr->x1 : nw_band_x0;
    int b = spr->x2 < nw_band_x1 - 1 ? spr->x2 : nw_band_x1 - 1;
    int c0 = x_from < a ? x_from : a;
    for (int x = c0; x <= b; x++) nw_clipbot[x] = nw_cliptop[x] = -2;
    for (int i = nw_drawseg_count - 1; i >= 0; i--) {
        nw_drawseg_t *ds = &nw_drawsegs[i];
        if (ds->x1 > spr->x2 || ds->x2 < spr->x1 || (!ds->silhouette && ds->masked == NW_OPEN_NONE))
            continue;
        int r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
        int r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;
        fixed_t scale, lowscale;
        if (ds->scale1 > ds->scale2) {
            lowscale = ds->scale2;
            scale = ds->scale1;
        } else {
            lowscale = ds->scale1;
            scale = ds->scale2;
        }
        if (scale < spr->scale
            || (lowscale < spr->scale && !R_PointOnSegSide(spr->gx, spr->gy, (seg_t *) ds->curline))) {
            // masked mid texture behind the sprite: drawn first (in the band)
            if (ds->masked != NW_OPEN_NONE) {
                int m1 = r1 > nw_band_x0 ? r1 : nw_band_x0;
                int m2 = r2 < nw_band_x1 - 1 ? r2 : nw_band_x1 - 1;
                nw_masked_seg_range(ds, m1, m2);
            }
            continue;
        }
        // clip this piece of the sprite
        int silhouette = ds->silhouette;
        if (spr->gz >= ds->bsilheight) silhouette &= ~SIL_BOTTOM;
        if (spr->gzt <= ds->tsilheight) silhouette &= ~SIL_TOP;
        if (r1 < c0) r1 = c0;
        if (r2 > b) r2 = b;
        // (a frame drawn band by band has the openings of the band's columns only)
        if (r1 < nw_strip_x0) r1 = nw_strip_x0;
        if (r2 >= nw_strip_x1) r2 = nw_strip_x1 - 1;
        for (int x = r1; x <= r2; x++) {
            if ((silhouette & SIL_BOTTOM) && nw_clipbot[x] == -2)
                nw_clipbot[x] = ds->botclip == NW_OPEN_MAX ? viewheight
                                : ds->botclip == NW_OPEN_MIN ? -1
                                : nw_openings[ds->botclip + x - ds->x1] - FLOOR_CEILING_CLIP_OFFSET;
            if ((silhouette & SIL_TOP) && nw_cliptop[x] == -2)
                nw_cliptop[x] = ds->topclip == NW_OPEN_MAX ? viewheight
                                : ds->topclip == NW_OPEN_MIN ? -1
                                : nw_openings[ds->topclip + x - ds->x1] - FLOOR_CEILING_CLIP_OFFSET;
        }
    }
    for (int x = c0; x <= b; x++) {
        if (nw_clipbot[x] == -2) nw_clipbot[x] = viewheight;
        if (nw_cliptop[x] == -2) nw_cliptop[x] = -1;
    }
    nw_draw_vis_sprite(spr, c0, b);
}

// vanilla's R_DrawPSprite and R_DrawPlayerSprites (on top of everything)
static void nw_draw_psprite(pspdef_t *psp, const int8_t *lights) {
    spriteframeref_t sprframe = sprite_frame(psp->state->sprite, psp->state->frame & FF_FRAMEMASK);
    int lump = spriteframe_unrotated_pic(sprframe);
    boolean flip = (boolean) spriteframe_unrotated_flipped(sprframe);
    fixed_t tx = psp->sx - (SCREENWIDTH / 2) * FRACUNIT - sprite_offset(lump);
    int x1 = (centerxfrac + FixedMul(tx, pspritescale)) >> FRACBITS;
    if (x1 > viewwidth) return;
    tx += sprite_width(lump);
    int x2 = ((centerxfrac + FixedMul(tx, pspritescale)) >> FRACBITS) - 1;
    if (x2 < 0) return;
    vissprite_t avis, *vis = &avis;
    vis->mobjflags = 0;
    vis->texturemid = (BASEYCENTER << FRACBITS) + FRACUNIT / 2 - (psp->sy - sprite_topoffset(lump));
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth - 1 : x2;
    vis->scale = pspritescale << detailshift;
    if (flip) {
        vis->xiscale = -pspriteiscale;
        vis->startfrac = sprite_width(lump) - 1;
    } else {
        vis->xiscale = pspriteiscale;
        vis->startfrac = 0;
    }
    if (vis->x1 > x1) vis->startfrac += vis->xiscale * (vis->x1 - x1);
    vis->patch = lump;
    if (viewplayer->powers[pw_invisibility] > 4 * 32 || viewplayer->powers[pw_invisibility] & 8) {
        vis->colormap = -1; // shadow draw
    } else if (fixedcolormap) {
        vis->colormap = fixedcolormap;
    } else if (psp->state->frame & FF_FULLBRIGHT) {
        vis->colormap = 0;
    } else {
        vis->colormap = lights[MAXLIGHTSCALE - 1];
    }
    int a = vis->x1 > nw_band_x0 ? vis->x1 : nw_band_x0;
    int b = vis->x2 < nw_band_x1 - 1 ? vis->x2 : nw_band_x1 - 1;
    // (a fuzzy weapon counts its pixels from its first column)
    int c0 = vis->colormap < 0 ? vis->x1 : a;
    for (int x = c0; x <= b; x++) {
        nw_clipbot[x] = viewheight;
        nw_cliptop[x] = -1;
    }
    if (c0 <= b) nw_draw_vis_sprite(vis, c0, b);
}

static void nw_draw_player_sprites(void) {
    int lightnum = (mobj_sector(viewplayer->mo)->lightlevel >> LIGHTSEGSHIFT) + extralight;
    const int8_t *lights = scalelight[lightnum < 0 ? 0 : lightnum >= LIGHTLEVELS ? LIGHTLEVELS - 1 : lightnum];
    pspdef_t *psp = viewplayer->psprites;
    for (int i = 0; i < NUMPSPRITES; i++, psp++)
        if (psp->state) nw_draw_psprite(psp, lights);
}

static void nw_draw_player_sprites(void);

// fuzz: where vanilla's fuzzpos is at each fuzzy sprite and at the weapon, the frame drawn in its order
// (the pixels counted, nothing drawn: no band)
static int nw_fuzz_psprites;
int nw_fuzz_prepass(int fuzzpos) {
    int bx0 = nw_band_x0, bx1 = nw_band_x1;
    nw_band_x0 = nw_band_x1 = SCREENWIDTH;
    nw_fuzz_at = fuzzpos;
    for (int k = 0; k < nw_vissprite_count; k++) {
        vissprite_t *spr = &nw_vissprites[nw_vis_order[k]];
        if (spr->colormap >= 0) continue;
        spr->fuzzbase = (uint8_t) nw_fuzz_at;
        nw_draw_sprite(spr, spr->x1);
    }
    nw_fuzz_psprites = nw_fuzz_at;
    if (!viewangleoffset && (viewplayer->powers[pw_invisibility] > 4 * 32 || viewplayer->powers[pw_invisibility] & 8))
        nw_draw_player_sprites();
    nw_band_x0 = bx0;
    nw_band_x1 = bx1;
    return nw_fuzz_at;
}

// the band's sprites, masked walls and weapon (nw_band_x0 to nw_band_x1): vanilla's R_DrawMasked
void nw_draw_masked(void) {
    for (int k = 0; k < nw_vissprite_count; k++) {
        vissprite_t *spr = &nw_vissprites[nw_vis_order[k]];
        if (spr->x2 < nw_band_x0 || spr->x1 >= nw_band_x1) continue;
        if (spr->colormap < 0) nw_fuzz_at = spr->fuzzbase;
        nw_draw_sprite(spr, spr->colormap < 0 ? spr->x1 : nw_band_x1);
    }
    for (int i = nw_drawseg_count - 1; i >= 0; i--) {
        nw_drawseg_t *ds = &nw_drawsegs[i];
        if (ds->masked == NW_OPEN_NONE || ds->x2 < nw_band_x0 || ds->x1 >= nw_band_x1) continue;
        nw_masked_seg_range(ds, ds->x1 > nw_band_x0 ? ds->x1 : nw_band_x0,
                            ds->x2 < nw_band_x1 - 1 ? ds->x2 : nw_band_x1 - 1);
    }
    nw_fuzz_at = nw_fuzz_psprites;
    if (!viewangleoffset) nw_draw_player_sprites();
}
#endif
