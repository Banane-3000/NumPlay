/* The part of a mapper a save state keeps besides its banks: what nofrendo's mappers read and
 * write in their get_state and set_state (written for Retro-Go on NumPlay with the names those
 * mappers use; GPL-3.0-or-later). */
#ifndef _LIBSNSS_H_
#define _LIBSNSS_H_

typedef struct
{
   union
   {
      struct { unsigned char registers[4], latch, numberOfBits; } mapper1;
      struct { unsigned char irqCounter, irqLatchCounter, irqCounterEnabled, last8000Write; } mapper4;
      struct { unsigned char dummy; } mapper5;
      struct { unsigned char latch[2], lastB000Write, lastC000Write, lastD000Write, lastE000Write; } mapper9;
      struct { unsigned char irqCounterLowByte, irqCounterHighByte, irqCounterEnabled; } mapper16;
      struct { unsigned char irqCounterLowByte, irqCounterHighByte, irqCounterEnabled; } mapper18;
      struct { unsigned char irqCounterLowByte, irqCounterHighByte, irqCounterEnabled; } mapper19;
      struct { unsigned char irqCounter, irqCounterEnabled; } mapper21;
      struct { unsigned char irqCounter, irqCounterEnabled; } mapper24;
      struct { unsigned char irqCounter, irqCounterEnabled; } mapper40;
      unsigned char bytes[32];
   } extraData;
} SnssMapperBlock;

#endif
