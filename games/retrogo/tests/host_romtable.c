/* Retro-Go's ROM table reader on a computer: reads a table file (tools/romtable.py --bin, or
 * a damaged one) from a buffer of exactly its size, so that the sanitizer stops any read past
 * it, touches every byte of every game it finds and prints them:
 *   host_romtable TABLE.bin        prints the count (-1: not a table), then "system size name" */
#include <stdio.h>
#include <stdlib.h>
#include "../src/romtable.h"

int main(int argc, char **argv) {
  if (argc < 2) return 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f) return 2;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *t = malloc(n ? (size_t)n : 1);
  if (fread(t, 1, (size_t)n, f) != (size_t)n) return 2;
  fclose(f);
  static rg_rom_t roms[RG_MAX_ROMS];
  int count = rg_romtable_read(t, (uint32_t)n, roms, RG_MAX_ROMS);
  printf("%d\n", count);
  for (int i = 0; i < count; i++) {
    unsigned sum = 0;
    for (uint32_t k = 0; k < roms[i].size; k++) sum += roms[i].data[k];
    char name[41];
    rg_rom_name(&roms[i], name, sizeof name);
    printf("%d %u %u %s\n", roms[i].system, (unsigned)roms[i].size, sum & 0xFF, name);
  }
  free(t);
  return 0;
}
