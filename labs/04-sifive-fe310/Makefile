CROSS   ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

# FE310 is a 32-bit core (RV32IMAC). Same toolchain as lab 03,
# different -march/-mabi.
CFLAGS  := -march=rv32imac -mabi=ilp32 -mcmodel=medany \
           -ffreestanding -nostdlib -nostartfiles \
           -Wall -O2 -g
LDFLAGS := -T src/link.ld -nostdlib

OBJS    := src/start.o src/blink.o

all: blink.elf blink.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

blink.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

blink.bin: blink.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f src/*.o blink.elf blink.bin

.PHONY: all clean
