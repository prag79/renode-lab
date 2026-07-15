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

    . = ALIGN(8);
    .data : { *(.data*) *(.sdata*) } > RAM

    . = ALIGN(8);
    _bss_start = .;
    .bss : { *(.bss*) *(.sbss*) *(COMMON) } > RAM
    . = ALIGN(8);
    _bss_end = .;

    . = ALIGN(16);
    . = . + 0x4000;
    _stack_top = .;
}
