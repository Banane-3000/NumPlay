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
** map99.c
**
** mapper 99 interface
** $Id: map099.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes_ppu.h"

/* Switch VROM for VS games */
static void map99_vromswitch(uint8 value)
{
   int bank = (value & 0x04) >> 2;
   mmc_bankvrom(8, 0x0000, bank);
}

/* mapper 99: VS. System */
static void map99_init(void)
{
   ppu_mirror(0, 1, 2, 3);
   ppu_setvromswitch(map99_vromswitch);
}

mapintf_t map99_intf =
{
   99, /* mapper number */
   "VS. System", /* mapper name */
   map99_init, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* get state (snss) */
   NULL, /* set state (snss) */
   NULL, /* memory read structure */
   NULL, /* memory write structure */
   NULL /* external sound device */
};

