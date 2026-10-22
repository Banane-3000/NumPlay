/* Script of the `ld -r` that makes the kit's base.o (Makefile): with no output section named,
   every input section is an orphan, so --unique keeps each one apart (a library's .text too)
   and the final link of NumPlay.nwa drops those its games don't call. */
SECTIONS {
  .ARM.attributes 0 : { KEEP(*(.ARM.attributes)) }
  /DISCARD/ : { *(.comment) *(.note*) *(.ARM.exidx*) *(.ARM.extab*) }
}
