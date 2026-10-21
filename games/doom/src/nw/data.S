/* The shareware DOOM1.WAD, packed by tools/whd_gen into RP2040 Doom's WHX format: read in place. */
#ifdef __APPLE__ /* (a Mac's computer build: Mach-O's names and sections) */
    .const_data
    .balign 4
    .globl _doom_whd
_doom_whd:
#else
    .section .rodata.doom_whd, "a"
    .balign 4
    .global doom_whd
doom_whd:
#endif
    .incbin DOOM_WHX
