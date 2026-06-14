/*
 * Linker script for the bare-metal ARM Cortex-A9 Demo.
 *
 * Places all code and data into the platform's system RAM, which the
 * platform description (smarttimer_arm.repl) maps at 0x80000000 with a
 * size of 0x04000000 (64 MiB). Defines the symbols start.S relies on:
 * _bss_start, _bss_end, and _stack_top.
 */
ENTRY(_start)

MEMORY {
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 64M
}

SECTIONS {
    . = ORIGIN(RAM);

    .text : {
        KEEP(*(.text.start))   /* _start must come first, at the RAM base */
        *(.text*)
    } > RAM

    .rodata : { *(.rodata*) } > RAM
    .data   : { *(.data*)   } > RAM

    . = ALIGN(4);
    _bss_start = .;
    .bss : { *(.bss*) *(COMMON) } > RAM
    . = ALIGN(4);
    _bss_end = .;

    . = ALIGN(16);
    . = . + 0x4000;            /* 16 KiB stack */
    _stack_top = .;
}
