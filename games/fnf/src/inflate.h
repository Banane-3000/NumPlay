#ifndef FNF_INFLATE_H
#define FNF_INFLATE_H
#include <stdint.h>

/* Unpacks n bytes of raw DEFLATE into out (cap bytes at most); returns the size unpacked, or -1 if damaged. */
int32_t fnf_inflate(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t cap);
#endif
