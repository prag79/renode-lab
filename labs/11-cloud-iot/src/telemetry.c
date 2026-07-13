/* Lab 11 - Cloud IoT: a simulated RISC-V sensor node.
 *
 * The device does the one thing a real edge sensor node does: read
 * (here: synthesize) a measurement and emit it as a compact JSON line
 * on its UART, once per period, forever:
 *
 *   {"device":"renode-sim-01","seq":0,"temp_c":24.7,"humidity":51}
 *
 * It deliberately knows NOTHING about the cloud, TCP/IP, MQTT or TLS.
 * The host-side bridge (tools/bridge.py) reads these lines over a TCP
 * socket and forwards them to AWS IoT Core / Azure IoT Hub and a
 * dashboard -- the classic edge -> gateway -> cloud split.
 */

#include <stdint.h>

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

static void uart_putu(uint32_t v) {
    char buf[12];
    int i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i--) uart_putc(buf[i]);
}

/* Print a value given in tenths as "NN.N" (e.g. 247 -> "24.7"). */
static void uart_put_fixed1(int32_t tenths) {
    if (tenths < 0) { uart_putc('-'); tenths = -tenths; }
    uart_putu((uint32_t)(tenths / 10));
    uart_putc('.');
    uart_putu((uint32_t)(tenths % 10));
}

/* Tiny linear-congruential PRNG so the readings wander a bit. */
static uint32_t rng_state = 0x1234abcdU;
static uint32_t rnd(void) {
    rng_state = rng_state * 1664525U + 1013904223U;
    return rng_state >> 16;
}

/* Rough busy-wait so telemetry streams at a human pace (~1-2 msg/s in
 * simulation). The exact rate is not important for this lab; lower the
 * bound for a faster stream. */
static void delay(void) {
    for (volatile uint32_t i = 0; i < 4000000U; i++) { }
}

int main(void) {
    uart_puts("\n*** Renode IoT node online: streaming telemetry as JSON ***\n");
    uart_puts("Each line is one cloud-ready message for the host bridge.\n");

    uint32_t seq = 0;
    for (;;) {
        int32_t temp_c   = 200 + (int32_t)(rnd() % 101);  /* tenths: 20.0..30.0 */
        int32_t humidity = 40  + (int32_t)(rnd() % 21);   /* 40..60 % */

        uart_puts("{\"device\":\"renode-sim-01\",\"seq\":");
        uart_putu(seq);
        uart_puts(",\"temp_c\":");
        uart_put_fixed1(temp_c);
        uart_puts(",\"humidity\":");
        uart_putu((uint32_t)humidity);
        uart_puts("}\n");

        seq++;
        delay();
    }
    return 0;
}
