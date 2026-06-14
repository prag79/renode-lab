ENTRY(_start)

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
