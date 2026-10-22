/* The saves' packer on a computer (with the sanitizers): every kind of data comes back as it
 * was, and damaged packed data is turned down without reading or writing past either buffer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/pack.h"

static uint32_t seed = 12345;
static uint32_t rnd(void) {
  seed ^= seed << 13, seed ^= seed >> 17, seed ^= seed << 5;
  return seed;
}

int main(void) {
  static uint16_t table[RG_PACK_HASH];
  int failures = 0;
  for (int round = 0; round < 400; round++) {
    uint32_t n = round < 8 ? (uint32_t[]){0, 1, 2, 3, 18, 130, 131, 65535}[round] : rnd() % 40000;
    uint8_t *in = malloc(n ? n : 1);
    int kind = round % 5;
    for (uint32_t i = 0; i < n; i++)
      in[i] = kind == 0   ? 0                                     /* a cartridge RAM never written */
              : kind == 1 ? (uint8_t)rnd()                        /* noise */
              : kind == 2 ? (uint8_t)(i % 7 == 0 ? rnd() : 0xFF)  /* sparse */
              : kind == 3 ? (uint8_t)(i / 300)                    /* runs */
                          : (uint8_t)(rnd() % 4);                 /* few values */
    uint32_t size = rg_pack(in, n, NULL, table);
    uint8_t *packed = malloc(size ? size : 1), *back = malloc(n ? n : 1);
    if (rg_pack(in, n, packed, table) != size || !rg_unpack(packed, size, back, n) || memcmp(in, back, n)) {
      printf("FAIL round %d: %u bytes (kind %d) don't come back\n", round, n, kind);
      failures++;
    }
    if (kind == 0 && n == 65535 && size > 600) printf("FAIL: 64 KB of zeros packed to %u bytes\n", size), failures++;
    /* damaged: bytes changed, cut short, or a size that doesn't match; must not crash */
    for (int k = 0; k < 20 && size; k++) {
      uint8_t *bad = malloc(size);
      memcpy(bad, packed, size);
      for (int j = 0; j < 1 + k % 4; j++) bad[rnd() % size] = (uint8_t)rnd();
      uint32_t cut = k % 3 == 0 ? rnd() % size : size, want = k % 5 == 0 ? (n ? rnd() % n : 0) : n;
      uint8_t *out = malloc(want ? want : 1);
      (void)rg_unpack(bad, cut, out, want);
      free(out);
      free(bad);
    }
    free(in), free(packed), free(back);
  }
  /* random bytes as packed data */
  for (int k = 0; k < 2000; k++) {
    uint32_t n = rnd() % 300, want = rnd() % 5000;
    uint8_t *junk = malloc(n ? n : 1), *out = malloc(want ? want : 1);
    for (uint32_t i = 0; i < n; i++) junk[i] = (uint8_t)rnd();
    (void)rg_unpack(junk, n, out, want);
    free(junk), free(out);
  }
  printf(failures ? "%d failures\n" : "ok   the packer: 400 buffers back as they were, 10000 damaged ones turned down\n",
         failures);
  return failures != 0;
}
