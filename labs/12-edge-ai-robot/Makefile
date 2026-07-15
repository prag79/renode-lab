CROSS   ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc

# RV64IMAC, integer-only: no float, no libc, no startup files. The intent
# model is int8, so the whole pipeline is integer -- no soft-float needed.
CFLAGS  := -march=rv64imac -mabi=lp64 -mcmodel=medany \
           -ffreestanding -nostdlib -nostartfiles \
           -Wall -O2 -g
LDFLAGS := -T src/link.ld -nostdlib

OBJS    := src/start.o src/robot.o

all: robot.elf

# robot.c pulls in the checked-in model weights.
src/robot.o: src/robot.c src/model.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

robot.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# Optional: regenerate src/model.h (prints the same table this ships with).
# Needs only Python 3 (no ML framework, no internet).
model:
	python3 tools/gen_model.py

# Headless regression: boot the firmware and assert all 8 commands
# classify into the expected intents (lab 06 / renode-test style).
test:
	@if command -v renode-test >/dev/null 2>&1; then \
	    renode-test tests/robot.robot; \
	else \
	    /opt/renode/renode-test tests/robot.robot; \
	fi

clean:
	rm -f src/*.o robot.elf robot_output.xml log.html report.html

.PHONY: all clean model test
