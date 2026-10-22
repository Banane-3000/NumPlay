/* The Game Boy as Retro-Go runs it: loading a cartridge read in place, a frame at a time, the
 * pad, the battery RAM and the parts of a save state. Built on gnuboy 1.0.3 (rofl0r's tree) by
 * Laguna and Gilgamesh, GPL-2.0-or-later: the cartridge header tables and the frame loop follow
 * its loader.c and emu.c. The sound registers are kept (games read them back) but not played:
 * the calculator has no speaker. */
#include <stddef.h>
#include <string.h>
#include "defs.h"
#include "regs.h"
#include "hw.h"
#include "cpu.h"
#include "mem.h"
#include "lcd.h"
#include "rtc.h"
#include "gb.h"

/* cartridge types (header byte 0x147): the bank controller, a battery, a clock */
static const byte mbc_table[256] = {
	0, 1, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 3,
	3, 3, 3, 3, 0, 0, 0, 0, 0, 5, 5, 5, MBC_RUMBLE, MBC_RUMBLE, MBC_RUMBLE, 0,
	[0xFE] = MBC_HUC3, [0xFF] = MBC_HUC1,
};
static const byte batt_table[256] = {
	0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1,
	1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
	[0xFE] = 1, [0xFF] = 1,
};
/* the cartridge RAM's size (header byte 0x149), in bytes, as the game sees it */
static const un32 ram_bytes[6] = {0, 2048, 8192, 32768, 131072, 65536};

static int rom_type, rom_banks, ram_banks, sram_size, has_rtc;
static byte sound_regs[0x30];

int gb_load(const uint8_t *data, uint32_t size)
{
	if (!data || size < 0x8000 || size > 8u << 20) return RG_ERR_ROM;
	int c = data[0x147];
	rom_type = mbc_table[c];
	if (rom_type == MBC_NONE && c != 0x00 && c != 0x08 && c != 0x09) return RG_ERR_MAPPER;
	/* every bank the header names, the ones the file lacks repeating the others; a power of two
	   so that a bank number can always be masked */
	int have = (int)(size / 16384), want = data[0x148] <= 8 ? 2 << data[0x148] : have;
	if (want < have) want = have;
	rom_banks = 2;
	while (rom_banks < want) rom_banks <<= 1;
	if (rom_banks > ROM_MAX_BANKS) return RG_ERR_ROM;
	memset(&rom, 0, sizeof rom);
	for (int i = 0; i < rom_banks; i++) rom.bank[i] = data + (i % have) * 16384;
	memcpy(rom.name, data + 0x134, 16);

	int rcode = data[0x149] < 6 ? data[0x149] : 0;
	sram_size = rom_type == MBC_MBC2 ? 512 : (int)ram_bytes[rcode];
	if (sram_size > 32768) return RG_ERR_MEMORY;
	ram_banks = sram_size > 8192 ? sram_size / 8192 : 1;
	has_rtc = (c == 0x0F || c == 0x10) && rom_type == MBC_MBC3;

	memset(&hw, 0, sizeof hw);
	hw.cgb = (data[0x143] & 0x80) != 0;
	rg_alloc_reset();
	memset(&ram, 0, sizeof ram);
	memset(&lcd, 0, sizeof lcd);
	ram.ibank = rg_alloc(4096 * (hw.cgb ? 8 : 2));
	lcd.vbank[0] = rg_alloc(8192 * (hw.cgb ? 2 : 1));
	lcd.vbank[1] = hw.cgb ? lcd.vbank[0] + 8192 : lcd.vbank[0];
	ram.sbank = rom_type == MBC_NONE && !sram_size ? NULL : rg_alloc(8192 * ram_banks);
	if (!ram.ibank || !lcd.vbank[0] || (!ram.sbank && (rom_type != MBC_NONE || sram_size)))
		return RG_ERR_MEMORY;

	memset(&mbc, 0, sizeof mbc);
	mbc.type = rom_type;
	mbc.batt = batt_table[c];
	mbc.romsize = rom_banks;
	mbc.ramsize = ram_banks;
	memset(&rtc, 0, sizeof rtc);
	rtc.batt = has_rtc;
	if (ram.sbank) memset(ram.sbank, 0xFF, 8192 * ram_banks); /* (a new cartridge's RAM) */
	gb_reset(true);
	return RG_OK;
}

void gb_reset(bool hard)
{
	if (hard)
	{
		memset(ram.ibank, 0, 4096 * (hw.cgb ? 8 : 2));
		memset(&cpu, 0, sizeof cpu);
	}
	hw_reset();
	lcd_reset();
	cpu_reset();
	mbc_reset();
	memset(sound_regs, 0, sizeof sound_regs);
	sound_regs[0x26 - 0x10] = 0xF1; /* (on at power up) */
}

bool gb_is_cgb(void) { return hw.cgb != 0; }

void gb_set_pad(int p)
{
	int pad = 0;
	if (p & RG_PAD_RIGHT) pad |= PAD_RIGHT;
	if (p & RG_PAD_LEFT) pad |= PAD_LEFT;
	if (p & RG_PAD_UP) pad |= PAD_UP;
	if (p & RG_PAD_DOWN) pad |= PAD_DOWN;
	if (p & RG_PAD_A) pad |= PAD_A;
	if (p & RG_PAD_B) pad |= PAD_B;
	if (p & RG_PAD_SELECT) pad |= PAD_SELECT;
	if (p & RG_PAD_START) pad |= PAD_START;
	if (pad == hw.pad) return;
	hw.pad = (byte)pad;
	pad_refresh();
}

/* One frame, from line 0 to the end of the vertical blank (gnuboy's emu_run). */
void gb_run_frame(bool draw)
{
	/* (never more than a few frames' time, whatever a save state left: LY only moves with the LCD on) */
	int budget = 4 * 35112;
	gb_draw = draw;
	budget -= cpu_emulate(2280);
	while ((R_LCDC & 0x80) && R_LY > 0 && R_LY < 144 && budget > 0)
		budget -= cpu_emulate(cpu.lcdc);
	rtc_tick();
	if (!(R_LCDC & 0x80))
		cpu_emulate(32832);
	while ((R_LCDC & 0x80) && R_LY > 0 && budget > 0)
		budget -= cpu_emulate(cpu.lcdc);
}

/* ------------------------------------------------------------ sound registers */
/* the bits that read back as 1 (Pan Docs) */
static const byte sound_mask[0x30] = {
	0x80, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0x3F, 0x00, 0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,
	0xFF, 0x00, 0x00, 0xBF, 0x00, 0x00, 0x70, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

unsigned char gb_sound_read(unsigned char r)
{
	if (r < 0x10 || r >= 0x40) return 0xFF;
	if (r == 0x26) return (sound_regs[0x16] & 0x80) | 0x70; /* on, no channel playing */
	return sound_regs[r - 0x10] | sound_mask[r - 0x10];
}

void gb_sound_write(unsigned char r, unsigned char b)
{
	if (r < 0x10 || r >= 0x40) return;
	sound_regs[r - 0x10] = b;
}

/* ------------------------------------------------------------ battery */
uint8_t *gb_sram(uint32_t *size)
{
	*size = (uint32_t)sram_size;
	return mbc.batt && ram.sbank ? ram.sbank[0] : NULL;
}

bool gb_sram_dirty(void) { return mbc.batt && ram.sram_dirty; }
void gb_sram_saved(void) { ram.sram_dirty = 0; }
bool gb_has_rtc(void) { return has_rtc; }

void *gb_rtc_state(uint32_t *size)
{
	*size = sizeof rtc;
	return &rtc;
}

void gb_rtc_loaded(void)
{
	rtc.batt = has_rtc;
	rtc_check();
}

void gb_get_time(int *d, int *h, int *m, int *s)
{
	if (d) *d = rtc.d;
	if (h) *h = rtc.h;
	if (m) *m = rtc.m;
	if (s) *s = rtc.s;
}

void gb_set_time(int d, int h, int m, int s)
{
	rtc.d = d, rtc.h = h, rtc.m = m, rtc.s = s, rtc.t = 0;
	rtc_check();
}

/* ------------------------------------------------------------ save states */
int gb_state_chunks(rg_chunk_t *out)
{
	int n = 0;
	out[n++] = (rg_chunk_t){&cpu, sizeof cpu};
	out[n++] = (rg_chunk_t){&hw, sizeof hw};
	out[n++] = (rg_chunk_t){&mbc, offsetof(struct mbc, rmap)};
	out[n++] = (rg_chunk_t){&rtc, sizeof rtc};
	out[n++] = (rg_chunk_t){sound_regs, sizeof sound_regs};
	out[n++] = (rg_chunk_t){ram.hi, sizeof ram.hi};
	out[n++] = (rg_chunk_t){LCD_STATE_AT(lcd), LCD_STATE_SIZE};
	out[n++] = (rg_chunk_t){ram.ibank, 4096 * (hw.cgb ? 8 : 2)};
	out[n++] = (rg_chunk_t){lcd.vbank[0], 8192 * (hw.cgb ? 2 : 1)};
	if (ram.sbank) out[n++] = (rg_chunk_t){ram.sbank, 8192 * ram_banks};
	return n;
}

/* whatever a state held, a machine this cartridge can be */
void gb_state_loaded(void)
{
	/* (the registers are 16 bits: the words above them stay clear) */
	union reg *regs[6] = {&cpu.pc, &cpu.sp, &cpu.bc, &cpu.de, &cpu.hl, &cpu.af};
	for (int i = 0; i < 6; i++) regs[i]->w[HI] = 0;
	cpu.af.b[LO][LO] &= 0xF0; /* (F's low bits are always 0) */
	hw.cgb = (rom.bank[0][0x143] & 0x80) != 0;
	hw.gba = 0;
	hw.pad = 0;
	mbc.type = rom_type;
	mbc.romsize = rom_banks;
	mbc.ramsize = ram_banks;
	mbc.batt = batt_table[rom.bank[0][0x147]];
	cpu.speed &= hw.cgb ? 1 : 0;
	cpu.ime &= 1;
	cpu.ima &= 1;
	if (cpu.lcdc < -1000 || cpu.lcdc > 1000) cpu.lcdc = 40;
	cpu.div &= 0xFF;
	cpu.tim &= 0x1FF;
	rtc.batt = has_rtc;
	rtc_check();
	lcd.wline &= 0xFF;
	lcd.wy_hit &= 1;
	if (!(R_LCDC & 0x80)) R_LY = 0; /* (the LCD off holds line 0) */
	mem_updatemap();
	pal_dirty();
	ram.sram_dirty = 1;
}
