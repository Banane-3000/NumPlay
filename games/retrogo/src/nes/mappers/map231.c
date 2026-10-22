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
** map231.c
**
** mapper 231 interface
** $Id: map231.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "noftypes.h"
#include "nes_mmc.h"

/* mapper 231: NINA-07, used in Wally Bear and the NO! Gang */

static void map231_init(void)
{
   mmc_bankrom(32, 0x8000, MMC_LASTBANK);
}

static void map231_write(uint32 address, uint8 value)
{
   int bank, vbank;
   UNUSED(address);

   bank = ((value & 0x80) >> 5) | (value & 0x03);
   vbank = (value >> 4) & 0x07;

   mmc_bankrom(32, 0x8000, bank);
   mmc_bankvrom(8, 0x0000, vbank);
}

static map_memwrite map231_memwrite[] =
{
   { 0x8000, 0xFFFF, map231_write },
   {     -1,     -1, NULL }
};

mapintf_t map231_intf = 
{
   231, /* mapper number */
   "NINA-07", /* mapper name */
   map231_init, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* get state (snss) */
   NULL, /* set state (snss) */
   NULL, /* memory read structure */
   map231_memwrite, /* memory write structure */
   NULL /* external sound device */
};

