/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
** This program is free software; you can redistribute it and/or modify it under the terms of
** the GNU General Public License as published by the Free Software Foundation; either version 3
** of the License, or (at your option) any later version. (Nofrendo was released under version
** 2 of the GNU Library General Public License; as its section 3 allows, this copy, changed for
** Retro-Go on NumPlay, is under the GNU General Public License instead.)
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
** without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
**
** Any permitted reproduction of these routines, in whole or in part, must bear this legend.
**
** noftypes.h: the types nofrendo uses.
*/

#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define  HOST_LITTLE_ENDIAN
#define  INLINE      static inline
#define  ZERO_LENGTH 0

/* quell stupid compiler warnings */
#define  UNUSED(x)   ((void)(x))

typedef  signed char    int8;
typedef  signed short   int16;
typedef  signed int     int32;
typedef  unsigned char  uint8;
typedef  unsigned short uint16;
typedef  unsigned int   uint32;

#define  ASSERT(expr)
#define  ASSERT_MSG(msg)
#define  log_printf(...)
#define  __PACKED__ __attribute__((packed))

#endif /* _TYPES_H_ */
