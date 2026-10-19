/* The shareware DOOM1.WAD, packed by tools/whd_gen into RP2040 Doom's WHX format: read in place. */
    .section .rodata.doom_whd, "a"
    .balign 4
    .global doom_whd
doom_whd:
    .incbin DOOM_WHX
