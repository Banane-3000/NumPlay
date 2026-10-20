/* The NES's sound registers without the sound: what $4015 reads back (the channels' length
 * counters, the frame interrupt), for the games that wait on them. Written for Retro-Go on
 * NumPlay; GPL-3.0-or-later. The length table and the halt bits are the 2A03's (NESdev wiki). */
#include "nes_apu.h"
#include "nes.h"

apu_t apu;

static const uint8 length_table[32] = {
   10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
   12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};

void apu_reset(void)
{
   memset(&apu, 0, sizeof apu);
}

static bool halted(int ch)
{
   return ch == 2 ? (apu.regs[8] & 0x80) : (apu.regs[ch * 4] & 0x20);
}

void apu_halfframe(void)
{
   for (int ch = 0; ch < 4; ch++)
      if (apu.length[ch] && !halted(ch))
         apu.length[ch]--;
}

uint8 apu_read(uint32 address)
{
   if (address != 0x4015)
      return (uint8) (address >> 8); /* (open bus) */
   uint8 value = 0;
   for (int ch = 0; ch < 4; ch++)
      if (apu.length[ch])
         value |= (uint8) (1 << ch);
   return value | nes_clearfiq();
}

void apu_write(uint32 address, uint8 value)
{
   if (address == 0x4015)
   {
      apu.enabled = value & 0x1F;
      for (int ch = 0; ch < 4; ch++)
         if (!(value & (1 << ch)))
            apu.length[ch] = 0;
      return;
   }
   if (address < 0x4000 || address >= 0x4018)
      return;
   apu.regs[address - 0x4000] = value;
   if (address < 0x4010 && (address & 3) == 3)
   {
      int ch = (int) ((address - 0x4000) >> 2);
      if (apu.enabled & (1 << ch))
         apu.length[ch] = length_table[value >> 3];
   }
}
