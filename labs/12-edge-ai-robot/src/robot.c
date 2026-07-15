/* Lab 12 - Edge-AI robot: natural-language command -> skill plan.
 *
 * A bare-metal RV64 "robot controller" that mirrors the architecture of
 * the LFM2.5-230M-on-Jetson robotics demo (a small on-device model acts
 * as a skill-selection layer that decomposes a command into a sequence
 * of pre-provided low-level skills) -- but shrunk so it runs as pure
 * integer arithmetic on a simulated MCU in Renode, with NO GPU, no ML
 * framework, and no network.
 *
 * Pipeline, once per command:
 *   1. TOKENIZE   the natural-language string into a bag-of-words vector
 *                 over a tiny vocabulary (src/model.h).
 *   2. CLASSIFY   with an int8 linear model: logits = W*x, intent=argmax.
 *   3. PLAN       expand the chosen intent into a fixed sequence of
 *                 low-level SKILLS (walk / turn / kneel / hold / stop /
 *                 dock) -- the "pre-trained skills" the model invokes.
 *   4. DRIVE      write each skill's parameters to a memory-mapped
 *                 "actuator" register block, so the robot's commanded
 *                 state is observable from the Renode monitor.
 *
 * The whole "AI" is the int8 matrix-vector multiply + argmax in
 * classify() -- readable in full, no magic.
 */

#include <stdint.h>
#include "model.h"

/* ---- Console UART (16550) ------------------------------------------- */
#define UART_BASE   0x10000000UL
#define UART_THR    (*(volatile unsigned char *)(UART_BASE + 0))
#define UART_LSR    (*(volatile unsigned char *)(UART_BASE + 5))
#define LSR_THRE    (1U << 5)

/* ---- Memory-mapped "robot actuator" register block ------------------
 * A plain MMIO window (see renode/robot.repl). The controller writes the
 * currently-commanded skill here; inspect it live from the monitor with
 *   sysbus ReadDoubleWord 0x90000000   (SKILL_ID), 0x90000004 (PARAM0) ...
 */
#define ACT_BASE    0x90000000UL
#define ACT_SKILL   0x00        /* current skill id                       */
#define ACT_PARAM0  0x04        /* skill param 0 (velocity / angle)       */
#define ACT_PARAM1  0x08        /* skill param 1 (distance / duration)    */
#define ACT_STEP    0x0C        /* index of this skill within the plan    */
#define ACT_REG(off) (*(volatile uint32_t *)(ACT_BASE + (off)))

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

static void uart_puti(int32_t v) {
    if (v < 0) { uart_putc('-'); v = -v; }
    uart_putu((uint32_t)v);
}

/* ---- Step 1: tokenize a command into bag-of-words counts ------------ */
/* Compare command slice [s, s+len) against a null-terminated vocab word. */
static int slice_eq(const char *s, int len, const char *word) {
    int i = 0;
    for (; i < len; i++) {
        if (word[i] == '\0' || s[i] != word[i]) return 0;
    }
    return word[i] == '\0';
}

static void encode(const char *cmd, int32_t x[VOCAB_SIZE]) {
    for (int j = 0; j < VOCAB_SIZE; j++) x[j] = 0;
    const char *p = cmd;
    while (*p) {
        while (*p == ' ') p++;              /* skip separators */
        const char *start = p;
        while (*p && *p != ' ') p++;         /* one whitespace token */
        int len = (int)(p - start);
        if (len > 0) {
            for (int j = 0; j < VOCAB_SIZE; j++) {
                if (slice_eq(start, len, vocab[j])) { x[j]++; break; }
            }
        }
    }
}

/* ---- Step 2: int8 linear classifier (logits = W*x, argmax) ---------- */
static int classify(const int32_t x[VOCAB_SIZE], int32_t logits[NUM_INTENTS]) {
    int best = 0;
    int32_t best_v = -2147483647;
    for (int i = 0; i < NUM_INTENTS; i++) {
        int32_t acc = 0;
        for (int j = 0; j < VOCAB_SIZE; j++)
            acc += (int32_t)model_W[i][j] * x[j];   /* int8 * int32 -> int32 */
        logits[i] = acc;
        if (acc > best_v) { best_v = acc; best = i; }
    }
    return best;
}

/* ---- Step 3: the low-level skill library + per-intent plans --------- */
enum { WALK = 1, TURN = 2, KNEEL = 3, HOLD = 4, STOP = 5, DOCK = 6 };

typedef struct { uint8_t id; int32_t p0; int32_t p1; } skill_t;

/* Each plan is a sequence of skills = the decomposition of one command.
 * WALK(vel_cm_s, dist_cm)  TURN(deg, dir)  KNEEL/HOLD(_, seconds). */
static const skill_t plan_patrol[]  = {{WALK,60,300},{TURN,90,0},{WALK,60,200},
                                       {TURN,90,0},{WALK,60,300},{DOCK,0,0}};
static const skill_t plan_home[]     = {{TURN,180,0},{WALK,80,500},{DOCK,0,0}};
static const skill_t plan_kneel[]    = {{HOLD,0,2},{KNEEL,0,5},{HOLD,0,2},{STOP,0,0}};
static const skill_t plan_advance[]  = {{WALK,100,300},{STOP,0,0}};
static const skill_t plan_retreat[]  = {{WALK,-50,200},{STOP,0,0}};
static const skill_t plan_turn[]     = {{TURN,180,0},{STOP,0,0}};
static const skill_t plan_stop[]     = {{STOP,0,0}};

static const skill_t *const plans[NUM_INTENTS] = {
    plan_patrol, plan_home, plan_kneel, plan_advance,
    plan_retreat, plan_turn, plan_stop,
};
static const int plan_len[NUM_INTENTS] = {6, 3, 4, 2, 2, 2, 1};

static const char *skill_name(uint8_t id) {
    switch (id) {
    case WALK:  return "WALK";
    case TURN:  return "TURN";
    case KNEEL: return "KNEEL";
    case HOLD:  return "HOLD";
    case STOP:  return "STOP";
    case DOCK:  return "DOCK";
    default:    return "?";
    }
}

/* ---- Step 4: drive the actuator + narrate each skill ---------------- */
static void drive(int step, const skill_t *s) {
    ACT_REG(ACT_SKILL)  = s->id;
    ACT_REG(ACT_PARAM0) = (uint32_t)s->p0;
    ACT_REG(ACT_PARAM1) = (uint32_t)s->p1;
    ACT_REG(ACT_STEP)   = (uint32_t)step;

    uart_puts("    ["); uart_putu((uint32_t)step); uart_puts("] ");
    uart_puts(skill_name(s->id));
    switch (s->id) {
    case WALK:
        uart_puts("  vel="); uart_puti(s->p0); uart_puts("cm/s");
        uart_puts(" dist="); uart_puti(s->p1); uart_puts("cm");
        if (s->p0 < 0) uart_puts("  (reverse)");
        break;
    case TURN:
        uart_puts("  angle="); uart_puti(s->p0); uart_puts("deg");
        break;
    case KNEEL:
    case HOLD:
        uart_puts("  "); uart_puti(s->p1); uart_puts("s");
        break;
    default:
        break;
    }
    uart_putc('\n');
}

int main(void) {
    uart_puts("\n*** Edge-AI robot: natural-language command -> skill plan ***\n");
    uart_puts("On-device int8 intent model (W*x, argmax) selects low-level skills.\n");
    uart_puts("No GPU, no ML framework, no network -- pure integer math on RV64.\n");

    int32_t x[VOCAB_SIZE];
    int32_t logits[NUM_INTENTS];

    for (int c = 0; c < NUM_COMMANDS; c++) {
        const char *cmd = sample_commands[c];

        uart_puts("\n---- command "); uart_putu((uint32_t)(c + 1));
        uart_puts(" of "); uart_putu((uint32_t)NUM_COMMANDS); uart_puts(" ----\n");
        uart_puts("input : \""); uart_puts(cmd); uart_puts("\"\n");

        encode(cmd, x);

        uart_puts("tokens:");
        int any = 0;
        for (int j = 0; j < VOCAB_SIZE; j++) {
            if (x[j]) { uart_putc(' '); uart_puts(vocab[j]); any = 1; }
        }
        if (!any) uart_puts(" (none in vocabulary)");
        uart_putc('\n');

        int intent = classify(x, logits);
        uart_puts("intent: "); uart_puts(intent_names[intent]);
        uart_puts("  (logit "); uart_puti(logits[intent]); uart_puts(")\n");

        uart_puts("plan  : "); uart_putu((uint32_t)plan_len[intent]);
        uart_puts(" skills\n");
        for (int k = 0; k < plan_len[intent]; k++)
            drive(k, &plans[intent][k]);
    }

    uart_puts("\nAll commands executed. Robot idle (wfi).\n");
    return 0;
}
