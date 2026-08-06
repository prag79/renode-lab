# Build the CoralNPU transformer kernel (rv32im, bare metal).
#
# The Coral NPU core is RISC-V rv32im (+ Kelvin SIMD we don't need for a
# scalar int8 matmul), so a stock rv32 toolchain compiles it. We use the
# same riscv64-unknown-elf-* toolchain as the rest of the labs with -march=rv32im.
# Override CROSS if your rv32 toolchain has a different prefix, e.g.
#   make CROSS=riscv32-unknown-elf-
#
# Weights come from the shared model header (../../src/model.h), so the
# NPU-side inference matches the host reference exactly. Run
# `python3 ../../tools/gen_model.py` first if you regenerated the model.
CROSS   ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -march=rv32im -mabi=ilp32 -mcmodel=medany \
           -ffreestanding -nostdlib -nostartfiles -fno-builtin \
           -Os -Wall -I ../../src
LDFLAGS := -T link.ld -nostdlib

OBJS    := crt0.o npu_transformer.o

all: npu_transformer.bin

npu_transformer.o: npu_transformer.c ../../src/model.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

npu_transformer.elf: $(OBJS) link.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

# Flat image for `npu LoadBinary @npu_transformer.bin 0`. The file offset is
# the NPU-local address: .text at 0x0 (ITCM), .rodata at 0x10400 (DTCM, above
# the mailbox), zero-padded in between -- the same layout as Antmicro's
# official coralnpu_v2_hello_world_add_floats.bin sample.
npu_transformer.bin: npu_transformer.elf
	$(OBJCOPY) -O binary $< $@

disasm: npu_transformer.elf
	$(CROSS)objdump -d $< | head -40

clean:
	rm -f *.o npu_transformer.elf npu_transformer.bin

.PHONY: all clean disasm
