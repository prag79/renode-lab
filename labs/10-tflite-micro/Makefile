# Lab 10 uses a PREBUILT TensorFlow Lite Micro binary
# (binaries/magic_wand/zephyr.elf), so there is nothing to compile:
# `lab 10` loads it directly. This Makefile exists for the optional
# `make test` regression run and to document rebuilding from source.

ELF := binaries/magic_wand/zephyr.elf

all:
	@test -f $(ELF) && echo "Prebuilt TFLM firmware present: $(ELF)" \
	  || echo "ERROR: $(ELF) missing (re-clone the lab)."

# Headless regression: boot the firmware, feed gestures, assert the
# model prints RING and SLOPE. TFLM inference is slow, so this can take
# a few minutes. Same renode-test runner as lab 06.
test:
	@if command -v renode-test >/dev/null 2>&1; then \
	    renode-test tests/magic-wand.robot; \
	else \
	    /opt/renode/renode-test tests/magic-wand.robot; \
	fi

# Rebuilding the firmware from source is a heavy, network-heavy job
# (clone TensorFlow + Zephyr + LiteX and run the TFLM make target). It
# is intentionally NOT wired into `lab 10`. See README section 5 and:
#   https://github.com/antmicro/litex-vexriscv-tensorflow-lite-demo
rebuild:
	@echo "See README section 5 for building zephyr.elf from source."

clean:
	@echo "Nothing to clean (firmware is prebuilt and vendored)."

.PHONY: all test rebuild clean
