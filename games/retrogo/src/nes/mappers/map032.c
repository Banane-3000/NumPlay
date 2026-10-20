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
** map32.c
**
** mapper 32 interface
** $Id: map032.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes_ppu.h"

static int select_c000 = 0;

/* mapper 32: Irem G-101 */
static void map32_write(uint32 address, uint8 value)
{
   switch (address >> 12)
   {
   case 0x08: 
      if (select_c000)
         mmc_bankrom(8, 0xC000, value);
      else
         mmc_bankrom(8, 0x8000, value);
      break;

   case 0x09: 
      if (value & 1)
         ppu_mirror(0, 0, 1, 1); /* horizontal */
      else
         ppu_mirror(0, 1, 0, 1); /* vertical */
   
      select_c000 = (value & 0x02);
      break;

   case 0x0A: 
      mmc_bankrom(8, 0xA000, value); 
      break;

   case 0x0B: 
      {
         int loc = (address & 0x07) << 10;
         mmc_bankvrom(1, loc, value);
      }
      break;

   default:
      break;
   }
}

static map_memwrite map32_memwrite[] =
{
   { 0x8000, 0xFFFF, map32_write },
   {     -1,     -1, NULL }
};

mapintf_t map32_intf =
{
   32, /* mapper number */
   "Irem G-101", /* mapper name */
   NULL, /* init routine */
   NULL, /* vblank callback */
   NULL, /* hblank callback */
   NULL, /* get state (snss) */
   NULL, /* set state (snss) */
   NULL, /* memory read structure */
   map32_memwrite, /* memory write structure */
   NULL /* external sound device */
};

