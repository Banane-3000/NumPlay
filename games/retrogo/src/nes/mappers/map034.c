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
** map34.c
**
** mapper 34 interface
** $Id: map034.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "noftypes.h"
#include "nes_mmc.h"

static void map34_init(void)
{
   mmc_bankrom(32, 0x8000, MMC_LASTBANK);
}

static void map34_write(uint32 address, uint8 value)
{
   if ((address & 0x8000) || (0x7FFD == address))
   {
      mmc_bankrom(32, 0x8000, value);
   }
   else if (0x7FFE == address)
   {
      mmc_bankvrom(4, 0x0000, value);
   }
   else if (0x7FFF == address)
   {
      mmc_bankvrom(4, 0x1000, value);
   }
}

static map_memwrite map34_memwrite[] = 
{
   { 0x7FFD, 0xFFFF, map34_write },
   { -1, -1, NULL }
};

mapintf_t map34_intf = 
{
   34, /* mapper number */
   "Nina-1", /* mapper name */
   map34_init, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* get state (snss) */
   NULL, /* set state (snss) */
   NULL, /* memory read structure */
   map34_memwrite, /* memory write structure */
   NULL /* external sound device */
};

