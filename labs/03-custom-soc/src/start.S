    .section .text.start, "ax"
    .global  _start
_start:
    /* Set the stack pointer to the top of RAM-allocated stack region */
    la   sp, _stack_top

    /* Zero the .bss section */
    la   t0, _bss_start
    la   t1, _bss_end
1:  bgeu t0, t1, 2f
    sd   zero, 0(t0)
    addi t0, t0, 8
    j    1b

2:  call main
hang:
    wfi
    j hang
