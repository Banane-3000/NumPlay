/* RetroGo.nwa on its own: the table of games keeps its own section, and np_retrogo_end marks
   its end, as NumPlay's numplay.ld marks the end of Retro-Go's block (main.c reads no game past
   it). */
SECTIONS {
  .rodata.np_roms : { KEEP(*(.rodata.np_roms)) np_retrogo_end = .; }
}
