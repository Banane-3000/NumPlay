/* Retro-Go's emulators on a computer: runs a ROM for some frames, with random keys if asked,
 * and writes the last frame as a PPM picture.
 *   host_core ROM FRAMES OUT.ppm [SEED]      (SEED: random pad presses; 0 for none)
 * The system comes from the file's extension (.gb .gbc .nes). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "../src/rg.h"
#include "../src/gb/gb.h"
#include "../src/nes/nes_core.h"

static uint8_t heap[RG_HEAP_SIZE] __attribute__((aligned(8)));
static uint32_t used;
bool rg_drawing = true;
static uint16_t frame[256 * 240];
static int width = 160, lines_seen;

void *rg_alloc(uint32_t size) {
  size = (size + 7) & ~7u;
  if (used + size > sizeof heap) return NULL;
  void *p = heap + used;
  used += size;
  memset(p, 0, size);
  return p;
}
void rg_alloc_reset(void) { used = 0; }

void rg_line(int y, const uint16_t *px) {
  if (y < 0 || y >= 240) {
    fprintf(stderr, "line %d out of the picture\n", y);
    abort();
  }
  memcpy(frame + y * 256, px, (size_t)width * 2);
  lines_seen++;
}

/* the ROM, read only as the calculator's flash is: a write into it stops the test */
static uint8_t *load(const char *path, uint32_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  size_t pages = ((size_t)n + 4095) / 4096 * 4096;
  uint8_t *d = mmap(NULL, pages, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (d == MAP_FAILED || fread(d, 1, (size_t)n, f) != (size_t)n) return NULL;
  fclose(f);
  mprotect(d, pages, PROT_READ);
  *size = (uint32_t)n;
  return d;
}

int main(int argc, char **argv) {
  if (argc < 4) return fprintf(stderr, "usage: host_core ROM FRAMES OUT.ppm [SEED]\n"), 2;
  uint32_t size;
  uint8_t *rom = load(argv[1], &size);
  if (!rom) return fprintf(stderr, "can't read %s\n", argv[1]), 1;
  int frames = atoi(argv[2]);
  unsigned seed = argc > 4 ? (unsigned)atoi(argv[4]) : 0;
  const char *ext = strrchr(argv[1], '.');
  bool nes = ext && !strcmp(ext, ".nes");
  int r = nes ? nes_load(rom, size) : gb_load(rom, size);
  if (r != RG_OK) return fprintf(stderr, "load failed: %d\n", r), 1;
  int h = nes ? 240 : 144;
  width = nes ? 256 : 160;
  int pad = 0;
  const char *script = argc > 5 ? argv[5] : ""; /* "FRAME=PAD,FRAME=PAD...": the pad from then on */
  for (int i = 0; i < frames; i++) {
    if (seed) {
      seed = seed * 1103515245u + 12345u;
      if ((seed >> 16) % 8 == 0) pad = (int)((seed >> 8) & 0xFF);
    }
    while (*script && atoi(script) == i) {
      const char *eq = strchr(script, '=');
      if (!eq) break;
      pad = atoi(eq + 1);
      const char *comma = strchr(eq, ',');
      script = comma ? comma + 1 : "";
    }
    if (nes) nes_set_pad(pad), nes_run_frame(true);
    else gb_set_pad(pad), gb_run_frame(true);
  }
  /* STATE=1: a save state taken now, 120 more frames, the state put back, the same 120 frames:
     the same picture at the end (the state holds the whole machine); then states of random
     bytes, which must not crash it. REDRAW=1: the last frame drawn again is the same picture. */
  if (getenv("STATE")) {
    rg_chunk_t parts[RG_MAX_CHUNKS];
    int n = nes ? nes_state_chunks(parts) : gb_state_chunks(parts);
    static uint8_t copy[RG_HEAP_SIZE + 65536];
    uint32_t at = 0;
    for (int i = 0; i < n; i++) memcpy(copy + at, parts[i].at, parts[i].size), at += parts[i].size;
    static uint16_t first[256 * 240];
    for (int pass = 0; pass < 2; pass++) {
      if (pass) {
        n = nes ? nes_state_chunks(parts) : gb_state_chunks(parts);
        at = 0;
        for (int i = 0; i < n; i++) memcpy(parts[i].at, copy + at, parts[i].size), at += parts[i].size;
        if (nes) nes_state_loaded();
        else gb_state_loaded();
      }
      for (int i = 0; i < 120; i++) {
        int p = (i / 10) % 3 ? 0 : RG_PAD_RIGHT | (i & 16 ? RG_PAD_A : 0);
        if (nes) nes_set_pad(p), nes_run_frame(true);
        else gb_set_pad(p), gb_run_frame(true);
      }
      if (!pass) memcpy(first, frame, sizeof first);
    }
    printf("state %s\n", memcmp(first, frame, sizeof first) ? "differs" : "same");
    for (int k = 0; k < 30; k++) {
      n = nes ? nes_state_chunks(parts) : gb_state_chunks(parts);
      for (int i = 0; i < n; i++)
        for (uint32_t j = 0; j < parts[i].size; j++)
          if (k < 10 || (j * 7 + (unsigned)k) % 13 == 0) {
            seed = seed * 1103515245u + 12345u + (unsigned)k;
            ((uint8_t *)parts[i].at)[j] = (uint8_t)(seed >> 16);
          }
      if (nes) nes_state_loaded();
      else gb_state_loaded();
      for (int i = 0; i < 30; i++) {
        if (nes) nes_set_pad(i & 0xFF), nes_run_frame(true);
        else gb_set_pad(i & 0xFF), gb_run_frame(true);
      }
    }
    printf("random states: no crash\n");
  }
  if (getenv("REDRAW")) {
    static uint16_t last[256 * 240];
    memcpy(last, frame, sizeof last);
    if (nes) nes_redraw();
    else gb_redraw();
    int differ = 0;
    for (int i = 0; i < 256 * h; i++) differ += last[i] != frame[i];
    printf("redraw: %d pixels differ\n", differ);
  }
  FILE *o = fopen(argv[3], "wb");
  fprintf(o, "P6\n%d %d\n255\n", width, h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < width; x++) {
      uint16_t c = frame[y * 256 + x];
      fputc((c >> 11) << 3 | (c >> 13), o);
      fputc(((c >> 5) & 63) << 2 | ((c >> 9) & 3), o);
      fputc((c & 31) << 3 | ((c >> 2) & 7), o);
    }
  fclose(o);
  printf("%d frames, %d lines, %u bytes of RAM\n", frames, lines_seen, used);
  return 0;
}
