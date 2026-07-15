CROSS   ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc

CFLAGS  := -march=rv64imac -mabi=lp64 -mcmodel=medany \
           -ffreestanding -nostdlib -nostartfiles \
           -Wall -O2 -g
LDFLAGS := -T src/link.ld -nostdlib

OBJS    := src/start.o src/host.o

all: host.elf

src/host.o: src/host.c src/model.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

host.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

model:
	python3 tools/gen_model.py

# Build the CoralNPU transformer kernel (rv32im) for the functional offload.
# Best-effort: needs an rv32 toolchain (riscv64-elf- with rv32im multilib,
# or CROSS=riscv32-unknown-elf-). See coralnpu/README.md.
kernel:
	$(MAKE) -C coralnpu/kernel

# Lab 13 requires Renode NIGHTLY (the `CPU.CoralNPU` component is not in
# any stable release yet), so this target uses `renode-test-nightly`
# installed alongside stable in the image. See README section on Renode
# versions.
test:
	@$(MAKE) -C coralnpu/kernel >/dev/null 2>&1 && echo "built transformer kernel (functional offload)" || echo "no rv32 toolchain: test will use the add sample + host fallback"
	@if command -v renode-test-nightly >/dev/null 2>&1; then \
	    renode-test-nightly tests/transformer.robot; \
	elif [ -x /opt/renode-nightly/renode-test ]; then \
	    /opt/renode-nightly/renode-test tests/transformer.robot; \
	else \
	    echo "lab 13 needs Renode nightly (renode-test-nightly / /opt/renode-nightly)."; \
	    echo "The stable 'renode-test' does not include CPU.CoralNPU. See README."; \
	    exit 1; \
	fi

clean:
	rm -f src/*.o host.elf robot_output.xml log.html report.html
	$(MAKE) -C coralnpu/kernel clean 2>/dev/null || true

.PHONY: all clean model kernel test
