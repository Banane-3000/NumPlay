/* Saves: hillclimb.sav (coins, vehicles, stages, upgrades, bests), and a copy of it in a Python script:
 * installing apps from the NumWorks website keeps only Python scripts, so the save comes back from it when the
 * file itself is gone. The copy is a comment line "#>name:base64" (the way NumPlay and its big games keep theirs). */
#include <string.h>
#include "game.h"
#include "plat.h"

#define SAVE_NAME "hillclimb.sav"
#define COPY_NAME "hillclimb_saves.py"
#define MAGIC 0x31524348u   /* "HCR1" */

Save sv;

typedef struct {
  uint32_t magic, size;
  Save s;
} SaveFile;

static const char copy_head[] =
  "# Hill Climb Racing keeps a copy of your progress here, so\n"
  "# that reinstalling it doesn't erase it. If you delete this\n"
  "# file, the game writes it again: your progress stays.\n";
static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void copy_write(void) {
  uint32_t len;
  const uint8_t *d = plat_load(SAVE_NAME, &len);
  if (!d || !len) return;
  uint32_t nl = (uint32_t)strlen(SAVE_NAME), total = 1 + (sizeof copy_head - 1) + 2 + nl + 1 + (len + 2) / 3 * 4 + 2;
  plat_remove(COPY_NAME);
  uint8_t *o = plat_reserve(COPY_NAME, total), *p = o;
  if (!o) return;
  d = plat_load(SAVE_NAME, &len);   /* (making the copy may have moved the file) */
  if (!d) return;
  *p++ = 0;   /* (the script's status byte: not imported) */
  memcpy(p, copy_head, sizeof copy_head - 1), p += sizeof copy_head - 1;
  *p++ = '#', *p++ = '>';
  memcpy(p, SAVE_NAME, nl), p += nl;
  *p++ = ':';
  for (uint32_t i = 0; i < len; i += 3, p += 4) {
    /* (written out: GCC 15.2 for the calculator got a loop over the four wrong, see NumPlay's
     * launcher/src/storage.c) */
    uint32_t left = len - i, v = (uint32_t)d[i] << 16 | (left > 1 ? d[i + 1] << 8 : 0) | (left > 2 ? d[i + 2] : 0);
    p[0] = (uint8_t)b64[v >> 18 & 63];
    p[1] = (uint8_t)b64[v >> 12 & 63];
    p[2] = left > 1 ? (uint8_t)b64[v >> 6 & 63] : '=';
    p[3] = left > 2 ? (uint8_t)b64[v & 63] : '=';
  }
  *p++ = '\n';
  *p++ = 0;
  plat_reserved(COPY_NAME, (uint32_t)(p - o));
}

static int b64_value(uint8_t c) {
  for (int i = 0; i < 64; i++)
    if ((uint8_t)b64[i] == c) return i;
  return -1;
}

static const uint8_t *copy_line(uint32_t *len) {
  uint32_t n, nl = (uint32_t)strlen(SAVE_NAME);
  const uint8_t *c = plat_load(COPY_NAME, &n);
  if (!c) return NULL;
  for (uint32_t i = 0; i + nl + 3 <= n; i++)
    if (c[i] == '#' && c[i + 1] == '>' && !memcmp(c + i + 2, SAVE_NAME, nl) && c[i + 2 + nl] == ':') {
      const uint8_t *l = c + i + 3 + nl;
      uint32_t m = 0;
      while (l + m < c + n && l[m] != '\n' && l[m]) m++;
      *len = m;
      return l;
    }
  return NULL;
}

void save_restore_copy(void) {
  uint32_t have, len;
  if (plat_load(SAVE_NAME, &have)) return;
  const uint8_t *l = copy_line(&len);
  if (!l || !len || len % 4) return;
  uint32_t size = len / 4 * 3 - (l[len - 1] == '=') - (l[len - 2] == '=');
  uint8_t *out = plat_reserve(SAVE_NAME, size);
  if (!out) return;
  l = copy_line(&len);
  bool ok = l != NULL && len / 4 * 3 - (l[len - 1] == '=') - (l[len - 2] == '=') == size;
  uint32_t o = 0;
  for (uint32_t i = 0; ok && i < len; i += 4) {
    int v0 = l[i] == '=' ? 0 : b64_value(l[i]), v1 = l[i + 1] == '=' ? 0 : b64_value(l[i + 1]);
    int v2 = l[i + 2] == '=' ? 0 : b64_value(l[i + 2]), v3 = l[i + 3] == '=' ? 0 : b64_value(l[i + 3]);
    if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) ok = false;
    uint32_t wd = (uint32_t)(v0 << 18 | v1 << 12 | v2 << 6 | v3);
    if (o < size) out[o++] = (uint8_t)(wd >> 16);
    if (o < size) out[o++] = (uint8_t)(wd >> 8);
    if (o < size) out[o++] = (uint8_t)wd;
  }
  if (ok && o == size) plat_reserved(SAVE_NAME, size);
  else plat_remove(SAVE_NAME);
}

static void defaults(void) {
  memset(&sv, 0, sizeof sv);
  sv.vehicles = 1, sv.stages = 1;
  for (int v = 0; v < NVEHICLES; v++)
    for (int u = 0; u < 4; u++) sv.level[v][u] = 1;
}

void save_load(void) {
  defaults();
  uint32_t len;
  const uint8_t *d = plat_load(SAVE_NAME, &len);
  if (!d || len != sizeof(SaveFile)) return;
  SaveFile f;
  memcpy(&f, d, sizeof f);
  if (f.magic != MAGIC || f.size != sizeof(Save)) return;
  sv = f.s;
  /* (whatever the file says, the first vehicle and stage are owned and levels stay in range) */
  sv.vehicles |= 1, sv.stages |= 1;
  if (sv.vehicle >= NVEHICLES || !(sv.vehicles >> sv.vehicle & 1)) sv.vehicle = 0;
  if (sv.stage >= NSTAGES || !(sv.stages >> sv.stage & 1)) sv.stage = 0;
  for (int v = 0; v < NVEHICLES; v++)
    for (int u = 0; u < 4; u++)
      if (sv.level[v][u] < 1 || sv.level[v][u] > vehicles[v].max[u]) sv.level[v][u] = 1;
}

bool save_write(void) {
  SaveFile f;
  memset(&f, 0, sizeof f);
  f.magic = MAGIC, f.size = sizeof(Save), f.s = sv;
  bool ok = plat_save(SAVE_NAME, &f, sizeof f);
  copy_write();
  return ok;
}
