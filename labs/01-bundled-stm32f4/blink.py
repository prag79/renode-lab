# Blink loop for the STM32F4 Discovery user LED (PD12).
#
# ASCII only: IronPython 2.7's execfile() refuses UTF-8 (PEP 263).
#
# Loaded from blink.resc via execfile(). IronPython 2.7 (embedded in
# Renode) exposes `monitor` as a global in that scope.
#
# Do NOT use `emulation RunFor` here. The bundled Contiki firmware is a
# busy loop (almost no WFI), so 200 ms of *virtual* time can take minutes
# of wall clock. The monitor then sits on "Machine resumed." with no
# True/False output -- it is not frozen, just executing every instruction
# of Contiki. Keep the CPU paused and delay with time.sleep so the LED
# state is visible immediately.
#
# GPIOD BSRR = 0x40020C18
#   low  16 bits = SET   (write 1 -> pin high, LED on)
#   high 16 bits = RESET (write 1 -> pin low,  LED off)
# PD12 => bit 12 in the low half, bit 28 in the high half.

import time

monitor.Parse('pause')
monitor.Parse('logLevel -1 sysbus.gpioPortD.UserLED')

led = monitor.Machine['sysbus.gpioPortD.UserLED']

print 'Blinking UserLED (PD12) 10 times; CPU stays paused.'
print '(True/False in the monitor *is* the blink -- there is no GUI LED.)'
for i in range(10):
    monitor.Parse('sysbus WriteDoubleWord 0x40020C18 0x00001000')
    print '  [%d] ON  State=%s' % (i, led.State)
    time.sleep(0.2)
    monitor.Parse('sysbus WriteDoubleWord 0x40020C18 0x10000000')
    print '  [%d] OFF State=%s' % (i, led.State)
    time.sleep(0.2)
print 'Done. Firmware is still paused; type start to resume Contiki.'
