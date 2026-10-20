/* The NES emulator (nofrendo), as Retro-Go's screens drive it. */
#ifndef NES_CORE_H
#define NES_CORE_H
#include "../rg.h"

#define NES_WIDTH 256
#define NES_HEIGHT 240

/* the palettes the Options offer */
enum { NES_PAL_NOFRENDO, NES_PAL_NTSC, NES_PAL_COUNT };

/* Loads an iNES ROM read in place; RG_OK or an RG_ERR_ code. */
int nes_load(const uint8_t *data, uint32_t size);
void nes_reset_machine(bool hard);
void nes_run_frame(bool draw);
void nes_set_pad(int rg_pad);
void nes_set_palette(int pal);
void nes_set_sprite_limit(bool on);
/* the last frame drawn, line by line through rg_line again (under the menus) */
void nes_redraw(void);
int nes_mapper(void);

/* the cartridge's battery RAM: NULL when it has no battery */
uint8_t *nes_sram(uint32_t *size);
bool nes_sram_dirty(void);
void nes_sram_saved(void);

int nes_state_chunks(rg_chunk_t *out);
void nes_state_loaded(void);
#endif
