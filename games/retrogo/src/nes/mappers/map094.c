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
** map94.c
**
** mapper 94 interface
** $Id: map094.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "noftypes.h"
#include "nes_mmc.h"

/* mapper 94: Senjou no Ookami */
static void map94_write(uint32 address, uint8 value)
{
   UNUSED(address);

   /* ($8000-$FFFF) D7-D2 = switch $8000-$BFFF */
   mmc_bankrom(16, 0x8000, value >> 2);
}

static map_memwrite map94_memwrite[] =
{
   { 0x8000, 0xFFFF, map94_write },
   {     -1,     -1, NULL }
};

mapintf_t map94_intf =
{
   94, /* mapper number */
   "Mapper 94", /* mapper name */
   NULL, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* get state (snss) */
   NULL, /* set state (snss) */
   NULL, /* memory read structure */
   map94_memwrite, /* memory write structure */
   NULL /* external sound device */
};

