/* NumBlocks.nwa only (NB_DIMS): the generators of the Nether (ChunkProviderHell, MapGenCavesHell) and the
 * End (ChunkProviderEnd), included at the end of gen.c to share its java.util.Random, its noise and its
 * memory: while the player is in one of them, the Overworld's caches (the summary cache on main's stack,
 * the scratch union U) are theirs, and gen_init makes them the Overworld's again.
 *
 * As in gen.c, the noise's sample positions are exact (fixed point) and its sums run in float; the
 * terrain, its surface and the caves follow 1.8.8 block for block. Minecraft 1.8.8 decorates the Nether
 * and the End with a random that is not the world's (it depends on the order chunks were made in): here
 * each decoration draws from a random seeded by its chunk, so it is the same every time. */
#include "dims.h"

/* gd_dim (gen.c): the generator gen_slab runs, DIM_* */

/* ---------------------------------------------------------------- noise: NoiseGeneratorOctaves, any size */
/* n octaves of a NoiseGeneratorOctaves (their states st) at grid origin (bx, by, bz), xs x ys x zs samples
 * (ys 1: the 2D noise) spaced sx, sy, sz (Q32), added into out[(x * zs + z) * ys + y] */
static void gd_octaves(const Octave *oc, int n, float *out, i64 bx, i64 by, i64 bz, int xs, int ys, int zs, u64 sx,
                       u64 sy, u64 sz) {
    uint8_t P[256];
    int xc[16], zc[16], yc[17];
    float xf[16], zf[16], yf[17];
    for (int o = 0; o < n; o++) {
        i64 off[3];
        perm_build(oc[o].st, P, off);
        float amp = (float)(1 << o);
        for (int k = 0; k < xs; k++) coord_split(coord_q32(bx, sx, o, k, off[0]), &xc[k], &xf[k]);
        for (int k = 0; k < zs; k++) coord_split(coord_q32(bz, sz, o, k, off[2]), &zc[k], &zf[k]);
        if (ys == 1) {
            /* NoiseGeneratorImproved's 2D path (no y) */
            int idx = 0;
            for (int x = 0; x < xs; x++) {
                float fx = xf[x], ux = fade(fx);
                int i3 = xc[x];
                for (int z = 0; z < zs; z++, idx++) {
                    float fz = zf[z], uz = fade(fz);
                    int l1 = zc[z];
                    int j3 = P[P[i3] & 255] + l1, i1 = P[P[(i3 + 1) & 255] & 255] + l1;
                    float a = lerp(ux, grad3(P[j3 & 255], fx, 0, fz), grad3(P[i1 & 255], fx - 1, 0, fz));
                    float b = lerp(ux, grad3(P[(j3 + 1) & 255], fx, 0, fz - 1), grad3(P[(i1 + 1) & 255], fx - 1, 0, fz - 1));
                    out[idx] += lerp(uz, a, b) * amp;
                }
            }
            continue;
        }
        for (int k = 0; k < ys; k++) coord_split(coord_q32(by, sy, o, k, off[1]), &yc[k], &yf[k]);
        int idx = 0;
        for (int x = 0; x < xs; x++) {
            float fx = xf[x], ux = fade(fx), gx1 = fx - 1;
            int pa = P[xc[x]], pb = P[(xc[x] + 1) & 255];
            for (int z = 0; z < zs; z++) {
                float fz = zf[z], uz = fade(fz), gz1 = fz - 1;
                int j4 = zc[z], cur = -1;
                float d16 = 0, d7 = 0, d17 = 0, d8 = 0;
                for (int y = 0; y < ys; y++, idx++) {
                    /* Java's stale gradients: recomputed only when the y cell changes (and at a column's start),
                     * with the y fraction of the first sample in the cell */
                    if (y == 0 || yc[y] != cur) {
                        cur = yc[y];
                        float fy = yf[y], gy1 = fy - 1;
                        int j5 = pa + cur, i6 = pb + cur;
                        int k5 = P[j5 & 255] + j4, l5 = P[(j5 + 1) & 255] + j4;
                        int i2 = P[i6 & 255] + j4, j6 = P[(i6 + 1) & 255] + j4;
                        d16 = lerp(ux, grad3(P[k5 & 255], fx, fy, fz), grad3(P[i2 & 255], gx1, fy, fz));
                        d7 = lerp(ux, grad3(P[l5 & 255], fx, gy1, fz), grad3(P[j6 & 255], gx1, gy1, fz));
                        d17 = lerp(ux, grad3(P[(k5 + 1) & 255], fx, fy, gz1), grad3(P[(i2 + 1) & 255], gx1, fy, gz1));
                        d8 = lerp(ux, grad3(P[(l5 + 1) & 255], fx, gy1, gz1), grad3(P[(j6 + 1) & 255], gx1, gy1, gz1));
                    }
                    float uy = fade(yf[y]);
                    out[idx] += lerp(uz, lerp(uy, d16, d7), lerp(uy, d17, d8)) * amp;
                }
            }
        }
    }
}

/* ======================================================================== */
/* The Nether: ChunkProviderHell                                             */
/* ======================================================================== */

#define NG 6   /* density grids kept */
typedef struct {
    Octave oc[48];   /* netherNoiseGen1 (16), netherNoiseGen2 (16), netherNoiseGen3 (8), slowsandGravel (4),
                        netherrackExculsivity (4) */
    float bias[17];  /* the density's offset by height (initializeNoiseField's adouble) */
    int gx[NG], gz[NG];
    uint32_t gage[NG], gclock;
    uint8_t gok[NG];
    float grid[NG][425];   /* (x * 5 + z) * 17 + y */
} NetherMem;
_Static_assert(sizeof(NetherMem) <= GEN_CACHE_BYTES, "the Nether's state fits in the summary cache");
#define NM ((NetherMem *)(void *)sumc)
static u64 SC_N1XZ, SC_N1Y, SC_N3XZ, SC_N3Y, SC_SLOW, SC_EXCL;

static void nether_init(void) {
    NetherMem *m = NM;
    memset(m, 0, sizeof *m);
    JRand r;
    jr_seed(&r, g_seed);
    for (int i = 0; i < 48; i++) m->oc[i].st = r.s, perm_skip(&r);
    SC_N1XZ = FX(684.412);
    SC_N1Y = FX(2053.236);
    SC_N3XZ = FX(684.412 / 80.0);
    SC_N3Y = FX(2053.236 / 60.0);
    SC_SLOW = FX(0.03125);
    SC_EXCL = FX(0.0625);
    for (int j = 0; j < 17; j++) {
        double a = cos((double)j * 3.141592653589793 * 6.0 / 17.0) * 2.0, d2 = j > 8 ? 16 - j : j;
        if (d2 < 4.0) {
            d2 = 4.0 - d2;
            a -= d2 * d2 * d2 * 10.0;
        }
        m->bias[j] = (float)a;
    }
}

/* initializeNoiseField: chunk (cx, cz)'s 5 x 17 x 5 densities */
static const float *nether_grid(int cx, int cz) {
    NetherMem *m = NM;
    int k = 0;
    for (int i = 0; i < NG; i++) {
        if (m->gok[i] && m->gx[i] == cx && m->gz[i] == cz) {
            m->gage[i] = ++m->gclock;
            return m->grid[i];
        }
        if (!m->gok[i] || m->gage[i] < m->gage[k]) k = m->gok[k] ? i : k;
    }
    for (int i = 0; i < NG; i++)
        if (!m->gok[i]) {
            k = i;
            break;
        }
    float n1[425], n2[425], n3[425];
    memset(n1, 0, sizeof n1);
    memset(n2, 0, sizeof n2);
    memset(n3, 0, sizeof n3);
    gd_octaves(m->oc + 32, 8, n3, cx * 4, 0, cz * 4, 5, 17, 5, SC_N3XZ, SC_N3Y, SC_N3XZ);
    gd_octaves(m->oc, 16, n1, cx * 4, 0, cz * 4, 5, 17, 5, SC_N1XZ, SC_N1Y, SC_N1XZ);
    gd_octaves(m->oc + 16, 16, n2, cx * 4, 0, cz * 4, 5, 17, 5, SC_N1XZ, SC_N1Y, SC_N1XZ);
    float *g = m->grid[k];
    for (int i = 0; i < 425; i++) {
        int y = i % 17;
        float d6 = n1[i] / 512.0f, d7 = n2[i] / 512.0f, d8 = (n3[i] / 10.0f + 1.0f) / 2.0f, d4;
        if (d8 < 0.0f) d4 = d6;
        else if (d8 > 1.0f) d4 = d7;
        else d4 = d6 + (d7 - d6) * d8;
        d4 -= m->bias[y];
        if (y > 13) {
            float d9 = (float)(y - 13) / 3.0f;
            d4 = d4 * (1.0f - d9) + -10.0f * d9;
        }
        g[i] = d4;
    }
    m->gx[k] = cx, m->gz[k] = cz, m->gok[k] = 1, m->gage[k] = ++m->gclock;
    return g;
}

/* func_180515_a: the terrain of a column (netherrack where the density is above 0, lava under y 32),
 * into col[0..127] */
static void nether_column(const float *g, int x, int z, uint8_t *col) {
    int j1 = x >> 2, k1 = z >> 2, j2 = x & 3, k2 = z & 3;
    for (int l1 = 0; l1 < 16; l1++) {
        /* the grid's corners, interpolated as Java does: along y in steps of 1/8, x and z in steps of 1/4 */
        float d1 = g[((j1 + 0) * 5 + k1 + 0) * 17 + l1], d2 = g[((j1 + 0) * 5 + k1 + 1) * 17 + l1];
        float d3 = g[((j1 + 1) * 5 + k1 + 0) * 17 + l1], d4 = g[((j1 + 1) * 5 + k1 + 1) * 17 + l1];
        float d5 = (g[((j1 + 0) * 5 + k1 + 0) * 17 + l1 + 1] - d1) * 0.125f;
        float d6 = (g[((j1 + 0) * 5 + k1 + 1) * 17 + l1 + 1] - d2) * 0.125f;
        float d7 = (g[((j1 + 1) * 5 + k1 + 0) * 17 + l1 + 1] - d3) * 0.125f;
        float d8 = (g[((j1 + 1) * 5 + k1 + 1) * 17 + l1 + 1] - d4) * 0.125f;
        for (int i2 = 0; i2 < 8; i2++) {
            /* (the steps added j2 times, k2 times, as Java's loops add them) */
            float d10 = d1, d11 = d2, d12 = (d3 - d1) * 0.25f, d13 = (d4 - d2) * 0.25f;
            for (int a = 0; a < j2; a++) d10 += d12, d11 += d13;
            float d15 = d10, d16 = (d11 - d10) * 0.25f;
            for (int b = 0; b < k2; b++) d15 += d16;
            int y = l1 * 8 + i2;
            col[y] = d15 > 0.0f ? GEN_CODE(GC_NETHERRACK) : y < 32 ? B_LAVA : B_AIR;
            d1 += d5, d2 += d6, d3 += d7, d4 += d8;
        }
    }
}

/* a column's surface (func_180516_b), its draws from r (the chunk's hellRNG) */
static void nether_surface(uint8_t *col, JRand *r, float slow, float grav, float excl) {
    int flag = slow + jr_doublef(r) * 0.2f > 0.0f;
    int flag1 = grav + jr_doublef(r) * 0.2f > 0.0f;
    int l = (int)(excl / 3.0f + 3.0f + jr_doublef(r) * 0.25f);
    int i1 = -1;
    const int top = 64;
    uint8_t s = GEN_CODE(GC_NETHERRACK), s1 = GEN_CODE(GC_NETHERRACK);
    for (int j1 = 127; j1 >= 0; --j1) {
        int a = j1 < 127 - jr_int(r, 5);
        if (a && j1 > jr_int(r, 5)) {
            uint8_t b = col[j1];
            if (b != B_AIR) {
                if (b == GEN_CODE(GC_NETHERRACK)) {
                    if (i1 == -1) {
                        if (l <= 0) {
                            s = B_AIR;
                            s1 = GEN_CODE(GC_NETHERRACK);
                        } else if (j1 >= top - 4 && j1 <= top + 1) {
                            s = GEN_CODE(GC_NETHERRACK);
                            s1 = GEN_CODE(GC_NETHERRACK);
                            if (flag1) s = B_GRAVEL, s1 = GEN_CODE(GC_NETHERRACK);
                            if (flag) s = GEN_CODE(GC_SOUL_SAND), s1 = GEN_CODE(GC_SOUL_SAND);
                        }
                        if (j1 < top && s == B_AIR) s = B_LAVA;
                        i1 = l;
                        col[j1] = j1 >= top - 1 ? s : s1;
                    } else if (i1 > 0) {
                        --i1;
                        col[j1] = s1;
                    }
                }
            } else {
                i1 = -1;
            }
        } else {
            col[j1] = B_BEDROCK;
        }
    }
}

/* ---- MapGenCavesHell: tunnels carved where the box they cut holds no lava */
typedef struct {
    int cx, cz;            /* the chunk carved */
    const uint32_t *lava;  /* its lava, a bit a block: [(x * 16 + z) * 4 + (y >> 5)] bit y & 31 */
    uint8_t *out;          /* the slab carved */
    int y0, h;
} HellCaves;

static int hc_lava(const HellCaves *c, int x, int y, int z) { return c->lava[(x * 16 + z) * 4 + (y >> 5)] >> (y & 31) & 1; }

static void hell_tunnel(HellCaves *c, i64 seed, double px, double py, double pz, float w, float yaw, float pitch, int k,
                        int len, double vy) {
    double d0 = (double)(c->cx * 16 + 8), d1 = (double)(c->cz * 16 + 8);
    float f = 0.0f, f1 = 0.0f;
    JRand rnd;
    jr_seed(&rnd, seed);
    if (len <= 0) {
        int i = 8 * 16 - 16;
        len = i - jr_int(&rnd, i / 4);
    }
    int flag1 = 0;
    if (k == -1) {
        k = len / 2;
        flag1 = 1;
    }
    int j = jr_int(&rnd, len / 2) + len / 4;
    for (int flag = jr_int(&rnd, 6) == 0; k < len; ++k) {
        double d2 = 1.5 + (double)(mh_sin((float)k * 3.1415927f / (float)len) * w * 1.0f);
        double d3 = d2 * vy;
        float f2 = mh_cos(pitch), f3 = mh_sin(pitch);
        px += (double)(mh_cos(yaw) * f2);
        py += (double)f3;
        pz += (double)(mh_sin(yaw) * f2);
        pitch = flag ? pitch * 0.92f : pitch * 0.7f;
        pitch += f1 * 0.1f;
        yaw += f * 0.1f;
        f1 *= 0.9f;
        f *= 0.75f;
        /* (Java's order of the draws: left to right) */
        float ra = jr_float(&rnd), rb = jr_float(&rnd), rc = jr_float(&rnd);
        f1 += (ra - rb) * rc * 2.0f;
        ra = jr_float(&rnd), rb = jr_float(&rnd), rc = jr_float(&rnd);
        f += (ra - rb) * rc * 4.0f;
        if (!flag1 && k == j && w > 1.0f) {
            i64 s1 = jr_long(&rnd);
            float a = jr_float(&rnd) * 0.5f + 0.5f;
            hell_tunnel(c, s1, px, py, pz, a, yaw - 1.5707964f, pitch / 3.0f, k, len, 1.0);
            i64 s2 = jr_long(&rnd);
            float b2 = jr_float(&rnd) * 0.5f + 0.5f;
            hell_tunnel(c, s2, px, py, pz, b2, yaw + 1.5707964f, pitch / 3.0f, k, len, 1.0);
            return;
        }
        if (flag1 || jr_int(&rnd, 4) != 0) {
            double d4 = px - d0, d5 = pz - d1, d6 = (double)(len - k), d7 = (double)(w + 2.0f + 16.0f);
            if (d4 * d4 + d5 * d5 - d6 * d6 > d7 * d7) return;
            if (px >= d0 - 16.0 - d2 * 2.0 && pz >= d1 - 16.0 - d2 * 2.0 && px <= d0 + 16.0 + d2 * 2.0 &&
                pz <= d1 + 16.0 + d2 * 2.0) {
                int k2 = (int)floor(px - d2) - c->cx * 16 - 1, kk = (int)floor(px + d2) - c->cx * 16 + 1;
                int l2 = (int)floor(py - d3) - 1, l = (int)floor(py + d3) + 1;
                int i3 = (int)floor(pz - d2) - c->cz * 16 - 1, i1 = (int)floor(pz + d2) - c->cz * 16 + 1;
                if (k2 < 0) k2 = 0;
                if (kk > 16) kk = 16;
                if (l2 < 1) l2 = 1;
                if (l > 120) l = 120;
                if (i3 < 0) i3 = 0;
                if (i1 > 16) i1 = 16;
                int flag2 = 0;
                for (int j1 = k2; !flag2 && j1 < kk; ++j1)
                    for (int k1 = i3; !flag2 && k1 < i1; ++k1)
                        for (int l1 = l + 1; !flag2 && l1 >= l2 - 1; --l1)
                            if (l1 >= 0 && l1 < 128) {
                                if (hc_lava(c, j1, l1, k1)) flag2 = 1;
                                if (l1 != l2 - 1 && j1 != k2 && j1 != kk - 1 && k1 != i3 && k1 != i1 - 1) l1 = l2;
                            }
                if (!flag2) {
                    for (int j3 = k2; j3 < kk; ++j3) {
                        double d10 = ((double)(j3 + c->cx * 16) + 0.5 - px) / d2;
                        for (int i2 = i3; i2 < i1; ++i2) {
                            double d8 = ((double)(i2 + c->cz * 16) + 0.5 - pz) / d2;
                            for (int j2 = l; j2 > l2; --j2) {
                                double d9 = ((double)(j2 - 1) + 0.5 - py) / d3;
                                if (d9 > -0.7 && d10 * d10 + d9 * d9 + d8 * d8 < 1.0) {
                                    int y = j2 - c->y0;
                                    if (y < 0 || y >= c->h) continue;
                                    uint8_t *b = &c->out[(y * 16 + i2) * 16 + j3];
                                    if (*b == GEN_CODE(GC_NETHERRACK) || *b == B_DIRT || *b == B_GRASS) *b = B_AIR;
                                }
                            }
                        }
                    }
                    if (flag1) break;
                }
            }
        }
    }
}

static void hell_caves(HellCaves *c) {
    JRand r;
    jr_seed(&r, g_seed);
    i64 j = jr_long(&r), k = jr_long(&r);
    for (int l = c->cx - 8; l <= c->cx + 8; ++l)
        for (int i1 = c->cz - 8; i1 <= c->cz + 8; ++i1) {
            jr_seed(&r, ((i64)l * j) ^ ((i64)i1 * k) ^ g_seed);
            /* recursiveGenerate */
            int i = jr_int(&r, jr_int(&r, jr_int(&r, 10) + 1) + 1);
            if (jr_int(&r, 5) != 0) i = 0;
            for (int n = 0; n < i; ++n) {
                double d0 = (double)(l * 16 + jr_int(&r, 16)), d1 = (double)jr_int(&r, 128),
                       d2 = (double)(i1 * 16 + jr_int(&r, 16));
                int kk = 1;
                if (jr_int(&r, 4) == 0) {
                    i64 s = jr_long(&r);
                    /* func_180705_a: a room */
                    float w = 1.0f + jr_float(&r) * 6.0f;
                    hell_tunnel(c, s, d0, d1, d2, w, 0.0f, 0.0f, -1, -1, 0.5);
                    kk += jr_int(&r, 4);
                }
                for (int m = 0; m < kk; ++m) {
                    float f = jr_float(&r) * 3.1415927f * 2.0f;
                    float f1 = (jr_float(&r) - 0.5f) * 2.0f / 8.0f;
                    float f2 = jr_float(&r) * 2.0f + jr_float(&r);
                    hell_tunnel(c, jr_long(&r), d0, d1, d2, f2 * 2.0f, f, f1, 0, 0, 0.5);
                }
            }
        }
}

static void nether_decorate(int cx, int cz, int y0, int h, uint8_t *out);   /* (below: fortresses, glowstone...) */

/* chunk (cx, cz), rows [y0, y0 + h): terrain, surface, caves, then what decorates it */
static void nether_slab(int cx, int cz, int y0, int h, uint8_t *out) {
    const float *g = nether_grid(cx, cz);
    NetherMem *m = NM;
    float slow[256], grav[256], excl[256];
    memset(slow, 0, sizeof slow);
    memset(grav, 0, sizeof grav);
    memset(excl, 0, sizeof excl);
    gd_octaves(m->oc + 40, 4, slow, cx * 16, cz * 16, 0, 16, 16, 1, SC_SLOW, SC_SLOW, FX(1.0));
    gd_octaves(m->oc + 40, 4, grav, cx * 16, 109, cz * 16, 16, 1, 16, SC_SLOW, FX(1.0), SC_SLOW);
    gd_octaves(m->oc + 44, 4, excl, cx * 16, cz * 16, 0, 16, 16, 1, SC_EXCL, SC_EXCL, SC_EXCL);
    JRand r;
    jr_seed(&r, (i64)cx * 341873128712LL + (i64)cz * 132897987541LL);
    uint32_t lava[16 * 16 * 4];
    for (int z = 0; z < 16; z++)
        for (int x = 0; x < 16; x++) {
            uint8_t col[128];
            nether_column(g, x, z, col);
            nether_surface(col, &r, slow[z + x * 16], grav[z + x * 16], excl[z + x * 16]);
            uint32_t *lv = &lava[(x * 16 + z) * 4];
            lv[0] = lv[1] = lv[2] = lv[3] = 0;
            for (int y = 0; y < 128; y++)
                if (col[y] == B_LAVA) lv[y >> 5] |= 1u << (y & 31);
            for (int y = 0; y < h; y++) out[(y * 16 + z) * 16 + x] = col[y0 + y];
        }
    HellCaves c = {cx, cz, lava, out, y0, h};
    hell_caves(&c);
    nether_decorate(cx, cz, y0, h, out);
}

static void nether_decorate(int cx, int cz, int y0, int h, uint8_t *out) { (void)cx, (void)cz, (void)y0, (void)h, (void)out; }

/* ======================================================================== */
/* The End: ChunkProviderEnd                                                 */
/* ======================================================================== */

typedef struct {
    Octave oc[40];   /* noiseGen1 (16), noiseGen2 (16), noiseGen3 (8) */
} EndMem;
_Static_assert(sizeof(EndMem) <= GEN_CACHE_BYTES, "the End's state fits in the summary cache");
#define EM ((EndMem *)(void *)sumc)
static u64 SC_E1XZ, SC_E3XZ, SC_E3Y;

static void end_init(void) {
    EndMem *m = EM;
    memset(m, 0, sizeof *m);
    JRand r;
    jr_seed(&r, g_seed);
    for (int i = 0; i < 40; i++) m->oc[i].st = r.s, perm_skip(&r);
    SC_E1XZ = FX(684.412 * 2.0);
    SC_E3XZ = FX(684.412 * 2.0 / 80.0);
    SC_E3Y = FX(684.412 / 160.0);
}

/* initializeNoiseField: chunk (cx, cz)'s 3 x 33 x 3 densities (cells of 8 blocks across, 4 up), with the
 * island's falloff from (0, 0) */
static void end_grid(int cx, int cz, float *g) {
    EndMem *m = EM;
    float n1[297], n2[297], n3[297];
    memset(n1, 0, sizeof n1);
    memset(n2, 0, sizeof n2);
    memset(n3, 0, sizeof n3);
    gd_octaves(m->oc + 32, 8, n3, cx * 2, 0, cz * 2, 3, 33, 3, SC_E3XZ, SC_E3Y, SC_E3XZ);
    gd_octaves(m->oc, 16, n1, cx * 2, 0, cz * 2, 3, 33, 3, SC_E1XZ, FX(684.412), SC_E1XZ);
    gd_octaves(m->oc + 16, 16, n2, cx * 2, 0, cz * 2, 3, 33, 3, SC_E1XZ, FX(684.412), SC_E1XZ);
    int i = 0;
    for (int j = 0; j < 3; j++)
        for (int k = 0; k < 3; k++) {
            float f = (float)(j + cx * 2), f1 = (float)(k + cz * 2);
            float f2 = 100.0f - sqrtf(f * f + f1 * f1) * 8.0f;
            if (f2 > 80.0f) f2 = 80.0f;
            if (f2 < -100.0f) f2 = -100.0f;
            for (int l = 0; l < 33; l++, i++) {
                float d3 = n1[i] / 512.0f, d4 = n2[i] / 512.0f, d5 = (n3[i] / 10.0f + 1.0f) / 2.0f, d2;
                if (d5 < 0.0f) d2 = d3;
                else if (d5 > 1.0f) d2 = d4;
                else d2 = d3 + (d4 - d3) * d5;
                d2 -= 8.0f;
                d2 += f2;
                if (l > 33 / 2 - 2) {
                    float d6 = (float)(l - (33 / 2 - 2)) / 64.0f;
                    d6 = d6 < 0 ? 0 : d6 > 1 ? 1 : d6;
                    d2 = d2 * (1.0f - d6) + -3000.0f * d6;
                }
                if (l < 8) {
                    float d7 = (float)(8 - l) / 7.0f;
                    d2 = d2 * (1.0f - d7) + -30.0f * d7;
                }
                g[i] = d2;
            }
        }
}

static void end_decorate(int cx, int cz, int y0, int h, uint8_t *out);

/* func_180520_a: end stone where the density is above 0 (the surface pass changes nothing there) */
static void end_slab(int cx, int cz, int y0, int h, uint8_t *out) {
    float g[297];
    if (cx * cx + cz * cz > 30 * 30) {   /* (far out, the falloff leaves the void: no noise needed) */
        memset(out, B_AIR, (size_t)h * 256);
        return;
    }
    end_grid(cx, cz, g);
    for (int j1 = 0; j1 < 2; j1++)
        for (int k1 = 0; k1 < 2; k1++)
            for (int l1 = 0; l1 < 32; l1++) {
                float d1 = g[((j1 + 0) * 3 + k1 + 0) * 33 + l1], d2 = g[((j1 + 0) * 3 + k1 + 1) * 33 + l1];
                float d3 = g[((j1 + 1) * 3 + k1 + 0) * 33 + l1], d4 = g[((j1 + 1) * 3 + k1 + 1) * 33 + l1];
                float d5 = (g[((j1 + 0) * 3 + k1 + 0) * 33 + l1 + 1] - d1) * 0.25f;
                float d6 = (g[((j1 + 0) * 3 + k1 + 1) * 33 + l1 + 1] - d2) * 0.25f;
                float d7 = (g[((j1 + 1) * 3 + k1 + 0) * 33 + l1 + 1] - d3) * 0.25f;
                float d8 = (g[((j1 + 1) * 3 + k1 + 1) * 33 + l1 + 1] - d4) * 0.25f;
                for (int i2 = 0; i2 < 4; i2++) {
                    float d10 = d1, d11 = d2, d12 = (d3 - d1) * 0.125f, d13 = (d4 - d2) * 0.125f;
                    int y = l1 * 4 + i2 - y0;
                    for (int j2 = 0; j2 < 8; j2++) {
                        float d15 = d10, d16 = (d11 - d10) * 0.125f;
                        for (int k2 = 0; k2 < 8; k2++) {
                            if (y >= 0 && y < h)
                                out[(y * 16 + k2 + k1 * 8) * 16 + j2 + j1 * 8] =
                                    d15 > 0.0f ? GEN_CODE(GC_END_STONE) : B_AIR;
                            d15 += d16;
                        }
                        d10 += d12, d11 += d13;
                    }
                    d1 += d5, d2 += d6, d3 += d7, d4 += d8;
                }
            }
    end_decorate(cx, cz, y0, h, out);
}

static void end_decorate(int cx, int cz, int y0, int h, uint8_t *out) { (void)cx, (void)cz, (void)y0, (void)h, (void)out; }

/* ======================================================================== */
/* which generator runs                                                      */
/* ======================================================================== */

void gen_dim(int d) {
    gd_dim = d;
    if (d == DIM_NETHER) nether_init();
    else if (d == DIM_END) end_init();
}

static int dims_slab(int cx, int cz, int y0, int h, uint8_t *out) {
    if (gd_dim == DIM_NETHER) nether_slab(cx, cz, y0, h, out);
    else if (gd_dim == DIM_END) end_slab(cx, cz, y0, h, out);
    else return 0;
    return 1;
}
