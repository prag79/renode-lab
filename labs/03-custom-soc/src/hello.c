/* Bare-metal hello world for our mini-rv SoC. Talks directly to the
 * NS16550 UART at 0x10000000 (matches QEMU virt + our Renode platform). */

#define UART_BASE   0x10000000UL

#define UART_THR    (*(volatile unsigned char *)(UART_BASE + 0))   /* TX hold */
#define UART_LSR    (*(volatile unsigned char *)(UART_BASE + 5))   /* line status */
#define LSR_THRE    (1U << 5)

static void uart_putc(char c) {
    while (!(UART_LSR & LSR_THRE)) { /* spin until TX empty */ }
    UART_THR = (unsigned char)c;
}

static void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

int main(void) {
    uart_puts("\n*** Hello from custom RV64 SoC! ***\n");
    uart_puts("UART @ 0x10000000, RAM @ 0x80000000\n");
    return 0;
}
