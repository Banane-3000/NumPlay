/* gnuboy's basic types. From gnuboy 1.0.3 (rofl0r's tree), GPL-2.0-or-later; see COPYING and
 * README.md in this folder. Trimmed for Retro-Go on NumPlay. */
#ifndef __DEFS_H__
#define __DEFS_H__

#define IS_LITTLE_ENDIAN 1
#define LO 0
#define HI 1
#define LIL(x) (x)

typedef unsigned char byte;

typedef unsigned char un8;
typedef unsigned short un16;
typedef unsigned int un32;

typedef signed char n8;
typedef signed short n16;
typedef signed int n32;

typedef un16 word;
typedef word addr;

#endif
