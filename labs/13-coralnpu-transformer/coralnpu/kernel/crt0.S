/* CoralNPU (Kelvin) kernel startup - rv32im, bare metal.
 *
 * Runs ON the Coral NPU core (not the host). Sets up the stack in DTCM,
 * zeroes .bss, calls kernel_main(), then terminates the core with the
 * Kelvin `mpause` instruction so the host sees STATUS.HALTED.
 *
 * `mpause` is a CoralNPU-specific SYSTEM instruction (opcode 0x0B..SYSTEM);
 * from the coralnpu-mpact ISA it encodes to 0x08000073. A stock rv32
 * assembler doesn't know the mnemonic, so we emit the raw word.
 */
    .section .text.start, "ax"
    .global _start
_start:
    la      sp, _stack_top

    /* zero .bss */
    la      t0, _bss_start
    la      t1, _bss_end
1:  bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b

2:  call    kernel_main

_halt:
    .word   0x08000073      /* mpause: terminate the CoralNPU program */
    j       _halt           /* safety net if mpause is ever a no-op   */
