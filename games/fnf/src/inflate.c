/* Raw DEFLATE (RFC 1951) into a buffer, for the pictures and the charts. Huffman codes up to 9 bits long are read
 * through a table of the next 9 bits, longer ones bit by bit; a damaged stream is refused, never overrun. */
#include <string.h>
#include "inflate.h"

#define FAST 9

typedef struct {
  uint16_t count[16], symbol[288];
  uint16_t fast[1 << FAST];   /* length << 9 | symbol, 0: longer than FAST bits */
} huff_t;

typedef struct {
  const uint8_t *in, *end;
  uint32_t bits;
  int nbits, pad;   /* pad: zero bytes read past the end (a look ahead may; using them is a damaged stream) */
  int bad;
} reader_t;

static void need(reader_t *r, int n) {
  while (r->nbits < n) {
    uint32_t b = 0;
    if (r->in < r->end) b = *r->in++;
    else if (++r->pad > 4) r->bad = 1;
    r->bits |= b << r->nbits;
    r->nbits += 8;
  }
}

static uint32_t take(reader_t *r, int n) {
  if (!n) return 0;
  need(r, n);
  uint32_t v = r->bits & ((1u << n) - 1);
  r->bits >>= n, r->nbits -= n;
  return v;
}

static int build(huff_t *h, const uint8_t *len, int n) {
  uint16_t offs[16];
  memset(h->count, 0, sizeof h->count);
  memset(h->fast, 0, sizeof h->fast);
  for (int i = 0; i < n; i++) h->count[len[i]]++;
  h->count[0] = 0;
  int left = 1;
  for (int l = 1; l < 16; l++) {
    left = (left << 1) - h->count[l];
    if (left < 0) return 0;
  }
  offs[1] = 0;
  for (int l = 1; l < 15; l++) offs[l + 1] = (uint16_t)(offs[l] + h->count[l]);
  for (int i = 0; i < n; i++)
    if (len[i]) h->symbol[offs[len[i]]++] = (uint16_t)i;
  /* the table: the canonical codes, bit-reversed (the stream gives a code's first bit first) */
  int code = 0, k = 0;
  for (int l = 1; l <= FAST; l++) {
    for (int c = 0; c < h->count[l]; c++, k++, code++) {
      int rev = 0;
      for (int b = 0; b < l; b++) rev |= ((code >> b) & 1) << (l - 1 - b);
      for (int i = rev; i < (1 << FAST); i += 1 << l) h->fast[i] = (uint16_t)(l << 9 | h->symbol[k]);
    }
    code <<= 1;
  }
  return 1;
}

static int decode(reader_t *r, const huff_t *h) {
  need(r, FAST);
  uint16_t f = h->fast[r->bits & ((1 << FAST) - 1)];
  if (f) {
    int l = f >> 9;
    r->bits >>= l, r->nbits -= l;
    return f & 511;
  }
  int code = 0, first = 0, index = 0;
  for (int l = 1; l < 16; l++) {
    code |= (int)take(r, 1);
    int count = h->count[l];
    if (code - count < first) return h->symbol[index + (code - first)];
    index += count;
    first = (first + count) << 1;
    code <<= 1;
  }
  r->bad = 1;
  return -1;
}

static const uint16_t LEN_BASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99,
                                      115, 131, 163, 195, 227, 258};
static const uint8_t LEN_EXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t DIST_BASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                                       1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t DIST_EXTRA[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
                                       12, 12, 13, 13};

static huff_t lit, dist;   /* (static: 2.6 KB off the stack) */

int32_t fnf_inflate(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t cap) {
  static const uint8_t ORDER[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
  reader_t r = {in, in + n, 0, 0, 0, 0};
  uint32_t o = 0;
  int last;
  do {
    last = (int)take(&r, 1);
    int type = (int)take(&r, 2);
    if (type == 0) {   /* stored */
      r.bits >>= r.nbits & 7, r.nbits -= r.nbits & 7;
      uint32_t len = take(&r, 16), nlen = take(&r, 16);
      if ((len ^ 0xFFFF) != nlen || o + len > cap) return -1;
      for (uint32_t i = 0; i < len; i++) out[o++] = (uint8_t)take(&r, 8);
    } else if (type == 1 || type == 2) {
      uint8_t len[320];
      int nlit = 288, ndist = 30;
      if (type == 1) {
        for (int i = 0; i < 288; i++) len[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
        for (int i = 0; i < 30; i++) len[288 + i] = 5;
      } else {
        nlit = (int)take(&r, 5) + 257, ndist = (int)take(&r, 5) + 1;
        int ncode = (int)take(&r, 4) + 4;
        if (nlit > 286 || ndist > 30) return -1;
        memset(len, 0, 19);
        for (int i = 0; i < ncode; i++) len[ORDER[i]] = (uint8_t)take(&r, 3);
        if (!build(&lit, len, 19)) return -1;
        for (int i = 0; i < nlit + ndist;) {
          int s = decode(&r, &lit);
          if (s < 0 || r.bad) return -1;
          if (s < 16) {
            len[i++] = (uint8_t)s;
            continue;
          }
          int rep, v = 0;
          if (s == 16) {
            if (!i) return -1;
            v = len[i - 1], rep = 3 + (int)take(&r, 2);
          } else if (s == 17) {
            rep = 3 + (int)take(&r, 3);
          } else {
            rep = 11 + (int)take(&r, 7);
          }
          if (i + rep > nlit + ndist) return -1;
          while (rep--) len[i++] = (uint8_t)v;
        }
        if (!len[256]) return -1;
      }
      if (!build(&lit, len, nlit) || !build(&dist, len + nlit, ndist)) return -1;
      for (;;) {
        int s = decode(&r, &lit);
        if (s < 0 || r.bad) return -1;
        if (s < 256) {
          if (o >= cap) return -1;
          out[o++] = (uint8_t)s;
        } else if (s == 256) {
          break;
        } else {
          s -= 257;
          if (s >= 29) return -1;
          uint32_t l = LEN_BASE[s] + take(&r, LEN_EXTRA[s]);
          int d = decode(&r, &dist);
          if (d < 0 || d >= 30) return -1;
          uint32_t back = DIST_BASE[d] + take(&r, DIST_EXTRA[d]);
          if (r.bad || back > o || o + l > cap) return -1;
          for (uint32_t i = 0; i < l; i++, o++) out[o] = out[o - back];
        }
      }
    } else {
      return -1;
    }
    if (r.bad || r.pad * 8 > r.nbits) return -1;
  } while (!last);
  return (int32_t)o;
}
