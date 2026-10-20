/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
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
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** nes_rom.h
**
** NES ROM loading/saving related defines / prototypes
** $Id: nes_rom.h,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#ifndef _NES_ROM_H_
#define _NES_ROM_H_

#include "noftypes.h"

typedef enum
{
   MIRROR_HORIZ   = 0,
   MIRROR_VERT    = 1
} mirror_t;

#define  ROM_FLAG_BATTERY     0x01
#define  ROM_FLAG_TRAINER     0x02
#define  ROM_FLAG_FOURSCREEN  0x04
#define  ROM_FLAG_VERSUS      0x08

typedef struct rominfo_s
{
   /* pointers to ROM and VROM: the cartridge read in place, never written */
   uint8 *rom, *vrom;

   /* pointers to SRAM and VRAM */
   uint8 *sram, *vram;

   /* number of banks */
   int rom_banks, vrom_banks;
   int sram_banks, vram_banks;

   int mapper_number;
   mirror_t mirror;

   uint8 flags;
} rominfo_t;


#endif /* _NES_ROM_H_ */

