/* Bare-metal blink + hello for the SiFive FE310 (HiFive1).
 *
 * Two real on-chip peripherals, at their real datasheet addresses:
 *   - SiFive UART0 @ 0x10013000  (NOT an NS16550 - different register map)
 *   - SiFive GPIO  @ 0x10012000
 *
 * We drive GPIO pin 19 (the green LED on a physical HiFive1 rev B)
 * and narrate every transition over the UART so you can watch the
 * "blink" even without a GUI. */

#include <stdint.h>

#define UART0_BASE  0x10013000UL
#define GPIO_BASE   0x10012000UL

/* SiFive UART register offsets */
#define UART_TXDATA (*(volatile uint32_t *)(UART0_BASE + 0x00)) /* [31]=full, [7:0]=data */
#define UART_TXCTRL (*(volatile uint32_t *)(UART0_BASE + 0x08)) /* [0]=txen */
#define UART_DIV    (*(volatile uint32_t *)(UART0_BASE + 0x18)) /* baud divisor */
#define UART_TXFULL (1U << 31)

/* SiFive GPIO register offsets */
#define GPIO_OUTPUT_EN  (*(volatile uint32_t *)(GPIO_BASE + 0x08))
#define GPIO_OUTPUT_VAL (*(volatile uint32_t *)(GPIO_BASE + 0x0C))

#define LED_PIN     19          /* green LED on HiFive1 rev B */
#define LED_MASK    (1U << LED_PIN)

static void uart_init(void) {
    UART_DIV   = 138;           /* ~115200 @ 16 MHz; Renode ignores timing */
    UART_TXCTRL = 1;            /* enable TX */
}

static void uart_putc(char c) {
    while (UART_TXDATA & UART_TXFULL) { /* spin while TX FIFO full */ }
    UART_TXDATA = (uint8_t)c;
}

static void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static void delay(volatile uint32_t n) {
    while (n--) { __asm__ volatile("nop"); }
}

int main(void) {
    uart_init();
    uart_puts("\n*** Hello from a SiFive FE310 (HiFive1)! ***\n");
    uart_puts("UART0 @ 0x10013000, GPIO @ 0x10012000, RAM @ 0x80000000\n");

    GPIO_OUTPUT_EN |= LED_MASK; /* make pin 19 an output */

    for (;;) {
        GPIO_OUTPUT_VAL |= LED_MASK;
        uart_puts("LED on\n");
        delay(200000);

        GPIO_OUTPUT_VAL &= ~LED_MASK;
        uart_puts("LED off\n");
        delay(200000);
    }
    return 0;
}
