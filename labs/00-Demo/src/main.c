//Bare metal ARM Demo to exercise SmartTimer MMIO
#include <stdint.h>

#define SMARTTIMER_BASE 0x70000000u

#define SMARTTIMER_CONTROL (*(volatile uint32_t *)(SMARTTIMER_BASE + 0x00))
#define SMARTTIMER_PERIOD (*(volatile uint32_t *)(SMARTTIMER_BASE + 0x04))
#define SMARTTIMER_DUTY (*(volatile uint32_t *)(SMARTTIMER_BASE + 0x08))
#define SMARTTIMER_STATUS (*(volatile uint32_t *)(SMARTTIMER_BASE + 0x0C))

#define SMARTTIMER_CONTROL_ENABLE (1U << 0)
#define SMARTTIMER_CONTROL_IRQ_ENABLE (1U << 1)
#define SMARTTIMER_STATUS_PENDING (1U << 0)

static inline void mmio_write32(volatile uint32_t *reg, uint32_t value) {
    *reg = value;
}

static inline uint32_t mmio_read32(volatile uint32_t *reg) {
    return *reg;
}

int main(void) {
    mmio_write32(&SMARTTIMER_CONTROL, SMARTTIMER_CONTROL_ENABLE | SMARTTIMER_CONTROL_IRQ_ENABLE);
    mmio_write32(&SMARTTIMER_PERIOD, 1000000);
    mmio_write32(&SMARTTIMER_DUTY, 500000);
    mmio_write32(&SMARTTIMER_STATUS, SMARTTIMER_STATUS_PENDING);
  //Read smart timer register contents
    uint32_t control = mmio_read32(&SMARTTIMER_CONTROL);
    uint32_t period = mmio_read32(&SMARTTIMER_PERIOD);
    uint32_t duty = mmio_read32(&SMARTTIMER_DUTY);
    uint32_t status = mmio_read32(&SMARTTIMER_STATUS);
    return 0;
}

