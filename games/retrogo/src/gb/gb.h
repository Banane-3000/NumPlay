/* The Game Boy and Game Boy Color emulator (gnuboy), as Retro-Go's screens drive it. */
#ifndef GB_H
#define GB_H
#include "../rg.h"

#define GB_WIDTH 160
#define GB_HEIGHT 144

/* the DMG palettes the Options offer, as Retro-Go names them */
enum { GB_PAL_DMG, GB_PAL_POCKET, GB_PAL_LIGHT, GB_PAL_GBC, GB_PAL_SGB, GB_PAL_COUNT };

/* Loads a ROM read in place (it stays where it is); RG_OK or an RG_ERR_ code. */
int gb_load(const uint8_t *data, uint32_t size);
void gb_reset(bool hard);
void gb_run_frame(bool draw);
void gb_set_pad(int rg_pad);
bool gb_is_cgb(void);
void gb_set_palette(int pal);

/* the cartridge's battery RAM (and its clock): NULL when it has no battery */
uint8_t *gb_sram(uint32_t *size);
bool gb_sram_dirty(void);
void gb_sram_saved(void);
bool gb_has_rtc(void);
/* the clock's state, kept with the battery RAM */
void *gb_rtc_state(uint32_t *size);
void gb_rtc_loaded(void);
void gb_get_time(int *d, int *h, int *m, int *s);
void gb_set_time(int d, int h, int m, int s);

/* save states: the parts of the machine, and putting it back together after they were read */
int gb_state_chunks(rg_chunk_t *out);
void gb_state_loaded(void);

/* (for the core) the sound registers, kept but not played */
unsigned char gb_sound_read(unsigned char r);
void gb_sound_write(unsigned char r, unsigned char b);
extern int gb_draw; /* this frame is drawn */
/* the last frame drawn, line by line through rg_line again (under the menus) */
void gb_redraw(void);
#endif
