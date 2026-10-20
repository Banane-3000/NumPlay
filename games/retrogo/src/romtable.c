/* Reading Retro-Go's table of games (the layout is in tools/romtable.py). Whatever the table
 * holds, nothing is read past `limit` and no game points outside it. */
#include "romtable.h"

static uint32_t rd32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* by system, then by name as Retro-Go sorts its lists (letters before case) */
static bool before(const rg_rom_t *a, const rg_rom_t *b) {
  if (a->system != b->system) return a->system < b->system;
  for (int i = 0; i < RG_ROMS_NAME; i++) {
    int x = lower(a->name[i]), y = lower(b->name[i]);
    if (x != y) return x < y;
    if (!x) break;
  }
  return a->data < b->data;
}

int rg_romtable_read(const uint8_t *table, uint32_t limit, rg_rom_t *out, int max) {
  if (!table || limit < 12 || rd32(table) != RG_ROMS_MAGIC || rd32(table + 4) != RG_ROMS_VERSION) return -1;
  uint32_t count = rd32(table + 8);
  /* (as many entries as the readable bytes hold) */
  if (count > (limit - 12) / RG_ROMS_ENTRY) count = (limit - 12) / RG_ROMS_ENTRY;
  uint32_t first = 12 + count * RG_ROMS_ENTRY;
  int n = 0;
  for (uint32_t i = 0; i < count && n < max; i++) {
    const uint8_t *e = table + 12 + i * RG_ROMS_ENTRY;
    uint32_t at = rd32(e), size = rd32(e + 4);
    uint8_t system = e[8];
    if (at < first || (at & 3) || at > limit || !size || size > limit - at) continue;
    if (system < RG_SYS_GB || system >= RG_SYS_COUNT) continue;
    rg_rom_t r = {table + at, size, e + 12, system};
    int k = n++;
    for (; k > 0 && before(&r, &out[k - 1]); k--) out[k] = out[k - 1];
    out[k] = r;
  }
  return n;
}

void rg_rom_name(const rg_rom_t *rom, char *out, int n) {
  int o = 0;
  for (int i = 0; i < RG_ROMS_NAME && o < n - 1; i++) {
    uint8_t c = rom->name[i];
    if (!c) break;
    if (c >= 0x80) { /* one letter of UTF-8: its continuation bytes go with it */
      while (i + 1 < RG_ROMS_NAME && (rom->name[i + 1] & 0xC0) == 0x80) i++;
      c = '?';
    } else if (c < 0x20 || c == 0x7F) {
      c = ' ';
    }
    out[o++] = (char)c;
  }
  while (o > 0 && out[o - 1] == ' ') o--;
  if (!o && n > 1) out[o++] = '?';
  out[o] = 0;
}
