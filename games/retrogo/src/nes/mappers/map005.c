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
** map5.c
**
** mapper 5 interface
** $Id: map005.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include "noftypes.h"
#include "nes_mmc.h"
#include "nes.h"

/* TODO: there's lots of info about this mapper now;
** let's implement it correctly/completely
*/

static struct
{
   int counter, enabled;
   int reset, latch;
} irq;

/* MMC5 - Castlevania III, etc */
static void map5_hblank(int vblank)
{
   UNUSED(vblank);

   if (irq.counter == nes_getcontextptr()->scanline)
   {
      if (true == irq.enabled)
      {
         nes_irq();
         irq.reset = true;
      }
      //else 
      //   irq.reset = false;
      irq.counter = irq.latch;
   }
}

static void map5_write(uint32 address, uint8 value)
{
   static int page_size = 8;

   /* ex-ram memory-- bleh! */
   if (address >= 0x5C00 && address <= 0x5FFF)
      return;

   switch (address)
   {
   case 0x5100:
      /* PRG page size setting */
      /* 0:32k 1:16k 2,3:8k */
      switch (value & 3)
      {
      case 0:
         page_size = 32;
         break;

      case 1:
         page_size = 16;
         break;
      
      case 2:
      case 3:
         page_size = 8;
         break;
      }
      break;

   case 0x5101:
      /* CHR page size setting */
      /* 0:8k 1:4k 2:2k 3:1k */
      break;

   case 0x5104:
      /* GFX mode setting */
      /*
      00:split mode
      01:split & exgraffix
      10:ex-ram
      11:exram + write protect
      */
      break;
   
   case 0x5105:
      /* TODO: exram needs to fill in nametables 2-3 */
      ppu_mirror(value & 3, (value >> 2) & 3, (value >> 4) & 3, value >> 6);
      break;

   case 0x5106:
   case 0x5107:
      /* ex-ram fill mode stuff */
      break;
   
   case 0x5113:
      /* ram page for $6000-7FFF? bit 2*/
      break;
   
   case 0x5114:
      mmc_bankrom(8, 0x8000, value);
      //if (page_size == 8)
      //   mmc_bankrom(8, 0x8000, value);
      break;

   case 0x5115:
      mmc_bankrom(8, 0x8000, value);
      mmc_bankrom(8, 0xA000, value + 1);
      //if (page_size == 8)
      //   mmc_bankrom(8, 0xA000, value);
      //else if (page_size == 16)
      //   mmc_bankrom(16, 0x8000, value >> 1);
         //mmc_bankrom(16, 0x8000, value & 0xFE);
      break;

   case 0x5116:
      mmc_bankrom(8, 0xC000, value);
      //if (page_size == 8)
      //   mmc_bankrom(8, 0xC000, value);
      break;

   case 0x5117:
      //if (page_size == 8)
      //   mmc_bankrom(8, 0xE000, value);
      //else if (page_size == 16)
      //   mmc_bankrom(16, 0xC000, value >> 1);
         //mmc_bankrom(16, 0xC000, value & 0xFE);
      //else if (page_size == 32)
      //   mmc_bankrom(32, 0x8000, value >> 2);
         //mmc_bankrom(32, 0x8000, value & 0xFC);
      break;

   case 0x5120:
      mmc_bankvrom(1, 0x0000, value);
      break;

   case 0x5121:
      mmc_bankvrom(1, 0x0400, value);
      break;

   case 0x5122:
      mmc_bankvrom(1, 0x0800, value);
      break;

   case 0x5123:
      mmc_bankvrom(1, 0x0C00, value);
      break;

   case 0x5124:
   case 0x5125:
   case 0x5126:
   case 0x5127:
      /* more VROM shit? */
      break;

   case 0x5128:
      mmc_bankvrom(1, 0x1000, value);
      break;

   case 0x5129:
      mmc_bankvrom(1, 0x1400, value);
      break;

   case 0x512A:
      mmc_bankvrom(1, 0x1800, value);
      break;

   case 0x512B:
      mmc_bankvrom(1, 0x1C00, value);
      break;

   case 0x5203:
      irq.counter = value;
      irq.latch = value;
//      irq.reset = false;
      break;

   case 0x5204:
      irq.enabled = (value & 0x80) ? true : false;
//      irq.reset = false;
      break;

   default:
#ifdef NOFRENDO_DEBUG
      log_printf("unknown mmc5 write: $%02X to $%04X\n", value, address);
#endif /* NOFRENDO_DEBUG */
      break;
   }
}

static uint8 map5_read(uint32 address)
{
   /* Castlevania 3 IRQ counter */
   if (address == 0x5204)
   {
      /* if reset == 1, we've hit scanline */
      return (irq.reset ? 0x40 : 0x00);
   }
   else
   {
#ifdef NOFRENDO_DEBUG
      log_printf("invalid MMC5 read: $%04X", address);
#endif
      return 0xFF;
   }
}

static void map5_init(void)
{
   mmc_bankrom(8, 0x8000, MMC_LASTBANK);
   mmc_bankrom(8, 0xA000, MMC_LASTBANK);
   mmc_bankrom(8, 0xC000, MMC_LASTBANK);
   mmc_bankrom(8, 0xE000, MMC_LASTBANK);

   irq.counter = irq.enabled = 0;
   irq.reset = irq.latch = 0;
}

/* incomplete SNSS definition */
static void map5_getstate(SnssMapperBlock *state)
{
   state->extraData.mapper5.dummy = 0;
}

static void map5_setstate(SnssMapperBlock *state)
{
   UNUSED(state);
}

static map_memwrite map5_memwrite[] =
{
   /* $5000 - $5015 handled by sound */
   { 0x5016, 0x5FFF, map5_write },
   { 0x8000, 0xFFFF, map5_write },
   {     -1,     -1, NULL }
};

static map_memread map5_memread[] = 
{
   { 0x5204, 0x5204, map5_read },
   {     -1,     -1, NULL }
};

mapintf_t map5_intf =
{
   5, /* mapper number */
   "MMC5", /* mapper name */
   map5_init, /* init routine */
   NULL, /* vblank callback */
   map5_hblank, /* hblank callback */
   map5_getstate, /* get state (snss) */
   map5_setstate, /* set state (snss) */
   map5_memread, /* memory read structure */
   map5_memwrite, /* memory write structure */
   NULL /* external sound device (no sound: the calculator has no speaker) */
};
