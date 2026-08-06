/* Auto-generatable intent model for the edge-AI robot (lab 12).
 *
 * This is the "brain" the LFM2.5-on-Jetson demo uses an LLM for, shrunk
 * to something that runs as pure integer math on a bare-metal MCU: a
 * linear classifier that maps a natural-language command to ONE of a
 * handful of intents. Each intent then expands (in robot.c) into a
 * sequence of pre-provided low-level SKILLS -- the same "skill-selection
 * layer invoking pre-trained skills" split the LFM2.5 robotics writeup
 * describes, just at TinyML scale and with no GPU.
 *
 * Model:  logits = W * x   (x = bag-of-words counts over `vocab`)
 *         intent = argmax(logits)
 * `W` is int8; inference is int8*int8 -> int32 accumulate + argmax, so
 * there is not a single floating-point op in the whole pipeline.
 *
 * `tools/gen_model.py` can regenerate this file; it's checked in so the
 * lab builds with no Python and no internet.
 */
#ifndef MODEL_H
#define MODEL_H

#include <stdint.h>

#define VOCAB_SIZE  20
#define NUM_INTENTS 7

/* The recognized keywords. Any word in a command that isn't here is
 * simply ignored (out-of-vocabulary), exactly like a real tokenizer. */
static const char *const vocab[VOCAB_SIZE] = {
    "walk", "forward", "backward", "back",  "kneel",
    "hold", "still",   "turn",     "around","left",
    "right","patrol",  "area",     "stop",  "halt",
    "home", "dock",    "go",       "return","wait",
};

static const char *const intent_names[NUM_INTENTS] = {
    "PATROL", "RETURN_HOME", "KNEEL_HOLD", "ADVANCE",
    "RETREAT", "TURN_AROUND", "STOP",
};

/* int8 weight matrix [intent][word]. Each intent scores high on the
 * words that characterize it; argmax over W*x picks the intent. */
static const int8_t model_W[NUM_INTENTS][VOCAB_SIZE] = {
/*                       walk   fwd    bwd     back     kneel    hold    still     turn     arnd     left   right patrol area stop halt home dock go ret wait */
/* PATROL   */ {  0,  0,  0,  0,   0,   0,   0,   0,   1,   0,   0,    5,   4,   0,   0,   0,   0,  0,  0,   0 },
/* RET_HOME */ {  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   5,   5,  1,  4,   0 },
/* KNEEL_HD */ {  0,  0,  0,  0,   5,   4,   3,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,  0,  0,   0 },
/* ADVANCE  */ {  2,  5,  0,  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,  0,  0,   0 },
/* RETREAT  */ {  2,  0,  5,  4,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,  0,  0,   0 },
/* TURN_ARN */ {  0,  0,  0,  0,   0,   0,   0,   5,   3,   3,   3,    0,   0,   0,   0,   0,   0,  0,  0,   0 },
/* STOP     */ {  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   5,   5,   0,   0,  0,  0,   3 },
};

/* Natural-language commands the robot receives, one per "tick". Edit
 * these freely (only words in `vocab` above influence the decision). */
static const char *const sample_commands[] = {
    "walk forward",
    "walk backward",
    "kneel and hold still",
    "turn around",
    "patrol the area",
    "return home",
    "go to the dock",
    "stop and wait",
};
#define NUM_COMMANDS ((int)(sizeof(sample_commands) / sizeof(sample_commands[0])))

#endif /* MODEL_H */
