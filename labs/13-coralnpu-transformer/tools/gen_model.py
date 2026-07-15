#!/usr/bin/env python3
"""Regenerate src/model.h for the Coral-NPU transformer ROBOT lab (lab 13).

Dependency-free (plain Python 3, no numpy / no ML framework, no internet).
It emits a small but *real* int8 transformer encoder block that acts as the
robot's language policy:

    command words -> embedding -> 1-head self-attention -> FFN
                  -> mean-pool -> classifier -> robot intent

The transformer classifies a natural-language command into one of four
robot INTENTS; the firmware then expands that intent into a plan of
low-level skills and drives a memory-mapped actuator (lab 12's shape, with
a transformer brain). The weights are hand-constructed (deterministic) so
the mapping is interpretable: every vocabulary word belongs to an intent,
and the network routes a command to the intent its words vote for. All
integer (int8 weights, int32 accumulate, integer-softmax LUT) so it runs
on a bare-metal RV64 core with no FPU -- and its matmuls are the workload
we offload to the Coral NPU.

Run:  python3 tools/gen_model.py   (or `make model`), then `make`.
"""
import math
import os

# ---- Transformer configuration -------------------------------------------
N_CLASSES = 4       # robot intents (see INTENTS below)
SEQ_LEN = 8         # max tokens per command
D_MODEL = 16        # embedding / hidden width
D_FF = 32           # feed-forward inner width

# Softmax LUT: exp(-diff/SCALE) quantized to Q7 (0..127).
SOFTMAX_LUT_SIZE = 32
SOFTMAX_SCALE = 4.0

# ---- Robot vocabulary: every word belongs to exactly one intent ----------
# The transformer learns "word -> intent" through the embedding; a command
# is routed to the intent its words vote for. Token id 0 is padding.
INTENTS = ["ADVANCE", "RETREAT", "TURN_AROUND", "RETURN_HOME"]

VOCAB_BY_INTENT = {
    0: ["walk", "forward", "move", "ahead", "go", "advance"],   # ADVANCE
    1: ["back", "backward", "reverse", "retreat"],              # RETREAT
    2: ["turn", "around", "spin", "rotate", "left", "right"],   # TURN_AROUND
    3: ["home", "dock", "return", "base", "charge"],            # RETURN_HOME
}

# Natural-language commands the robot executes, with their expected intent.
SAMPLE_COMMANDS = [
    "walk forward",         # ADVANCE
    "move ahead",           # ADVANCE
    "retreat backward",     # RETREAT
    "reverse now",          # RETREAT ("now" not in vocab -> ignored)
    "turn around",          # TURN_AROUND
    "spin left",            # TURN_AROUND
    "return home",          # RETURN_HOME
    "return to base",       # RETURN_HOME ("to" not in vocab -> ignored)
]

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "src", "model.h")


def build_vocab():
    """Flatten the per-intent word lists into an id-indexed vocab (0=pad)
    plus a token_id -> intent map."""
    vocab = [""]                 # id 0 = padding
    token_intent = [-1]
    for intent in range(N_CLASSES):
        for word in VOCAB_BY_INTENT[intent]:
            vocab.append(word)
            token_intent.append(intent)
    return vocab, token_intent


def build_embedding(vocab, token_intent):
    """e[id][d]: a token carries a +4 signal in its intent's dimension plus
    a small per-token fingerprint (keeps distinct words distinguishable)."""
    emb = [[0] * D_MODEL for _ in range(len(vocab))]
    for tid in range(len(vocab)):
        intent = token_intent[tid]
        if intent < 0:
            continue
        emb[tid][intent] = 4
        emb[tid][N_CLASSES + (tid % (D_MODEL - N_CLASSES))] = 1
    return emb


def identity(rows, cols, diag=1):
    m = [[0] * cols for _ in range(rows)]
    for i in range(min(rows, cols)):
        m[i][i] = diag
    return m


def build_ffn():
    w1 = [[0] * D_FF for _ in range(D_MODEL)]
    w2 = [[0] * D_MODEL for _ in range(D_FF)]
    for d in range(D_MODEL):
        h0, h1 = (2 * d) % D_FF, (2 * d + 1) % D_FF
        w1[d][h0] = 1
        w1[d][h1] = 1
        w2[h0][d] = 1
    return w1, w2


def build_classifier():
    w = [[0] * N_CLASSES for _ in range(D_MODEL)]
    for c in range(N_CLASSES):
        w[c][c] = 8
    return w


def build_softmax_lut():
    return [int(round(127.0 * math.exp(-i / SOFTMAX_SCALE)))
            for i in range(SOFTMAX_LUT_SIZE)]


def fmt_i8_matrix(name, mat, cols):
    lines = [f"static const int8_t {name}[{len(mat)}][{cols}] = {{"]
    for row in mat:
        lines.append("    {" + ",".join(f"{v:2d}" for v in row) + "},")
    lines.append("};")
    return "\n".join(lines)


def fmt_i8_vec(name, vec):
    return (f"static const int8_t {name}[{len(vec)}] = {{"
            + ",".join(f"{v:3d}" for v in vec) + "};")


def fmt_str_array(name, items):
    body = ",".join(f'"{s}"' for s in items)
    return f"static const char *const {name}[] = {{{body}}};"


def main():
    vocab, token_intent = build_vocab()
    emb = build_embedding(vocab, token_intent)
    wq, wk, wv, wo = (identity(D_MODEL, D_MODEL) for _ in range(4))
    w1, w2 = build_ffn()
    wcls = build_classifier()
    lut = build_softmax_lut()
    vocab_size = len(vocab)

    out = []
    out.append("/* AUTO-GENERATED by tools/gen_model.py -- edit that file, not this. */")
    out.append("/* int8 transformer language policy for the Coral-NPU robot lab. */")
    out.append("#ifndef MODEL_H")
    out.append("#define MODEL_H\n")
    out.append("#include <stdint.h>\n")
    out.append(f"#define VOCAB_SIZE       {vocab_size}")
    out.append(f"#define N_CLASSES        {N_CLASSES}")
    out.append(f"#define SEQ_LEN          {SEQ_LEN}")
    out.append(f"#define D_MODEL          {D_MODEL}")
    out.append(f"#define D_FF             {D_FF}")
    out.append(f"#define SOFTMAX_LUT_SIZE {SOFTMAX_LUT_SIZE}\n")

    out.append(fmt_i8_matrix("embedding", emb, D_MODEL) + "\n")
    out.append(fmt_i8_matrix("W_q", wq, D_MODEL))
    out.append(fmt_i8_matrix("W_k", wk, D_MODEL))
    out.append(fmt_i8_matrix("W_v", wv, D_MODEL))
    out.append(fmt_i8_matrix("W_o", wo, D_MODEL) + "\n")
    out.append(fmt_i8_matrix("W_ff1", w1, D_FF))
    out.append(fmt_i8_matrix("W_ff2", w2, D_MODEL) + "\n")
    out.append(fmt_i8_matrix("W_cls", wcls, N_CLASSES) + "\n")
    out.append(fmt_i8_vec("softmax_lut", lut) + "\n")

    # id-indexed vocabulary (index 0 is the empty padding token).
    out.append(fmt_str_array("vocab", vocab))
    out.append("#define NUM_VOCAB ((int)(sizeof(vocab)/sizeof(vocab[0])))\n")

    out.append(fmt_str_array("intent_names", INTENTS) + "\n")

    out.append(fmt_str_array("sample_commands", SAMPLE_COMMANDS))
    out.append("#define NUM_COMMANDS "
               "((int)(sizeof(sample_commands)/sizeof(sample_commands[0])))\n")

    out.append("#endif /* MODEL_H */")

    with open(OUT, "w") as f:
        f.write("\n".join(out) + "\n")
    print(f"wrote {OUT}: {N_CLASSES} intents, vocab={vocab_size}, "
          f"d_model={D_MODEL}, {len(SAMPLE_COMMANDS)} commands")


if __name__ == "__main__":
    main()
