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
** map78.c
**
** mapper 78 interface
** $Id: map078.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes_ppu.h"

/* mapper 78: Holy Diver, Cosmo Carrier */
/* ($8000-$FFFF) D2-D0 = switch $8000-$BFFF */
/* ($8000-$FFFF) D7-D4 = switch PPU $0000-$1FFF */
/* ($8000-$FFFF) D3 = switch mirroring */
static void map78_write(uint32 address, uint8 value)
{
   UNUSED(address);

   mmc_bankrom(16, 0x8000, value & 7);
   mmc_bankvrom(8, 0x0000, (value >> 4) & 0x0F);

   /* Ugh! Same abuse of the 4-screen bit as with Mapper #70 */
   if (mmc_getinfo()->flags & ROM_FLAG_FOURSCREEN)
   {
      if (value & 0x08)
         ppu_mirror(0, 1, 0, 1); /* vert */
      else
         ppu_mirror(0, 0, 1, 1); /* horiz */
   }
   else
   {
      int mirror = (value >> 3) & 1;
      ppu_mirror(mirror, mirror, mirror, mirror);
   }
}

static map_memwrite map78_memwrite[] =
{
   { 0x8000, 0xFFFF, map78_write },
   {     -1,     -1, NULL }
};

mapintf_t map78_intf =
{
   78, /* mapper number */
   "Mapper 78", /* mapper name */
   NULL, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* get state (snss) */
   NULL, /* set state (snss) */
   NULL, /* memory read structure */
   map78_memwrite, /* memory write structure */
   NULL /* external sound device */
};

