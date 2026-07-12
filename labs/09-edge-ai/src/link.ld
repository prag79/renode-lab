ENTRY(_start)

MEMORY {
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 64M
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
    . = . + 0x4000;          /* 16 KiB stack */
    _stack_top = .;
}
