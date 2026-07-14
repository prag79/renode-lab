/* Lab 11 (fleet variant) - multi-node IoT -> gateway -> cloud.
 *
 * This is the fusion of lab 08 (many nodes on a shared bus) and lab 11
 * (a node streaming JSON to the host cloud bridge). One C source, two
 * roles, selected at build time via -DNODE_ID:
 *
 *   NODE_ID != 0  -> a SENSOR node. Broadcasts a compact JSON telemetry
 *                    line onto the shared bus, forever:
 *                        {"device":"renode-sim-02","seq":0,"temp_c":25.1,"humidity":48}
 *
 *   NODE_ID == 0  -> the GATEWAY. Listens on the shared bus, and relays
 *                    every complete JSON line out its uplink UART. That
 *                    uplink is wired (in the .resc) to a host TCP socket,
 *                    where tools/bridge.py forwards it to AWS IoT Core /
 *                    Azure IoT Hub + the dashboard.
 *
 * Two NS16550 UARTs per node:
 *   uart0 @ 0x10000000  the gateway's cloud UPLINK (host socket); on a
 *                       sensor it is just a local console.
 *   uart1 @ 0x10001000  the shared BUS ("radio"): a Renode UARTHub
 *                       broadcasts every byte to the uart1 RX of the
 *                       other nodes.
 *
 * The edge nodes stay dumb and know nothing about the cloud, TCP/IP,
 * MQTT or TLS. Only the gateway touches the uplink, and only the host
 * bridge touches the credentials -- the classic edge -> gateway -> cloud
 * split, now with more than one edge node.
 */

#include <stdint.h>

#define UART0_BASE  0x10000000UL     /* gateway uplink / sensor console */
#define UART1_BASE  0x10001000UL     /* shared bus ("radio")            */

/* NS16550 register offsets (DLAB=0). */
#define THR  0                       /* TX holding / RX buffer          */
#define LSR  5                       /* line status                     */
#define LSR_THRE  (1U << 5)          /* TX holding register empty       */
#define LSR_DR    (1U << 0)          /* RX data ready                   */

#define REG(base, off) (*(volatile unsigned char *)((base) + (off)))

#ifndef NODE_ID
#define NODE_ID 0
#endif

/* Each role uses a different subset of the helpers below; silence -Wall
 * for whichever half is compiled out. */
#define MAYBE_UNUSED __attribute__((unused))

MAYBE_UNUSED static void uart_putc(unsigned long base, char c) {
    while (!(REG(base, LSR) & LSR_THRE)) { }
    REG(base, THR) = (unsigned char)c;
}

MAYBE_UNUSED static void uart_puts(unsigned long base, const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc(base, '\r');
        uart_putc(base, *s++);
    }
}

MAYBE_UNUSED static void uart_putu(unsigned long base, uint32_t v) {
    char buf[12];
    int i = 0;
    if (v == 0) { uart_putc(base, '0'); return; }
    while (v) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i--) uart_putc(base, buf[i]);
}

/* Print a value given in tenths as "NN.N" (e.g. 251 -> "25.1"). */
MAYBE_UNUSED static void uart_put_fixed1(unsigned long base, int32_t tenths) {
    if (tenths < 0) { uart_putc(base, '-'); tenths = -tenths; }
    uart_putu(base, (uint32_t)(tenths / 10));
    uart_putc(base, '.');
    uart_putu(base, (uint32_t)(tenths % 10));
}

/* Non-blocking read: returns the byte, or -1 if the RX FIFO is empty. */
MAYBE_UNUSED static int uart_getc(unsigned long base) {
    if (!(REG(base, LSR) & LSR_DR)) return -1;
    return (int)REG(base, THR);
}

/* Tiny linear-congruential PRNG, seeded per node so readings differ. */
MAYBE_UNUSED static uint32_t rng_state = 0x1234abcdU + 0x1000U * NODE_ID;
MAYBE_UNUSED static uint32_t rnd(void) {
    rng_state = rng_state * 1664525U + 1013904223U;
    return rng_state >> 16;
}

MAYBE_UNUSED static void delay(uint32_t loops) {
    for (volatile uint32_t i = 0; i < loops; i++) { }
}

#if NODE_ID == 0
/* ---- Gateway: relay every JSON line from the bus to the uplink. ---- */
int main(void) {
    /* Banner goes out the uplink too; bridge.py ignores non-JSON lines. */
    uart_puts(UART0_BASE, "\n*** IoT gateway online: bus -> cloud uplink ***\n");

    char line[128];
    int len = 0;

    for (;;) {
        int c = uart_getc(UART1_BASE);
        if (c < 0) continue;             /* nothing on the bus yet      */
        if (c == '\r') continue;         /* ignore CR, split on LF      */
        if (c == '\n') {
            line[len] = '\0';
            if (len > 0) {
                uart_puts(UART0_BASE, line);   /* forward to the cloud  */
                uart_puts(UART0_BASE, "\n");
            }
            len = 0;
        } else if (len < (int)sizeof(line) - 1) {
            line[len++] = (char)c;
        }
    }
    return 0;
}
#else
/* ---- Sensor node: broadcast JSON telemetry on the bus, forever. ---- */
int main(void) {
    uart_puts(UART0_BASE, "\n*** IoT sensor node ");
    uart_putu(UART0_BASE, NODE_ID);
    uart_puts(UART0_BASE, " online: broadcasting JSON on the bus ***\n");

    /* Stagger start-up so nodes don't transmit in lock-step and clobber
     * each other on the shared medium every single cycle. */
    delay(600000U * NODE_ID);

    uint32_t seq = 0;
    for (;;) {
        /* Per-node offset so each node's line on the chart is distinct. */
        int32_t temp_c   = 200 + (int32_t)(NODE_ID * 15) + (int32_t)(rnd() % 61);
        int32_t humidity = 40  + (int32_t)(rnd() % 21);

        uart_puts(UART1_BASE, "{\"device\":\"renode-sim-0");
        uart_putu(UART1_BASE, NODE_ID);
        uart_puts(UART1_BASE, "\",\"seq\":");
        uart_putu(UART1_BASE, seq);
        uart_puts(UART1_BASE, ",\"temp_c\":");
        uart_put_fixed1(UART1_BASE, temp_c);
        uart_puts(UART1_BASE, ",\"humidity\":");
        uart_putu(UART1_BASE, (uint32_t)humidity);
        uart_puts(UART1_BASE, "}\n");

        /* Mirror to the local console so this node shows activity too. */
        uart_puts(UART0_BASE, "sent seq=");
        uart_putu(UART0_BASE, seq);
        uart_putc(UART0_BASE, '\n');

        seq++;
        /* Inter-message pacing. Lower this for a faster stream (or raise
         * it to model a low-power sensor that reports less often). */
        delay(1500000U);
    }
    return 0;
}
#endif
