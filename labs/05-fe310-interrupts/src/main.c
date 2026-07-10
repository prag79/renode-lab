/* Interrupt-driven blink on the SiFive FE310.
 *
 * Instead of a busy-loop (lab 04), the LED is toggled from a machine
 * timer interrupt service routine. This is the real embedded idiom:
 * the CPU sleeps in `wfi` and only wakes when the CLINT fires.
 *
 * The moving parts:
 *   - CLINT mtime / mtimecmp  : when mtime >= mtimecmp, the machine
 *                               timer interrupt (CLINT line 1 -> cpu@7) fires.
 *   - mtvec                   : trap vector base (direct mode).
 *   - mie.MTIE (bit 7)        : enable the machine timer interrupt.
 *   - mstatus.MIE (bit 3)     : global machine interrupt enable.
 *
 * Each ISR re-arms mtimecmp for the next tick, so the interrupt is
 * periodic. */

#include <stdint.h>

/* ---- UART0 (SiFive) ---- */
#define UART0_BASE  0x10013000UL
#define UART_TXDATA (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_TXCTRL (*(volatile uint32_t *)(UART0_BASE + 0x08))
#define UART_DIV    (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UART_TXFULL (1U << 31)

/* ---- GPIO (SiFive) ---- */
#define GPIO_BASE       0x10012000UL
#define GPIO_OUTPUT_EN  (*(volatile uint32_t *)(GPIO_BASE + 0x08))
#define GPIO_OUTPUT_VAL (*(volatile uint32_t *)(GPIO_BASE + 0x0C))
#define LED_PIN     19
#define LED_MASK    (1U << LED_PIN)

/* ---- CLINT ---- */
#define CLINT_BASE  0x02000000UL
#define MTIMECMP_LO (*(volatile uint32_t *)(CLINT_BASE + 0x4000))
#define MTIMECMP_HI (*(volatile uint32_t *)(CLINT_BASE + 0x4004))
#define MTIME_LO    (*(volatile uint32_t *)(CLINT_BASE + 0xBFF8))
#define MTIME_HI    (*(volatile uint32_t *)(CLINT_BASE + 0xBFFC))

/* CLINT frequency from the .repl. One tick every TICK_HZ-th of a second. */
#define CLINT_FREQ  62000000UL
#define TICK_HZ     5
#define INTERVAL    (CLINT_FREQ / TICK_HZ)

/* ---- CSR helpers ---- */
#define read_csr(reg) ({ unsigned long __v; \
    __asm__ volatile ("csrr %0, " #reg : "=r"(__v)); __v; })
#define write_csr(reg, val) \
    __asm__ volatile ("csrw " #reg ", %0" :: "r"((unsigned long)(val)))
#define set_csr(reg, bits) \
    __asm__ volatile ("csrs " #reg ", %0" :: "r"((unsigned long)(bits)))

#define MSTATUS_MIE (1U << 3)
#define MIE_MTIE    (1U << 7)

static void uart_putc(char c) {
    while (UART_TXDATA & UART_TXFULL) { }
    UART_TXDATA = (uint8_t)c;
}

static void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

/* Minimal unsigned-decimal printer (no libc in this bare-metal build). */
static void uart_putu(uint32_t v) {
    char buf[10];
    int i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v > 0) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i > 0) { uart_putc(buf[--i]); }
}

static uint64_t read_mtime(void) {
    uint32_t hi, lo, hi2;
    do { hi = MTIME_HI; lo = MTIME_LO; hi2 = MTIME_HI; } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
}

/* 64-bit mtimecmp write, RV32-safe: park hi at max first so no
 * spurious interrupt fires while the low word is being updated. */
static void set_mtimecmp(uint64_t v) {
    MTIMECMP_HI = 0xffffffffU;
    MTIMECMP_LO = (uint32_t)v;
    MTIMECMP_HI = (uint32_t)(v >> 32);
}

/* The trap handler. The `interrupt("machine")` attribute makes GCC
 * emit the register save/restore and the `mret` for us. We only ever
 * enable the machine timer, so no cause dispatch is needed here.
 *
 * aligned(4): mtvec uses its low 2 bits to select Direct vs Vectored
 * mode, so the handler address must be 4-byte aligned (those bits 0 =
 * Direct). With the C extension GCC might otherwise place it on a
 * 2-byte boundary. */
static uint32_t tick_count;

void __attribute__((interrupt("machine"), aligned(4))) timer_isr(void) {
    /* 1. We got here because mtime >= mtimecmp raised the machine-timer
     *    interrupt (CLINT line 1 -> cpu@7) and the CPU trapped to mtvec. */
    tick_count++;

    /* 2. Re-arm the comparator so the next interrupt fires one INTERVAL
     *    later. Without this the timer would fire once and never again. */
    set_mtimecmp(read_mtime() + INTERVAL);

    /* 3. Do the actual work: toggle the LED on GPIO pin 19. */
    GPIO_OUTPUT_VAL ^= LED_MASK;
    int led_on = (GPIO_OUTPUT_VAL & LED_MASK) != 0;

    /* 4. Narrate what just happened so it's visible on the UART. */
    uart_puts("[IRQ #");
    uart_putu(tick_count);
    uart_puts("] machine-timer fired -> re-armed mtimecmp, LED ");
    uart_puts(led_on ? "ON\n" : "OFF\n");
}

int main(void) {
    UART_DIV    = 138;
    UART_TXCTRL = 1;
    uart_puts("\n*** FE310 timer-interrupt blink ***\n");
    uart_puts("Setup: LED on GPIO pin 19, CLINT machine timer at ");
    uart_putu(TICK_HZ);
    uart_puts(" Hz.\n");
    uart_puts("The CPU sleeps in 'wfi' and only wakes when the timer\n");
    uart_puts("interrupt fires; each wake toggles the LED and prints a\n");
    uart_puts("line below. Watch the [IRQ #N] counter climb.\n\n");

    GPIO_OUTPUT_EN |= LED_MASK;               /* make pin 19 an output */

    set_mtimecmp(read_mtime() + INTERVAL);    /* arm the first deadline */
    write_csr(mtvec, (unsigned long)&timer_isr);  /* trap vector, direct mode */
    set_csr(mie, MIE_MTIE);                   /* enable machine-timer irq (bit 7) */
    set_csr(mstatus, MSTATUS_MIE);            /* global interrupt enable (bit 3) */

    for (;;) {
        __asm__ volatile ("wfi");             /* sleep until the next interrupt */
    }
    return 0;
}
