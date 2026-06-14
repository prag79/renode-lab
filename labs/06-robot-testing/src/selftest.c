/* The "device under test" for the Renode Robot suite.
 *
 * It runs a tiny power-on self-test and narrates each step over the
 * NS16550 UART, finishing with a PASS or FAIL banner. The Robot test
 * (tests/uart.robot) asserts that the right lines appear in the right
 * order - exactly how you'd gate firmware in CI. */

#define UART_BASE   0x10000000UL
#define UART_THR    (*(volatile unsigned char *)(UART_BASE + 0))
#define UART_LSR    (*(volatile unsigned char *)(UART_BASE + 5))
#define LSR_THRE    (1U << 5)

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

/* A couple of trivial checks so the self-test has something to pass. */
static int check_memory(void) {
    volatile unsigned long *scratch = (volatile unsigned long *)(0x80100000UL);
    *scratch = 0xDEADBEEFCAFEF00DUL;
    return *scratch == 0xDEADBEEFCAFEF00DUL;
}

static int check_alu(void) {
    volatile unsigned x = 7, y = 6;
    return (x * y) == 42;
}

int main(void) {
    uart_puts("\n=== Renode CI self-test ===\n");

    int ok = 1;

    uart_puts("step 1: memory ");
    if (check_memory()) { uart_puts("OK\n"); } else { uart_puts("FAIL\n"); ok = 0; }

    uart_puts("step 2: alu ");
    if (check_alu()) { uart_puts("OK\n"); } else { uart_puts("FAIL\n"); ok = 0; }

    if (ok) {
        uart_puts("ALL TESTS PASSED\n");
    } else {
        uart_puts("SELF-TEST FAILED\n");
    }
    return 0;
}
