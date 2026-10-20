/* Retro-Go's saves, packed (pack.c): the cartridges' RAM and the save states are mostly zeros
 * and repeats, and the calculator's file system holds 42 KB. */
#ifndef RG_PACK_H
#define RG_PACK_H
#include <stdbool.h>
#include <stdint.h>

#define RG_PACK_HASH 4096 /* entries of the packer's table (uint16_t each) */
#define RG_PACK_MAX 65535 /* the most bytes packed at a time */

/* n bytes packed into out (NULL: only measured); the packed size */
uint32_t rg_pack(const uint8_t *in, uint32_t n, uint8_t *out, uint16_t *table);
/* exactly `size` bytes unpacked from n; false if the data doesn't hold together (out may then be
   partly written) */
bool rg_unpack(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t size);
#endif
