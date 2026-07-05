/* Multi-node IoT firmware for a SiFive FE310 (HiFive1).
 *
 * One source, three roles, selected at build time via -DNODE_ID:
 *   NODE_ID == 0  -> the GATEWAY: listens on the shared bus and prints
 *                    every sensor report to its console.
 *   NODE_ID != 0  -> a SENSOR node: periodically broadcasts a fake
 *                    temperature reading onto the shared bus.
 *
 * Two UARTs per node (real FE310-G000 datasheet addresses):
 *   - uart0 @ 0x10013000  the node's local console (analyzer / log).
 *   - uart1 @ 0x10023000  the "radio": wired to a Renode UART hub that
 *                         broadcasts every byte to all other nodes.
 *
 * A UART hub is a shared broadcast medium: whatever one node transmits
 * on uart1 is delivered to the uart1 RX of every OTHER node. That is a
 * faithful stand-in for a multi-drop bus (RS-485) or a single wireless
 * channel that a swarm of IoT nodes shares. */

#include <stdint.h>

#define UART0_BASE 0x10013000UL      /* local console                 */
#define UART1_BASE 0x10023000UL      /* shared network bus ("radio")  */

/* SiFive UART register offsets (same map as lab 04, plus RX side). */
#define TXDATA 0x00                  /* [31]=full,  [7:0]=data         */
#define RXDATA 0x04                  /* [31]=empty, [7:0]=data         */
#define TXCTRL 0x08                  /* [0]=txen                       */
#define RXCTRL 0x0C                  /* [0]=rxen                       */
#define UDIV   0x18                  /* baud divisor (ignored by sim)  */

#define TXFULL  (1U << 31)
#define RXEMPTY (1U << 31)

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

#ifndef NODE_ID
#define NODE_ID 0
#endif

/* The gateway and sensor roles use different subsets of these helpers,
   so mark them unused-safe to keep -Wall quiet for whichever role is
   compiled out. */
#define MAYBE_UNUSED __attribute__((unused))

MAYBE_UNUSED static void uart_init(unsigned long base) {
    REG(base, UDIV)   = 138;         /* ~115200 @ 16 MHz; timing ignored */
    REG(base, TXCTRL) = 1;           /* enable TX */
    REG(base, RXCTRL) = 1;           /* enable RX */
}

MAYBE_UNUSED static void uart_putc(unsigned long base, char c) {
    while (REG(base, TXDATA) & TXFULL) { /* spin while TX FIFO full */ }
    REG(base, TXDATA) = (uint8_t)c;
}

MAYBE_UNUSED static void uart_puts(unsigned long base, const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc(base, '\r');
        uart_putc(base, *s++);
    }
}

MAYBE_UNUSED static void uart_putdec(unsigned long base, uint32_t v) {
    char buf[10];
    int i = 0;
    if (v == 0) { uart_putc(base, '0'); return; }
    while (v) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i--) uart_putc(base, buf[i]);
}

/* Non-blocking read: returns the byte, or -1 if the RX FIFO is empty. */
MAYBE_UNUSED static int uart_getc(unsigned long base) {
    uint32_t v = REG(base, RXDATA);
    if (v & RXEMPTY) return -1;
    return (int)(v & 0xFF);
}

MAYBE_UNUSED static void delay(volatile uint32_t n) {
    while (n--) { __asm__ volatile("nop"); }
}

#if NODE_ID == 0
/* ---- Gateway: collect newline-terminated reports and print them. ---- */
int main(void) {
    uart_init(UART0_BASE);
    uart_init(UART1_BASE);
    uart_puts(UART0_BASE, "\n*** IoT gateway (node 0) online ***\n");
    uart_puts(UART0_BASE, "Listening on the shared bus for sensor reports...\n");

    char line[64];
    int len = 0;

    for (;;) {
        int c = uart_getc(UART1_BASE);
        if (c < 0) continue;             /* nothing received yet */
        if (c == '\r') continue;         /* ignore CR, split on LF */
        if (c == '\n') {
            line[len] = '\0';
            uart_puts(UART0_BASE, "[gateway] report: ");
            uart_puts(UART0_BASE, line);
            uart_putc(UART0_BASE, '\n');
            len = 0;
        } else if (len < (int)sizeof(line) - 1) {
            line[len++] = (char)c;
        }
    }
    return 0;
}
#else
/* ---- Sensor node: broadcast a reading on the bus, forever. ---- */
int main(void) {
    uart_init(UART0_BASE);
    uart_init(UART1_BASE);
    uart_puts(UART0_BASE, "\n*** IoT sensor node ");
    uart_putdec(UART0_BASE, NODE_ID);
    uart_puts(UART0_BASE, " online ***\n");

    /* Stagger the nodes so their reports don't collide on the shared
       bus every single cycle (a real, observable multi-drop problem). */
    delay(2000000U * NODE_ID);

    uint32_t seq = 0;
    for (;;) {
        uint32_t temp = 20 + NODE_ID + (seq % 10);   /* fake reading */

        /* Broadcast one line: "node<id> t=<temp> seq=<seq>\n" */
        uart_puts(UART1_BASE, "node");
        uart_putdec(UART1_BASE, NODE_ID);
        uart_puts(UART1_BASE, " t=");
        uart_putdec(UART1_BASE, temp);
        uart_puts(UART1_BASE, " seq=");
        uart_putdec(UART1_BASE, seq);
        uart_putc(UART1_BASE, '\n');

        /* Local log so this node's own console shows activity too. */
        uart_puts(UART0_BASE, "sent t=");
        uart_putdec(UART0_BASE, temp);
        uart_putc(UART0_BASE, '\n');

        seq++;
        delay(6000000U);
    }
    return 0;
}
#endif
