/* Edge-AI lab: run a tiny int8-quantized neural network on a bare-metal
 * RISC-V core — no OS, no ML framework, no floating point.
 *
 * The model (weights + a few sample images) lives in the auto-generated
 * model.h. It is a 2-layer MLP for 8x8 handwritten digits:
 *
 *     input (64 pixels, 0..16)
 *        |  W1 [32x64] + b1     <- layer 1
 *        v
 *     ReLU, then >> REQUANT_SHIFT   (requantize int32 -> int8)
 *        |  W2 [10x32] + b2     <- layer 2
 *        v
 *     argmax over 10 logits  ->  predicted digit
 *
 * Everything is integer arithmetic (int8 weights, int32 accumulators),
 * which is exactly how real TinyML "quantized" models run on MCUs with
 * no FPU: it's smaller, faster, and needs no soft-float library. The
 * whole inference is the two matrix-vector products below. */

#include <stdint.h>
#include "model.h"

/* ---- NS16550 UART (same as lab 07) ---- */
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
    char buf[10];
    int i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i--) uart_putc(buf[i]);
}

static void uart_puti(int32_t v) {
    if (v < 0) { uart_putc('-'); uart_putu((uint32_t)(-v)); }
    else uart_putu((uint32_t)v);
}

/* The whole neural network: two integer matrix-vector products.
 * Returns the predicted class; fills out_logits[OUT_DIM]. */
static int infer(const int8_t *x, int32_t *out_logits) {
    int32_t hidden[HID_DIM];

    /* Layer 1: hidden = requantize(ReLU(W1 * x + b1)) */
    for (int j = 0; j < HID_DIM; j++) {
        int32_t acc = model_b1[j];
        const int8_t *w = &model_w1[j * IN_DIM];
        for (int i = 0; i < IN_DIM; i++) {
            acc += (int32_t)w[i] * (int32_t)x[i];
        }
        if (acc < 0) acc = 0;              /* ReLU */
        acc >>= REQUANT_SHIFT;             /* int32 -> int8 range */
        if (acc > 127) acc = 127;
        hidden[j] = acc;
    }

    /* Layer 2: logits = W2 * hidden + b2, then argmax */
    int best = 0;
    int32_t best_val = 0;
    for (int k = 0; k < OUT_DIM; k++) {
        int32_t acc = model_b2[k];
        const int8_t *w = &model_w2[k * HID_DIM];
        for (int j = 0; j < HID_DIM; j++) {
            acc += (int32_t)w[j] * (int32_t)hidden[j];
        }
        out_logits[k] = acc;
        if (k == 0 || acc > best_val) { best_val = acc; best = k; }
    }
    return best;
}

/* Draw the 8x8 input as ASCII art so you can SEE the digit the model
 * is looking at (pixels are 0..16; map to a 10-level brightness ramp). */
static void render(const int8_t *x) {
    static const char ramp[] = " .:-=+*#%@";   /* 10 chars, dark -> bright */
    for (int r = 0; r < 8; r++) {
        uart_puts("    ");
        for (int c = 0; c < 8; c++) {
            int v = x[r * 8 + c];              /* 0..16 */
            int idx = (v * 9) / 16;            /* -> 0..9 */
            uart_putc(ramp[idx]);
            uart_putc(ramp[idx]);              /* double-wide for aspect */
        }
        uart_putc('\n');
    }
}

int main(void) {
    uart_puts("\n*** Edge AI on RISC-V: handwritten-digit recognition ***\n");
    uart_puts("Model: int8-quantized MLP (");
    uart_putu(IN_DIM); uart_puts("->");
    uart_putu(HID_DIM); uart_puts("->");
    uart_putu(OUT_DIM); uart_puts("), integer-only inference, no FPU.\n");
    uart_puts("Classifying "); uart_putu(N_SAMPLES);
    uart_puts(" sample 8x8 digits...\n");

    int32_t logits[OUT_DIM];
    int correct = 0;

    for (int s = 0; s < N_SAMPLES; s++) {
        const int8_t *img = &sample_images[s * IN_DIM];

        uart_puts("\n---- sample "); uart_putu((uint32_t)(s + 1));
        uart_puts(" of "); uart_putu(N_SAMPLES); uart_puts(" ----\n");
        render(img);

        int pred = infer(img, logits);
        int truth = sample_labels[s];

        uart_puts("  prediction = "); uart_putu((uint32_t)pred);
        uart_puts("  (true = "); uart_putu((uint32_t)truth); uart_puts(")  ");
        if (pred == truth) { uart_puts("OK\n"); correct++; }
        else               { uart_puts("MISS\n"); }

        uart_puts("  logits:");
        for (int k = 0; k < OUT_DIM; k++) { uart_putc(' '); uart_puti(logits[k]); }
        uart_putc('\n');
    }

    uart_puts("\n==== accuracy: "); uart_putu((uint32_t)correct);
    uart_puts(" / "); uart_putu(N_SAMPLES); uart_puts(" correct ====\n");
    uart_puts("Inference complete. The CPU is now idle (wfi).\n");

    for (;;) { __asm__ volatile ("wfi"); }
    return 0;
}
