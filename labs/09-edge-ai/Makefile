CROSS   ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc

# RV64IMAC, integer-only: no float, no libc, no startup files. The model
# is quantized to int8 precisely so we never need soft-float here.
CFLAGS  := -march=rv64imac -mabi=lp64 -mcmodel=medany \
           -ffreestanding -nostdlib -nostartfiles \
           -Wall -O2 -g
LDFLAGS := -T src/link.ld -nostdlib

OBJS    := src/start.o src/digits.o

all: digits.elf

# digits.c pulls in the auto-generated weights.
src/digits.o: src/digits.c src/model.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

digits.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# Optional: regenerate src/model.h by retraining + requantizing the MLP.
# Needs numpy + scikit-learn (only for regeneration, not for the lab).
# `make model` then `make` to rebuild the firmware with new weights.
model:
	python3 tools/gen_model.py

clean:
	rm -f src/*.o digits.elf

.PHONY: all clean model
