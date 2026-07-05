CROSS   ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

# FE310 is a 32-bit core (RV32IMAC), same toolchain as labs 04/05.
CFLAGS  := -march=rv32imac -mabi=ilp32 -mcmodel=medany \
           -ffreestanding -nostdlib -nostartfiles \
           -Wall -O2 -g
LDFLAGS := -T src/link.ld -nostdlib

# One firmware image per node. node0 is the gateway; node1/node2 are
# sensors. Same C source, different -DNODE_ID baked in per image.
NODES := 0 1 2
ELVES := $(addprefix node,$(addsuffix .elf,$(NODES)))

all: $(ELVES)

# Shared startup object (identical for every node).
src/start.o: src/start.S
	$(CC) $(CFLAGS) -c $< -o $@

# Per-node object of iot.c, with the node id compiled in.
node%.o: src/iot.c
	$(CC) $(CFLAGS) -DNODE_ID=$* -c $< -o $@

node%.elf: node%.o src/start.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f src/*.o node*.o $(ELVES)

.PHONY: all clean
