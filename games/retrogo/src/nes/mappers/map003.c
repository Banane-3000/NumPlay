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
** map3.c
**
** mapper 3 interface
** $Id: map003.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "noftypes.h"
#include "nes_mmc.h"

/* mapper 3: CNROM */
static void map3_write(uint32 address, uint8 value)
{
   UNUSED(address);

   mmc_bankvrom(8, 0x0000, value);
}

static map_memwrite map3_memwrite[] =
{
   { 0x8000, 0xFFFF, map3_write },
   {     -1,     -1, NULL }
};

mapintf_t map3_intf =
{
   3, /* mapper number */
   "CNROM", /* mapper name */
   NULL, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* get state (snss) */
   NULL, /* set state (snss) */
   NULL, /* memory read structure */
   map3_memwrite, /* memory write structure */
   NULL /* external sound device */
};

