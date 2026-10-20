/* A small LZ77 packer for Retro-Go's saves.
 *
 * The packed stream: a byte under 0x80 is a run of that many plus one bytes as they are; from
 * 0x80, a match: its length minus 3 in the low 7 bits (127: the rest follows in bytes, 255
 * meaning there is more), then its distance back minus 1 in two bytes (little-endian). A match
 * can be as long as the data, so a cartridge RAM of zeros packs to a few bytes. */
#include "pack.h"
#include <string.h>

static uint32_t hash(const uint8_t *p) {
  return (((uint32_t)p[0] << 8 ^ (uint32_t)p[1] << 4 ^ p[2]) * 2654435761u >> 20) & (RG_PACK_HASH - 1);
}

uint32_t rg_pack(const uint8_t *in, uint32_t n, uint8_t *out, uint16_t *table) {
  for (int i = 0; i < RG_PACK_HASH; i++) table[i] = 0xFFFF;
  if (n > RG_PACK_MAX) n = RG_PACK_MAX;
  uint32_t o = 0, i = 0, lit = 0, lit_at = 0;
  while (i < n) {
    uint32_t len = 0, dist = 0;
    if (i + 3 <= n) {
      uint32_t h = hash(in + i), cand = table[h];
      table[h] = (uint16_t)i;
      if (cand != 0xFFFF && cand < i) {
        while (i + len < n && in[cand + len] == in[i + len]) len++;
        dist = i - cand;
      }
    }
    if (len >= 3) {
      lit = 0;
      uint32_t l = len - 3;
      if (out) out[o] = (uint8_t)(0x80 | (l < 127 ? l : 127));
      o++;
      if (l >= 127) {
        for (l -= 127; l >= 255; l -= 255) {
          if (out) out[o] = 255;
          o++;
        }
        if (out) out[o] = (uint8_t)l;
        o++;
      }
      if (out) out[o] = (uint8_t)(dist - 1), out[o + 1] = (uint8_t)((dist - 1) >> 8);
      o += 2;
      /* (the bytes inside the match go into the table too, every fourth in a long one) */
      for (uint32_t k = 1; k < len && i + k + 3 <= n; k += len > 64 ? 4 : 1) table[hash(in + i + k)] = (uint16_t)(i + k);
      i += len;
    } else {
      if (!lit || lit == 128) lit_at = o++, lit = 0;
      if (out) out[lit_at] = (uint8_t)lit, out[o] = in[i];
      o++, i++, lit++;
    }
  }
  return o;
}

bool rg_unpack(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t size) {
  uint32_t i = 0, o = 0;
  while (i < n) {
    uint32_t t = in[i++];
    if (t < 0x80) {
      uint32_t run = t + 1;
      if (run > n - i || run > size - o) return false;
      memcpy(out + o, in + i, run);
      i += run, o += run;
    } else {
      uint32_t len = (t & 127) + 3;
      if ((t & 127) == 127)
        for (;;) {
          if (i >= n || len > size) return false;
          uint32_t b = in[i++];
          len += b;
          if (b < 255) break;
        }
      if (n - i < 2) return false;
      uint32_t dist = ((uint32_t)in[i] | (uint32_t)in[i + 1] << 8) + 1;
      i += 2;
      if (dist > o || len > size - o) return false;
      for (uint32_t k = 0; k < len; k++, o++) out[o] = out[o - dist];
    }
  }
  return o == size;
}
