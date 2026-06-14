/* Firmware that drives our hand-written timer IP (SimpleTimer.cs).
 *
 * The timer's IRQ line is wired to the CPU's machine-external-interrupt
 * input (irq 11). So the flow is:
 *   1. program RELOAD with a period and enable the timer + its interrupt
 *   2. enable mie.MEIE (bit 11) and mstatus.MIE, set mtvec
 *   3. sleep in wfi; every period the timer raises its IRQ, we trap,
 *      acknowledge it by writing STATUS, and print a line. */

#include <stdint.h>

/* ---- NS16550 UART ---- */
#define UART_BASE   0x10000000UL
#define UART_THR    (*(volatile unsigned char *)(UART_BASE + 0))
#define UART_LSR    (*(volatile unsigned char *)(UART_BASE + 5))
#define LSR_THRE    (1U << 5)

/* ---- our custom timer IP ---- */
#define TIMER_BASE      0x10001000UL
#define TIMER_CONTROL   (*(volatile uint32_t *)(TIMER_BASE + 0x00))
#define TIMER_RELOAD    (*(volatile uint32_t *)(TIMER_BASE + 0x04))
#define TIMER_COUNTER   (*(volatile uint32_t *)(TIMER_BASE + 0x08))
#define TIMER_STATUS    (*(volatile uint32_t *)(TIMER_BASE + 0x0C))
#define CTRL_ENABLE     (1U << 0)
#define CTRL_IRQ_ENABLE (1U << 1)
#define STATUS_PENDING  (1U << 0)

/* Timer runs at 1 MHz (the C# default), so 1,000,000 ticks ~= 1 s. */
#define PERIOD_TICKS    1000000U

/* ---- CSR helpers ---- */
#define read_csr(reg) ({ unsigned long __v; \
    __asm__ volatile ("csrr %0, " #reg : "=r"(__v)); __v; })
#define write_csr(reg, val) \
    __asm__ volatile ("csrw " #reg ", %0" :: "r"((unsigned long)(val)))
#define set_csr(reg, bits) \
    __asm__ volatile ("csrs " #reg ", %0" :: "r"((unsigned long)(bits)))

#define MSTATUS_MIE (1UL << 3)
#define MIE_MEIE    (1UL << 11)   /* machine external interrupt enable */

static void uart_putc(char c) {
    while (!(UART_LSR & LSR_THRE)) { }
    UART_THR = (unsigned char)c;
}

static void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static volatile unsigned ticks;

/* Machine-mode trap handler. The only interrupt we enable is the timer's
 * external IRQ, so any trap here means "the timer fired". */
void __attribute__((interrupt("machine"), aligned(4))) trap_handler(void) {
    if (TIMER_STATUS & STATUS_PENDING) {
        TIMER_STATUS = STATUS_PENDING;   /* write-1-to-clear; deasserts IRQ */
        ticks++;
        uart_puts("tick from my custom timer IP\n");
    }
}

int main(void) {
    uart_puts("\n*** Custom peripheral lab: SimpleTimer IP ***\n");
    uart_puts("Programming the timer and going to sleep...\n");

    TIMER_RELOAD  = PERIOD_TICKS;
    TIMER_CONTROL = CTRL_ENABLE | CTRL_IRQ_ENABLE;

    write_csr(mtvec, (unsigned long)&trap_handler);
    set_csr(mie, MIE_MEIE);
    set_csr(mstatus, MSTATUS_MIE);

    for (;;) {
        __asm__ volatile ("wfi");
    }
    return 0;
}
