    .section .text.start, "ax"
    .global  _start
_start:
    la   sp, _stack_top

    la   t0, _bss_start
    la   t1, _bss_end
1:  bgeu t0, t1, 2f
    sd   zero, 0(t0)
    addi t0, t0, 8
    j    1b

2:  call main
hang:
    wfi
    j    hang
