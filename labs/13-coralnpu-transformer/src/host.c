/* Lab 13 - Transformer robot policy accelerated by a Coral NPU.
 *
 * An end-to-end robot controller (lab 12's shape, transformer brain, real
 * accelerator):
 *
 *   command string  ->  TOKENIZE  into vocabulary token ids
 *                   ->  POLICY    an int8 transformer encoder picks a robot
 *                                 INTENT. This runs ON the Coral NPU when a
 *                                 real kernel is loaded (functional offload);
 *                                 otherwise it falls back to the host CPU.
 *                   ->  PLAN      expand the intent into low-level SKILLS
 *                   ->  DRIVE     write each skill to a memory-mapped actuator
 *
 * FUNCTIONAL OFFLOAD. When `coralnpu/kernel/npu_transformer.bin` is loaded
 * into the NPU's ITCM (see the .resc / tools/build_kernel.sh), the WHOLE
 * transformer executes on the Coral core: the host stages the tokenized
 * command into the DTCM mailbox, releases the NPU from reset, waits for
 * STATUS.HALTED (the kernel ends with Kelvin `mpause`), and reads the
 * predicted intent back from DTCM. The host CPU only tokenizes and drives
 * the actuator. If the loaded kernel is the plain "add" sample (no mailbox
 * protocol), the host detects that and runs the policy itself instead.
 *
 * Integer only (int8 weights, int32 accumulate, integer-softmax LUT): no
 * FPU, no ML framework, no network.
 */

#include <stdint.h>
#include "model.h"

/* ---- Console UART (16550) ------------------------------------------- */
#define UART_BASE   0x10000000UL
#define UART_THR    (*(volatile unsigned char *)(UART_BASE + 0))
#define UART_LSR    (*(volatile unsigned char *)(UART_BASE + 5))
#define LSR_THRE    (1U << 5)

/* ---- Memory-mapped robot actuator (peripherals/RobotActuator.cs) ----- */
#define ACT_BASE    0x90000000UL
#define ACT_SKILL   0x00
#define ACT_PARAM0  0x04
#define ACT_PARAM1  0x08
#define ACT_STEP    0x0C
#define ACT_REG(o)  (*(volatile uint32_t *)(ACT_BASE + (o)))

/* ---- Coral NPU (CPU.CoralNPU): CSR + TCM windows on the host bus -----
 *   ITCM 0xE00000000  (kernel loaded here)
 *   DTCM 0xE00010000  (host<->NPU mailbox; matches npu_transformer.c)
 *   CSR  0xE00030000  (RESET_CONTROL 0x0 / STATUS 0x8)
 */
#define NPU_BASE        0xE00000000UL
#define NPU_DTCM        (NPU_BASE + 0x10000UL)
#define NPU_CSR         (NPU_BASE + 0x30000UL)
#define CSR_RESET_CTRL  0x00
#define CSR_STATUS      0x08
#define NPU_CSR_REG(o)  (*(volatile uint32_t *)(NPU_CSR + (o)))

/* DTCM mailbox (host view), identical layout to the kernel's DTCM_BASE. */
#define MB_LEN          (*(volatile uint32_t *)(NPU_DTCM + 0x000))
#define MB_TOKENS       ((volatile uint32_t *)(NPU_DTCM + 0x004))
#define MB_INTENT       (*(volatile uint32_t *)(NPU_DTCM + 0x100))
#define MB_LOGITS       ((volatile int32_t *)(NPU_DTCM + 0x104))
#define MB_DONE         (*(volatile uint32_t *)(NPU_DTCM + 0x200))
#define DONE_MAGIC      0x00C0DE00u

/* ---- UART helpers --------------------------------------------------- */
static void uart_putc(char c) { while (!(UART_LSR & LSR_THRE)) { } UART_THR = (unsigned char)c; }
static void uart_puts(const char *s) { while (*s) { if (*s == '\n') uart_putc('\r'); uart_putc(*s++); } }
static void uart_putu(uint32_t v) {
    char b[12]; int i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i--) uart_putc(b[i]);
}
static void uart_puti(int32_t v) { if (v < 0) { uart_putc('-'); v = -v; } uart_putu((uint32_t)v); }

static uint32_t g_macs;   /* host-side MAC counter (fallback path)        */

/* =====================================================================
 * Step 1: tokenize a command string into vocabulary token ids.
 * ===================================================================== */
static int slice_eq(const char *s, int len, const char *w) {
    int i = 0;
    for (; i < len; i++) if (w[i] == '\0' || s[i] != w[i]) return 0;
    return w[i] == '\0';
}
static int tokenize(const char *cmd, int tokens[SEQ_LEN]) {
    int n = 0;
    const char *p = cmd;
    while (*p && n < SEQ_LEN) {
        while (*p == ' ') p++;
        const char *start = p;
        while (*p && *p != ' ') p++;
        int len = (int)(p - start);
        if (len > 0)
            for (int id = 1; id < VOCAB_SIZE; id++)
                if (slice_eq(start, len, vocab[id])) { tokens[n++] = id; break; }
    }
    return n;
}

/* =====================================================================
 * Step 2a: the transformer policy on the HOST (fallback + reference).
 * ===================================================================== */
static void matvec_rc(const int8_t *W, int cols, int rows,
                      const int32_t *x, int32_t *y) {
    for (int r = 0; r < rows; r++) {
        int32_t acc = 0;
        const int8_t *w = W + (long)r * cols;
        for (int c = 0; c < cols; c++) { acc += (int32_t)w[c] * x[c]; g_macs++; }
        y[r] = acc;
    }
}
static void attention(int32_t x[SEQ_LEN][D_MODEL], int len,
                      int32_t out[SEQ_LEN][D_MODEL]) {
    int32_t q[SEQ_LEN][D_MODEL], k[SEQ_LEN][D_MODEL], v[SEQ_LEN][D_MODEL];
    for (int i = 0; i < len; i++) {
        matvec_rc(&W_q[0][0], D_MODEL, D_MODEL, x[i], q[i]);
        matvec_rc(&W_k[0][0], D_MODEL, D_MODEL, x[i], k[i]);
        matvec_rc(&W_v[0][0], D_MODEL, D_MODEL, x[i], v[i]);
    }
    for (int i = 0; i < len; i++) {
        int32_t score[SEQ_LEN], best = -2147483647;
        for (int j = 0; j < len; j++) {
            int32_t s = 0;
            for (int d = 0; d < D_MODEL; d++) { s += q[i][d] * k[j][d]; g_macs++; }
            score[j] = s;
            if (s > best) best = s;
        }
        int32_t w[SEQ_LEN], sum = 0;
        for (int j = 0; j < len; j++) {
            int32_t diff = best - score[j];
            if (diff < 0) diff = 0;
            if (diff >= SOFTMAX_LUT_SIZE) diff = SOFTMAX_LUT_SIZE - 1;
            w[j] = softmax_lut[diff];
            sum += w[j];
        }
        if (sum == 0) sum = 1;
        int32_t ctx[D_MODEL];
        for (int d = 0; d < D_MODEL; d++) {
            int32_t acc = 0;
            for (int j = 0; j < len; j++) acc += (w[j] * 128 / sum) * v[j][d];
            ctx[d] = acc >> 7;
        }
        matvec_rc(&W_o[0][0], D_MODEL, D_MODEL, ctx, out[i]);
    }
}
static void ffn(const int32_t x[D_MODEL], int32_t out[D_MODEL]) {
    int32_t h[D_FF];
    for (int f = 0; f < D_FF; f++) {
        int32_t acc = 0;
        for (int d = 0; d < D_MODEL; d++) { acc += W_ff1[d][f] * x[d]; g_macs++; }
        h[f] = acc > 0 ? acc : 0;
    }
    for (int d = 0; d < D_MODEL; d++) {
        int32_t acc = 0;
        for (int f = 0; f < D_FF; f++) { acc += W_ff2[f][d] * h[f]; g_macs++; }
        out[d] = acc >> 1;
    }
}
static int host_policy(const int *tokens, int len, int32_t logits[N_CLASSES]) {
    if (len == 0) len = 1;
    int32_t x[SEQ_LEN][D_MODEL];
    for (int i = 0; i < len; i++)
        for (int d = 0; d < D_MODEL; d++) x[i][d] = embedding[tokens[i]][d];
    int32_t att[SEQ_LEN][D_MODEL];
    attention(x, len, att);
    for (int i = 0; i < len; i++)
        for (int d = 0; d < D_MODEL; d++) att[i][d] += x[i][d];
    int32_t pooled[D_MODEL];
    for (int d = 0; d < D_MODEL; d++) pooled[d] = 0;
    for (int i = 0; i < len; i++) {
        int32_t fo[D_MODEL];
        ffn(att[i], fo);
        for (int d = 0; d < D_MODEL; d++) pooled[d] += att[i][d] + fo[d];
    }
    for (int d = 0; d < D_MODEL; d++) pooled[d] /= len;
    int best = 0;
    for (int c = 0; c < N_CLASSES; c++) {
        int32_t acc = 0;
        for (int d = 0; d < D_MODEL; d++) { acc += W_cls[d][c] * pooled[d]; g_macs++; }
        logits[c] = acc;
        if (acc > logits[best]) best = c;
    }
    return best;
}

/* =====================================================================
 * Step 2b: run the transformer policy ON the Coral NPU (functional offload).
 * ===================================================================== */
static int npu_run(void) {
    NPU_CSR_REG(CSR_RESET_CTRL) = 0x3;   /* hold in reset, clock gated */
    NPU_CSR_REG(CSR_RESET_CTRL) = 0x1;   /* release clock gate         */
    NPU_CSR_REG(CSR_RESET_CTRL) = 0x0;   /* release reset -> executes   */
    for (uint32_t spin = 0; spin < 4000000; spin++) {
        uint32_t st = NPU_CSR_REG(CSR_STATUS);
        if (st & 0x2) return 2;          /* FAULT  */
        if (st & 0x1) return 1;          /* HALTED */
    }
    return 0;                            /* timeout */
}

/* Stage tokens into DTCM, run the NPU kernel, read the intent back.
 * Returns 1 on success (kernel wrote the DONE magic), else 0. */
static int npu_infer(const int *tokens, int len, int32_t logits[N_CLASSES]) {
    MB_LEN = (uint32_t)len;
    for (int i = 0; i < len && i < SEQ_LEN; i++) MB_TOKENS[i] = (uint32_t)tokens[i];
    MB_DONE = 0;

    if (npu_run() != 1) return -1;
    if (MB_DONE != DONE_MAGIC) return -1;   /* stub kernel, not our transformer */

    for (int c = 0; c < N_CLASSES; c++) logits[c] = MB_LOGITS[c];
    return (int)MB_INTENT;
}

/* =====================================================================
 * Step 3: low-level skill library + per-intent plans.
 * ===================================================================== */
enum { WALK = 1, TURN = 2, STOP = 3, DOCK = 4 };
typedef struct { uint8_t id; int32_t p0; int32_t p1; } skill_t;

static const skill_t plan_advance[] = {{WALK, 100, 300}, {STOP, 0, 0}};
static const skill_t plan_retreat[] = {{WALK, -50, 200}, {STOP, 0, 0}};
static const skill_t plan_turn[]    = {{TURN, 180, 0},   {STOP, 0, 0}};
static const skill_t plan_home[]    = {{TURN, 180, 0}, {WALK, 80, 500}, {DOCK, 0, 0}};
static const skill_t *const plans[N_CLASSES] = {
    plan_advance, plan_retreat, plan_turn, plan_home,
};
static const int plan_len[N_CLASSES] = {2, 2, 2, 3};

static const char *skill_name(uint8_t id) {
    switch (id) {
    case WALK: return "WALK";
    case TURN: return "TURN";
    case STOP: return "STOP";
    case DOCK: return "DOCK";
    default:   return "?";
    }
}

/* Step 4: drive the actuator + narrate each skill. */
static void drive(int step, const skill_t *s) {
    ACT_REG(ACT_SKILL)  = s->id;
    ACT_REG(ACT_PARAM0) = (uint32_t)s->p0;
    ACT_REG(ACT_PARAM1) = (uint32_t)s->p1;
    ACT_REG(ACT_STEP)   = (uint32_t)step;
    uart_puts("    ["); uart_putu((uint32_t)step); uart_puts("] ");
    uart_puts(skill_name(s->id));
    switch (s->id) {
    case WALK:
        uart_puts("  vel="); uart_puti(s->p0); uart_puts("cm/s dist=");
        uart_puti(s->p1); uart_puts("cm");
        if (s->p0 < 0) uart_puts("  (reverse)");
        break;
    case TURN:
        uart_puts("  angle="); uart_puti(s->p0); uart_puts("deg");
        break;
    default: break;
    }
    uart_putc('\n');
}

/* =====================================================================
 * main: the robot loop.
 * ===================================================================== */
int main(void) {
    uart_puts("\n*** Transformer robot policy accelerated by a Coral NPU ***\n");
    uart_puts("command -> tokenize -> int8 transformer -> intent -> skills -> actuator\n");
    uart_puts("Host: bare-metal RV64. Policy engine: CPU.CoralNPU (rv32im kernel).\n");

    /* Probe: does a real transformer kernel answer on the NPU? (The kernel
     * writes DONE_MAGIC; the plain add sample does not.) */
    int32_t probe_logits[N_CLASSES];
    int probe_tokens[1] = {1};
    int npu_online = (npu_infer(probe_tokens, 1, probe_logits) >= 0);
    uart_puts(npu_online
        ? "\nCoral NPU: transformer kernel online -> policy runs ON the NPU.\n"
        : "\nCoral NPU: no transformer kernel (build it: tools/build_kernel.sh)\n"
          "           -> falling back to the host CPU for the policy.\n");

    /* --- Head-to-head on one command: host CPU vs Coral NPU ------------ */
    {
        int tk[SEQ_LEN];
        int len = tokenize(sample_commands[0], tk);
        int32_t lg[N_CLASSES];

        uart_puts("\n=== where does the transformer run? command: \"");
        uart_puts(sample_commands[0]); uart_puts("\" ===\n");

        uart_puts("# host_policy_start\n");
        g_macs = 0;
        int hi = host_policy(tk, len, lg);
        uart_puts("# host_policy_done\n");
        uart_puts("host CPU  : intent "); uart_puts(intent_names[hi]);
        uart_puts(", "); uart_putu(g_macs); uart_puts(" MACs on the host\n");

        if (npu_online) {
            uart_puts("# npu_infer_start\n");
            int ni = npu_infer(tk, len, lg);
            uart_puts("# npu_infer_done\n");
            uart_puts("Coral NPU : intent "); uart_puts(intent_names[ni]);
            uart_puts(ni == hi ? "  (matches host)\n" : "  (MISMATCH!)\n");
            uart_puts("            host did almost no work; the matmuls ran on the NPU.\n");
        }
    }

    /* --- Robot loop: route every command, expand to skills, actuate ---- */
    uart_puts("\n=== executing commands ===\n");
    for (int c = 0; c < NUM_COMMANDS; c++) {
        const char *cmd = sample_commands[c];
        int tk[SEQ_LEN];
        int32_t lg[N_CLASSES];

        uart_puts("\n---- command "); uart_putu((uint32_t)(c + 1));
        uart_puts(" of "); uart_putu((uint32_t)NUM_COMMANDS); uart_puts(" ----\n");
        uart_puts("input : \""); uart_puts(cmd); uart_puts("\"\n");

        int len = tokenize(cmd, tk);
        uart_puts("tokens:");
        if (len == 0) uart_puts(" (none in vocabulary)");
        for (int i = 0; i < len; i++) { uart_putc(' '); uart_puts(vocab[tk[i]]); }
        uart_putc('\n');

        int intent;
        if (npu_online) {
            intent = npu_infer(tk, len, lg);
            if (intent < 0) { intent = host_policy(tk, len, lg); }   /* safety */
            uart_puts("intent: "); uart_puts(intent_names[intent]); uart_puts("  (on Coral NPU)\n");
        } else {
            g_macs = 0;
            intent = host_policy(tk, len, lg);
            uart_puts("intent: "); uart_puts(intent_names[intent]); uart_puts("  (on host CPU)\n");
        }

        uart_puts("plan  : "); uart_putu((uint32_t)plan_len[intent]);
        uart_puts(" skills\n");
        for (int k = 0; k < plan_len[intent]; k++)
            drive(k, &plans[intent][k]);
    }

    uart_puts("\nAll commands executed. Robot idle (wfi).\n");
    return 0;
}
