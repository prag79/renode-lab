CROSS   ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc

CFLAGS  := -march=rv64imac -mabi=lp64 -mcmodel=medany \
           -ffreestanding -nostdlib -nostartfiles \
           -Wall -O2 -g
LDFLAGS := -T src/link.ld -nostdlib

# Default (single-node) build: one sensor streaming straight to the host
# bridge. `make fleet` builds the optional multi-node variant instead.
OBJS    := src/start.o src/telemetry.o

all: telemetry.elf

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

telemetry.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# ---- Fleet variant: gateway + 2 sensors from one source (-DNODE_ID) ----
FLEET_ELVES := gateway.elf sensor1.elf sensor2.elf

fleet: $(FLEET_ELVES)

# Per-node object of fleet.c, with the node id compiled in.
fleet%.o: src/fleet.c
	$(CC) $(CFLAGS) -DNODE_ID=$* -c $< -o $@

gateway.elf: fleet0.o src/start.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

sensor1.elf: fleet1.o src/start.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

sensor2.elf: fleet2.o src/start.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f src/*.o fleet*.o telemetry.elf $(FLEET_ELVES)

.PHONY: all fleet clean
