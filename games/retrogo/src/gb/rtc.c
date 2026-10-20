/* The MBC3's real-time clock. From gnuboy 1.0.3 (rofl0r's tree), GPL-2.0-or-later (see COPYING).
 * Changed for Retro-Go on NumPlay: the day counter has its 9 bits and sets the carry when it
 * passes 511, as on the cartridge; no files (the clock is kept with the battery save). The
 * calculator has no clock of its own for apps: time goes on while the game runs, and the
 * Options can set it, as in Retro-Go. */
#include "defs.h"
#include "mem.h"
#include "rtc.h"

struct rtc rtc;

void rtc_latch(byte b)
{
	if ((rtc.latch ^ b) & b & 1)
	{
		rtc.regs[0] = rtc.s;
		rtc.regs[1] = rtc.m;
		rtc.regs[2] = rtc.h;
		rtc.regs[3] = rtc.d;
		rtc.regs[4] = ((rtc.d>>8)&1) | (rtc.stop<<6) | (rtc.carry<<7);
		rtc.regs[5] = 0xff;
		rtc.regs[6] = 0xff;
		rtc.regs[7] = 0xff;
	}
	rtc.latch = b;
}

void rtc_write(byte b)
{
	if (!(rtc.sel & 8)) return;
	switch (rtc.sel & 7)
	{
	case 0:
		rtc.s = rtc.regs[0] = b;
		while (rtc.s >= 60) rtc.s -= 60;
		break;
	case 1:
		rtc.m = rtc.regs[1] = b;
		while (rtc.m >= 60) rtc.m -= 60;
		break;
	case 2:
		rtc.h = rtc.regs[2] = b;
		while (rtc.h >= 24) rtc.h -= 24;
		break;
	case 3:
		rtc.regs[3] = b;
		rtc.d = (rtc.d & 0x100) | b;
		break;
	case 4:
		rtc.regs[4] = b;
		rtc.d = (rtc.d & 0xff) | ((b&1)<<8);
		rtc.stop = (b>>6)&1;
		rtc.carry = (b>>7)&1;
		break;
	}
}

void rtc_tick()
{
	if (rtc.stop) return;
	if (++rtc.t >= 60)
	{
		if (++rtc.s >= 60)
		{
			if (++rtc.m >= 60)
			{
				if (++rtc.h >= 24)
				{
					if (++rtc.d >= 512)
					{
						rtc.d = 0;
						rtc.carry = 1;
					}
					rtc.h = 0;
				}
				rtc.m = 0;
			}
			rtc.s = 0;
		}
		rtc.t = 0;
	}
}

/* after a save brought the clock back: whatever it held, a time the clock can show */
void rtc_check()
{
	if (rtc.t < 0 || rtc.t >= 60) rtc.t = 0;
	if (rtc.s < 0 || rtc.s >= 60) rtc.s = 0;
	if (rtc.m < 0 || rtc.m >= 60) rtc.m = 0;
	if (rtc.h < 0 || rtc.h >= 24) rtc.h = 0;
	if (rtc.d < 0 || rtc.d >= 512) rtc.d = 0;
	rtc.stop &= 1;
	rtc.carry &= 1;
	rtc.sel &= 0x0f;
}
