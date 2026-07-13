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

    /* Fold small-data (.sdata/.sbss) into the normal data/bss output so
     * nothing lands as a misaligned orphan section. */
    . = ALIGN(8);
    .data : { *(.data*) *(.sdata*) } > RAM

    /* start.S clears .bss with 8-byte `sd` stores, so both ends MUST be
     * 8-byte aligned (and the region a multiple of 8). Without this the
     * store is misaligned and the CPU traps to the reset vector. */
    . = ALIGN(8);
    _bss_start = .;
    .bss : { *(.bss*) *(.sbss*) *(COMMON) } > RAM
    . = ALIGN(8);
    _bss_end = .;

    . = ALIGN(16);
    . = . + 0x4000;          /* 16 KiB stack */
    _stack_top = .;
}
