CROSS   ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -march=rv64imac -mabi=lp64 -mcmodel=medany \
           -ffreestanding -nostdlib -nostartfiles \
           -Wall -O2 -g
LDFLAGS := -T src/link.ld -nostdlib

OBJS    := src/start.o src/main.o

all: firmware.elf

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

firmware.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f src/*.o firmware.elf

.PHONY: all clean
