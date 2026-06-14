    .section .text.start, "ax"
    .global  _start
_start:
    /* Stack pointer to the top of the linker-reserved region */
    la   sp, _stack_top

    /* Zero .bss (word at a time; RV32 so 4-byte stores) */
    la   t0, _bss_start
    la   t1, _bss_end
1:  bgeu t0, t1, 2f
    sw   zero, 0(t0)
    addi t0, t0, 4
    j    1b

2:  call main
hang:
    wfi
    j    hang
