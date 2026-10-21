/* data.bin (tools/pack.py) in the app's read-only data */
#ifdef __APPLE__
  .const_data
  .p2align 2
  .globl _s1_data
_s1_data:
  .incbin DATA_BIN
  .p2align 2
#else
  .section .rodata.s1_data,"a"
  .balign 4
  .global s1_data
s1_data:
  .incbin DATA_BIN
  .balign 4
#endif
#ifdef __ELF__
  .section .note.GNU-stack,"",%progbits
#endif
