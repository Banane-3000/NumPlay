/* data.bin (tools/pack.py) in the app's read-only data */
#ifdef __APPLE__
  .const_data
  .p2align 3
  .globl _cc_data
_cc_data:
  .incbin DATA_BIN
  .p2align 2
#else
  .section .rodata.cc_data,"a"
  .balign 8
  .global cc_data
cc_data:
  .incbin DATA_BIN
  .balign 4
#endif
#ifdef __ELF__
  .section .note.GNU-stack,"",%progbits
#endif
