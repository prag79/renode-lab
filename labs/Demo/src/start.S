/*
 * Reset / startup code for the bare-metal ARM Cortex-A9 Demo.
 *
 * After reset the core comes up in Supervisor (SVC) mode with no
 * valid stack and an uninitialized .bss. This stub sets the stack
 * pointer, clears .bss, jumps to main(), and parks the core if main
 * ever returns. The symbols _stack_top, _bss_start and _bss_end are
 * provided by the linker script (src/link.ld).
 */
    .section .text.start, "ax"
    .global  _start
_start:
    /* Point the stack pointer at the top of the linker-reserved region */
    ldr  sp, =_stack_top

    /* Zero the .bss section, one 32-bit word at a time */
    ldr  r0, =_bss_start
    ldr  r1, =_bss_end
    mov  r2, #0
1:  cmp  r0, r1
    bcs  2f                 /* r0 >= r1 (unsigned)? done */
    str  r2, [r0], #4       /* *r0 = 0; r0 += 4 */
    b    1b

2:  bl   main               /* enter C */
hang:
    wfi                     /* nothing left to do; idle the core */
    b    hang
