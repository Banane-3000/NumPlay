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
** nes_mmc.c
**
** NES Memory Management Controller (mapper) emulation
** $Id: nes_mmc.c,v 1.2 2001/04/27 14:37:11 neil Exp $
*/

#include <string.h>
#include "noftypes.h"
#include "nes6502.h"
#include "nes_mmc.h"
#include "nes_ppu.h"
#include "mmclist.h"
#include "nes_rom.h"

#define  MMC_8KROM         (mmc.cart->rom_banks * 2)
#define  MMC_16KROM        (mmc.cart->rom_banks)
#define  MMC_32KROM        (mmc.cart->rom_banks / 2)
#define  MMC_8KVROM        (mmc.cart->vrom_banks)
#define  MMC_4KVROM        (mmc.cart->vrom_banks * 2)
#define  MMC_2KVROM        (mmc.cart->vrom_banks * 4)
#define  MMC_1KVROM        (mmc.cart->vrom_banks * 8)

#define  MMC_LAST8KROM     (MMC_8KROM - 1)
#define  MMC_LAST16KROM    (MMC_16KROM - 1)
#define  MMC_LAST32KROM    (MMC_32KROM - 1)
#define  MMC_LAST8KVROM    (MMC_8KVROM - 1)
#define  MMC_LAST4KVROM    (MMC_4KVROM - 1)
#define  MMC_LAST2KVROM    (MMC_2KVROM - 1)
#define  MMC_LAST1KVROM    (MMC_1KVROM - 1)

mmc_t mmc;

rominfo_t *mmc_getinfo(void)
{
   return mmc.cart;
}

/* VROM bankswitching */
void mmc_bankvrom(int size, uint32 address, int bank)
{
   if (0 == mmc.cart->vrom_banks)
      return;

   switch (size)
   {
   case 1:
      if (bank == MMC_LASTBANK)
         bank = MMC_LAST1KVROM;
      ppu_setpage(1, address >> 10, &mmc.cart->vrom[((unsigned) bank % (unsigned) MMC_1KVROM) << 10] - address);
      break;

   case 2:
      if (bank == MMC_LASTBANK)
         bank = MMC_LAST2KVROM;
      ppu_setpage(2, address >> 10, &mmc.cart->vrom[((unsigned) bank % (unsigned) MMC_2KVROM) << 11] - address);
      break;

   case 4:
      if (bank == MMC_LASTBANK)
         bank = MMC_LAST4KVROM;
      ppu_setpage(4, address >> 10, &mmc.cart->vrom[((unsigned) bank % (unsigned) MMC_4KVROM) << 12] - address);
      break;

   case 8:
      if (bank == MMC_LASTBANK)
         bank = MMC_LAST8KVROM;
      ppu_setpage(8, 0, &mmc.cart->vrom[((unsigned) bank % (unsigned) MMC_8KVROM) << 13]);
      break;

   default:
      log_printf("invalid VROM bank size %d\n", size);
   }
}

/* ROM bankswitching */
void mmc_bankrom(int size, uint32 address, int bank)
{
   nes6502_context mmc_cpu;

   nes6502_getcontext(&mmc_cpu); 

   switch (size)
   {
   case 8:
      if (bank == MMC_LASTBANK)
         bank = MMC_LAST8KROM;
      {
         int page = address >> NES6502_BANKSHIFT;
         mmc_cpu.mem_page[page] = &mmc.cart->rom[((unsigned) bank % (unsigned) MMC_8KROM) << 13];
         mmc_cpu.mem_page[page + 1] = mmc_cpu.mem_page[page] + 0x1000;
      }

      break;

   case 16:
      if (bank == MMC_LASTBANK)
         bank = MMC_LAST16KROM;
      {
         int page = address >> NES6502_BANKSHIFT;
         mmc_cpu.mem_page[page] = &mmc.cart->rom[((unsigned) bank % (unsigned) MMC_16KROM) << 14];
         mmc_cpu.mem_page[page + 1] = mmc_cpu.mem_page[page] + 0x1000;
         mmc_cpu.mem_page[page + 2] = mmc_cpu.mem_page[page] + 0x2000;
         mmc_cpu.mem_page[page + 3] = mmc_cpu.mem_page[page] + 0x3000;
      }
      break;

   case 32:
      if (bank == MMC_LASTBANK)
         bank = MMC_LAST32KROM;

      if (0 == MMC_32KROM) /* (Retro-Go on NumPlay: 16 KB of PRG ROM, twice) */
      {
         for (int page = 8; page < 16; page++)
            mmc_cpu.mem_page[page] = &mmc.cart->rom[(page & 3) << 12];
         break;
      }
      mmc_cpu.mem_page[8] = &mmc.cart->rom[((unsigned) bank % (unsigned) MMC_32KROM) << 15];
      mmc_cpu.mem_page[9] = mmc_cpu.mem_page[8] + 0x1000;
      mmc_cpu.mem_page[10] = mmc_cpu.mem_page[8] + 0x2000;
      mmc_cpu.mem_page[11] = mmc_cpu.mem_page[8] + 0x3000;
      mmc_cpu.mem_page[12] = mmc_cpu.mem_page[8] + 0x4000;
      mmc_cpu.mem_page[13] = mmc_cpu.mem_page[8] + 0x5000;
      mmc_cpu.mem_page[14] = mmc_cpu.mem_page[8] + 0x6000;
      mmc_cpu.mem_page[15] = mmc_cpu.mem_page[8] + 0x7000;
      break;

   default:
      log_printf("invalid ROM bank size %d\n", size);
      break;
   }

   nes6502_setcontext(&mmc_cpu);
}

/* Check to see if this mapper is supported */
bool mmc_peek(int map_num)
{
   mapintf_t **map_ptr = mappers;

   while (NULL != *map_ptr)
   {
      if ((*map_ptr)->number == map_num)
         return true;
      map_ptr++;
   }

   return false;
}

static void mmc_setpages(void)
{
   log_printf("setting up mapper %d\n", mmc.intf->number);

   /* Switch ROM into CPU space, set VROM/VRAM (done for ALL ROMs) */
   mmc_bankrom(16, 0x8000, 0);
   mmc_bankrom(16, 0xC000, MMC_LASTBANK);
   mmc_bankvrom(8, 0x0000, 0);

   if (mmc.cart->flags & ROM_FLAG_FOURSCREEN)
   {
      ppu_mirror(0, 1, 2, 3);
   }
   else
   {
      if (MIRROR_VERT == mmc.cart->mirror)
         ppu_mirror(0, 1, 0, 1);
      else
         ppu_mirror(0, 0, 1, 1);
   }

   /* if we have no VROM, switch in VRAM */
   /* TODO: fix this hack implementation */
   if (0 == mmc.cart->vrom_banks)
   {
      ASSERT(mmc.cart->vram);

      ppu_setpage(8, 0, mmc.cart->vram);
      ppu_mirrorhipages();
   }
}

/* Mapper initialization routine */
void mmc_reset(void)
{
   mmc_setpages();

   ppu_setlatchfunc(NULL);
   ppu_setvromswitch(NULL);

   if (mmc.intf->init)
      mmc.intf->init();

   log_printf("reset memory mapper\n");
}


/* the cartridge's mapper: false if nofrendo has none for it */
bool mmc_init(rominfo_t *rominfo)
{
   mapintf_t **map_ptr;

   for (map_ptr = mappers; *map_ptr; map_ptr++)
   {
      if ((*map_ptr)->number == rominfo->mapper_number)
      {
         mmc.intf = *map_ptr;
         mmc.cart = rominfo;
         return true;
      }
   }
   return false;
}
