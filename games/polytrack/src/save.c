/* The save (save.h): checked field by field; anything damaged, cut short or from elsewhere is not read: the copy in
 * the Python script is tried, then a fresh start. */
#include <stddef.h>
#include <string.h>
#include "plat.h"
#include "save.h"

void pg_fresh(pg_profile *p) {
  memset(p, 0, sizeof *p);
  p->laps = 3, p->rivals = 5, p->level = LEVEL_NORMAL;
}

static bool profile_ok(const pg_profile *p) {
  if (p->races > 999999 || p->wins > p->races || p->runs > 999999) return false;
  for (int i = 0; i < PG_CIRCUITS; i++) {
    if (p->best_ms[i] > PG_MAX_TIME || p->best_lap_ms[i] > PG_MAX_TIME) return false;
    if (p->best_ms[i] && (!p->best_lap_ms[i] || p->best_lap_ms[i] > p->best_ms[i])) return false;
  }
  if (p->best_m > PG_MAX_M || p->best_zone > 9999 || (p->best_zone && !p->best_m)) return false;
  return p->mode <= MODE_ENDLESS && p->track < PG_CIRCUITS && p->vehicle < PG_VEHICLES && p->laps >= 1 &&
         p->laps <= PG_MAX_LAPS && p->rivals <= PG_CARS - 1 && p->level <= LEVEL_HARD && p->manual <= 1 &&
         p->view < VIEWS && p->guide <= 1 && !p->spare[0] && !p->spare[1] && !p->spare[2];
}

bool pg_file_ok(const pg_file *f) {
  if (f->magic != PG_MAGIC || f->version != PG_VERSION || f->size != sizeof *f) return false;
  if (f->sum != pg_sum(f, offsetof(pg_file, sum))) return false;
  if (f->spare[0] || f->spare[1] || f->spare[2] || f->has_race > 1 || !profile_ok(&f->p)) return false;
  if (!f->has_race) {
    const uint8_t *b = (const uint8_t *)&f->race;
    for (unsigned i = 0; i < sizeof f->race; i++)
      if (b[i]) return false;
    return true;
  }
  return pg_valid(&f->race, NULL);
}

static const char head[] = "# PolyGT 3D keeps a copy of your progress here,\n"
                           "# so that installing it again doesn't erase it.\n";
static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static uint8_t copy[1 + sizeof head + 2 + sizeof PG_SAVE_NAME + 1 + (sizeof(pg_file) + 2) / 3 * 4 + 2];

/* the copy: the file in base 64, in a comment line of a script */
static bool copy_write(const pg_file *f) {
  const uint8_t *bytes = (const uint8_t *)f;
  uint32_t len = sizeof *f, nl = sizeof PG_SAVE_NAME - 1;
  uint8_t *p = copy;
  *p++ = 0; /* (a script's first byte: not imported) */
  memcpy(p, head, sizeof head - 1), p += sizeof head - 1;
  *p++ = '#', *p++ = '>';
  memcpy(p, PG_SAVE_NAME, nl), p += nl;
  *p++ = ':';
  for (uint32_t i = 0; i < len; i += 3, p += 4) {
    uint32_t left = len - i;
    uint32_t v = (uint32_t)bytes[i] << 16 | (left > 1 ? (uint32_t)bytes[i + 1] << 8 : 0) | (left > 2 ? bytes[i + 2] : 0);
    p[0] = (uint8_t)b64[v >> 18 & 63];
    p[1] = (uint8_t)b64[v >> 12 & 63];
    p[2] = left > 1 ? (uint8_t)b64[v >> 6 & 63] : '=';
    p[3] = left > 2 ? (uint8_t)b64[v & 63] : '=';
  }
  *p++ = '\n';
  return plat_save(PG_COPY_NAME, copy, (uint32_t)(p - copy));
}

static int b64v(uint8_t c) {
  for (int i = 0; i < 64; i++)
    if ((uint8_t)b64[i] == c) return i;
  return -1;
}

static bool copy_read(pg_file *f) {
  uint32_t n, nl = sizeof PG_SAVE_NAME - 1;
  const uint8_t *c = plat_load(PG_COPY_NAME, &n);
  if (!c) return false;
  for (uint32_t i = 0; i + nl + 3 <= n; i++) {
    if (c[i] != '#' || c[i + 1] != '>' || memcmp(c + i + 2, PG_SAVE_NAME, nl) || c[i + 2 + nl] != ':') continue;
    const uint8_t *l = c + i + 3 + nl;
    uint32_t m = 0;
    while (l + m < c + n && l[m] != '\n' && l[m]) m++;
    if (m != (sizeof *f + 2) / 3 * 4) return false;
    uint8_t *bytes = (uint8_t *)f;
    uint32_t o = 0;
    for (uint32_t k = 0; k < m; k += 4) {
      int v0 = b64v(l[k]), v1 = b64v(l[k + 1]), v2 = l[k + 2] == '=' ? 0 : b64v(l[k + 2]),
          v3 = l[k + 3] == '=' ? 0 : b64v(l[k + 3]);
      if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) return false;
      uint32_t w = (uint32_t)(v0 << 18 | v1 << 12 | v2 << 6 | v3);
      if (o < sizeof *f) bytes[o++] = (uint8_t)(w >> 16);
      if (o < sizeof *f) bytes[o++] = (uint8_t)(w >> 8);
      if (o < sizeof *f) bytes[o++] = (uint8_t)w;
    }
    return true;
  }
  return false;
}

bool save_load(pg_file *f) {
  uint32_t n;
  const uint8_t *d = plat_load(PG_SAVE_NAME, &n);
  bool ok = false;
  if (d && n == sizeof *f) {
    memcpy(f, d, sizeof *f);
    ok = pg_file_ok(f);
  }
  if (!ok && copy_read(f) && pg_file_ok(f)) {
    ok = true;
    plat_save(PG_SAVE_NAME, f, sizeof *f);   /* back where it belongs */
  }
  if (!ok) {
    memset(f, 0, sizeof *f);
    pg_fresh(&f->p);
  }
  return ok;
}

static pg_file out;   /* (static: the stack stays small) */
bool save_write(const pg_file *f) {
  out = *f;
  out.magic = PG_MAGIC, out.version = PG_VERSION, out.size = sizeof out;
  memset(out.spare, 0, sizeof out.spare);
  if (!out.has_race) memset(&out.race, 0, sizeof out.race);   /* (a race over is not kept) */
  out.sum = pg_sum(&out, offsetof(pg_file, sum));
  bool ok = plat_save(PG_SAVE_NAME, &out, sizeof out);
  copy_write(&out);
  return ok;
}
