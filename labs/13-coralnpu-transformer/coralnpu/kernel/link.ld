/* Linker script for the CoralNPU transformer kernel.
 *
 * The Coral NPU is a Harvard core with two tightly-coupled memories:
 *   ITCM  (instruction TCM)  at NPU-local 0x00000000  -> host 0xE00000000
 *   DTCM  (data TCM)         at NPU-local 0x00010000  -> host 0xE00010000
 *
 * IMPORTANT: the ITCM is FETCH-ONLY. Any data load/store into the ITCM
 * range faults the core ("MMU fault - the address ... is not specified in
 * any of the existing ranges"). Only .text may live in ITCM; every byte the
 * program READS or WRITES (.rodata weights, .data, .bss, stack, mailbox)
 * must live in DTCM.
 *
 * The fixed host<->NPU mailbox (see npu_transformer.c) occupies the first
 * 0x400 bytes of DTCM; .rodata/.data/.bss are placed above it.
 */
ENTRY(_start)

MEMORY {
    ITCM (rx) : ORIGIN = 0x00000000, LENGTH = 8K
    DTCM (rw) : ORIGIN = 0x00010000, LENGTH = 32K
}

SECTIONS {
    .text : {
        KEEP(*(.text.start))
        *(.text*)
    } > ITCM

    /* Reserve the first 0x400 bytes of DTCM for the host<->NPU mailbox.
     * A NOLOAD section is the reliable way to do this: it is the first
     * section allocated in the DTCM region, so it pushes everything after
     * it to 0x10400. (Assigning the location counter across MEMORY regions
     * does not work -- ld silently restarts at the region origin.) */
    .mailbox (NOLOAD) : {
        . = . + 0x400;
    } > DTCM

    .rodata : {
        *(.rodata*)
        *(.srodata*)
    } > DTCM
    .data : {
        *(.data*)
        *(.sdata*)
    } > DTCM

    . = ALIGN(8);
    _bss_start = .;
    .bss : {
        *(.bss*)
        *(.sbss*)
        *(COMMON)
    } > DTCM
    . = ALIGN(16);
    _bss_end = .;

    _stack_top = ORIGIN(DTCM) + LENGTH(DTCM);
}
