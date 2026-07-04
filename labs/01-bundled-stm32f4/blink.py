# Blink loop for the STM32F4 Discovery user LED (PD12).
#
# Loaded from blink.resc via `python "execfile('blink.py')"`.
# IronPython 2.7 (embedded in Renode) exposes `monitor` as a global
# in the execfile scope, so we can drive the simulator by feeding
# regular monitor command lines back through monitor.Parse().
#
# GPIOD BSRR = 0x40020C18
#   low  16 bits = SET   (write 1 -> pin goes high, LED on)
#   high 16 bits = RESET (write 1 -> pin goes low,  LED off)
# PD12 => bit 12 in the low half, bit 28 in the high half.
#
# `emulation RunFor "0.2"` advances 200 ms of *virtual* time then
# pauses automatically. Wall-clock rate depends on host speed;
# the simulated period is deterministically 400 ms per full cycle.

for i in range(10):
    monitor.Parse('sysbus WriteDoubleWord 0x40020C18 0x00001000')
    monitor.Parse('emulation RunFor "0.2"')
    monitor.Parse('sysbus.gpioPortD.UserLED State')
    monitor.Parse('sysbus WriteDoubleWord 0x40020C18 0x10000000')
    monitor.Parse('emulation RunFor "0.2"')
    monitor.Parse('sysbus.gpioPortD.UserLED State')
