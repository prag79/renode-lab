/* Linker script for the CoralNPU transformer kernel.
 *
 * The Coral NPU has two tightly-coupled memories:
 *   ITCM  (instruction TCM)  at NPU-local 0x00000000  -> host 0xE00000000
 *   DTCM  (data TCM)         at NPU-local 0x00010000  -> host 0xE00010000
 *
 * Code + weights (read-only, initialised) live in ITCM, because that is the
 * only region the host loads when it does `npu LoadBinary @kernel.bin 0`.
 * Working memory (.bss + stack) and the host<->NPU I/O mailbox live in DTCM.
 * The fixed I/O addresses (see npu_transformer.c) sit at the bottom of DTCM;
 * .bss is placed above them.
 */
ENTRY(_start)

MEMORY {
    ITCM (rx) : ORIGIN = 0x00000000, LENGTH = 32K
    DTCM (rw) : ORIGIN = 0x00010000, LENGTH = 32K
}

/* Reserve the first 0x400 bytes of DTCM for the I/O mailbox. */
_dtcm_bss_origin = ORIGIN(DTCM) + 0x400;

SECTIONS {
    .text : {
        KEEP(*(.text.start))
        *(.text*)
    } > ITCM

    .rodata : {
        *(.rodata*)
        *(.srodata*)
    } > ITCM

    . = _dtcm_bss_origin;
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
