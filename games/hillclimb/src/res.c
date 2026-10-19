/* The pictures and fonts in data.bin: "HCRD", their counts, then where each one starts. */
#include "res.h"

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

const Img *img(int id) {
  return (const Img *)(const void *)(hc_data + rd32(hc_data + 16 + 4 * id));
}

const Font *font(int id) {
  uint32_t nimg = rd32(hc_data + 4);
  return (const Font *)(const void *)(hc_data + rd32(hc_data + 16 + 4 * (nimg + (uint32_t)id)));
}
