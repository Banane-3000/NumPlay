/* PolyTrack's data, made by tools/pack.py from Kodub's web build of PolyTrack 0.6.3: the official tracks,
 * the track parts and the car (their models simplified), the materials. Do not edit. */
#ifndef PT_DATA_H
#define PT_DATA_H
#include <stdint.h>

#define PT_FIX 256   /* vertex units a metre */
typedef struct { uint32_t vfirst; uint8_t nv, pad; uint16_t ntri; uint32_t tfirst; } pt_lod;
typedef struct { pt_lod lod[2]; int16_t box[6]; } pt_chunk;
typedef struct {
  uint16_t vbase, nv;          /* its vertices in pt_pool */
  uint16_t chunk; uint8_t nchunk;
  uint16_t col, ncol;          /* physics triangles in pt_ctri */
  uint16_t grid; uint8_t gx, gz; /* their 5 m grid, from lo */
  int16_t lo[3], hi[3];
  uint8_t simple;              /* one mesh for all: faces can be hidden per instance (pt_masks) */
  uint8_t det;                 /* 1 checkpoint, 2 finish */
  uint8_t custom;              /* takes the custom block colours */
  float dc[3], ds[3];          /* its detector box */
  float start[3];              /* a start: where the car goes */
} pt_part;
typedef struct { int16_t x, z; uint8_t part, orient, color, y, mask, pad; } pt_inst;
extern const uint8_t pt_blob[];   /* data.bin (data.S) */
typedef struct { uint16_t inst, order; } pt_spec;
typedef struct {
  const char *name;
  uint8_t env, sun;
  uint16_t inst, ninst, mask, cell;
  int16_t gx0, gz0; uint8_t gw, gh, reach;
  uint16_t spec; uint8_t ncp, nfin, norders;
  uint16_t start;
  int16_t bx0, bx1, bz0, bz1;
  uint16_t thumb; uint8_t tw, th;
} pt_track;

#define PT_NPARTS 129
#define PT_NTRACKS 17
#define PT_NMAT 21
#define PT_M_ROAD 0
#define PT_M_ROAD_EDGE_RED 1
#define PT_M_ROAD_EDGE_WHITE 2
#define PT_M_ROAD_BARRIER 3
#define PT_M_FINISH_WHITE 4
#define PT_M_FINISH_BLACK 5
#define PT_M_FINISH 6
#define PT_M_START_LINE 7
#define PT_M_CHECKPOINT 8
#define PT_M_PLANE_WALL 9
#define PT_M_PLANE_WALL_DETAIL 10
#define PT_M_BLOCK_SURFACE 11
#define PT_M_PILLAR 12
#define PT_M_PILLAR_EDGE 13
#define PT_M_WALL_TRACK 14
#define PT_M_WALL_TRACK_BOTTOM 15
#define PT_M_WALL_TRACK_SIDES 16
#define PT_M_SIGN_RED 17
#define PT_M_SIGN_BLACK 18
#define PT_M_SIGN_YELLOW 19
#define PT_M_ROAD_EDGE 20
#define PT_CM_MAIN 0
#define PT_CM_STRIPE 1
#define PT_CM_AIRINTAKE 2
#define PT_CM_METAL 3
#define PT_CM_BRAKELIGHT 4
#define PT_CM_TIRE 5
#define PT_CM_RIM 6
#define pt_pool ((const int16_t *)(pt_blob + 0))   /* 61326 */
#define pt_cvidx ((const uint16_t *)(pt_blob + 122652))   /* 24335 */
#define pt_rtri ((const uint8_t *)(pt_blob + 171324))   /* 288939 */
extern const pt_chunk pt_chunks[216];
#define pt_ctri ((const uint16_t *)(pt_blob + 460264))   /* 93834 */
#define pt_cgrid ((const uint32_t *)(pt_blob + 647932))   /* 3762 */
#define pt_cgidx ((const uint16_t *)(pt_blob + 662980))   /* 75289 */
extern const pt_part pt_parts[PT_NPARTS];
extern const uint8_t pt_part_id[129];
extern const int8_t pt_orient[216];
#define pt_insts ((const pt_inst *)(pt_blob + 813560))   /* 35078 */
#define pt_masks ((const uint32_t *)(pt_blob + 1164340))   /* 1425 */
#define pt_tcells ((const uint16_t *)(pt_blob + 1170040))   /* 5771 */
#define pt_specs ((const pt_spec *)(pt_blob + 1181584))   /* 87 */
#define pt_thumbs ((const uint8_t *)(pt_blob + 1181932))   /* 10755 */
extern const pt_track pt_tracks[PT_NTRACKS];
extern const uint16_t pt_envmat[189];
extern const uint16_t pt_custom[27];
extern const uint16_t pt_carmat[15];
extern const int16_t pt_body_v[294];
extern const uint8_t pt_body_t[668];
#define PT_BODY_NV 98
#define PT_BODY_NT 167
extern const int16_t pt_wheel_v[126];
extern const uint8_t pt_wheel_t[320];
#define PT_WHEEL_NV 42
#define PT_WHEEL_NT 80
extern const int16_t pt_extra_v[252];
extern const uint8_t pt_extra_t[528];
#define PT_EXTRA_NV 84
#define PT_EXTRA_NT 132
extern const float pt_hull[12][3];
#define PT_HULL_N 12
extern const float pt_mountain[128];
#define PT_MOUNTAIN_N 128
#endif
