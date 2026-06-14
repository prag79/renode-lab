CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -mcpu=cortex-a9 -marm \
           -ffreestanding -nostdlib -nostartfiles \
           -Wall -O2 -g
LDFLAGS := -T src/link.ld -nostdlib

OBJS    := src/start.o src/hello.o

all: sw/bare.elf sw/bare.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

sw/bare.elf: $(OBJS)
	mkdir -p sw
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

sw/bare.bin: sw/bare.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f src/*.o sw/bare.elf sw/bare.bin

.PHONY: all clean
