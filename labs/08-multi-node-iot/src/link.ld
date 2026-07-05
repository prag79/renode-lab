ENTRY(_start)

/* FE310 on-chip RAM (DTIM) is only 16 KiB. Everything lives here:
   code, data, and the stack carved out at the top. Each of the three
   nodes runs its own copy of this image in its own machine. */
MEMORY {
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 16K
}

SECTIONS {
    . = ORIGIN(RAM);

    .text : {
        KEEP(*(.text.start))
        *(.text*)
    } > RAM

    .rodata : { *(.rodata*) } > RAM
    .data   : { *(.data*)   } > RAM

    _bss_start = .;
    .bss : { *(.bss*) *(COMMON) } > RAM
    _bss_end = .;

    . = ALIGN(16);
    . = . + 0x1000;          /* 4 KiB stack */
    _stack_top = .;
}
