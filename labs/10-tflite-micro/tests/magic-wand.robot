*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Resource                      ${RENODEKEYWORDS}

# Headless regression test for the real TFLM "magic wand" demo (lab 06
# style, but asserting on machine-learning output). It boots the
# prebuilt Zephyr+TFLM firmware, streams recorded gestures into the
# ADXL345, and asserts the model prints the expected RING and SLOPE
# gesture art. Run with:  make test   (or: lab 06-style renode-test)
#
# NB: TFLM inference is compute-heavy; the terminal tester uses a long
# timeout. Adapted from antmicro/litex-vexriscv-tensorflow-lite-demo
# (Apache-2.0).

*** Keywords ***
Wait For Ring
    Wait For Line On Uart     RING:
    Wait For Line On Uart     ${SPACE*10}*
    Wait For Line On Uart     ${SPACE*7}*${SPACE*5}*
    Wait For Line On Uart     ${SPACE*5}*${SPACE*9}*
    Wait For Line On Uart     ${SPACE*4}*${SPACE*11}*
    Wait For Line On Uart     ${SPACE*5}*${SPACE*9}*
    Wait For Line On Uart     ${SPACE*7}*${SPACE*5}*
    Wait For Line On Uart     ${SPACE*10}*

Wait For Slope
    Wait For Line On Uart    SLOPE:
    Wait For Line On Uart    ${SPACE*8}*
    Wait For Line On Uart    ${SPACE*7}*
    Wait For Line On Uart    ${SPACE*6}*
    Wait For Line On Uart    ${SPACE*5}*
    Wait For Line On Uart    ${SPACE*4}*
    Wait For Line On Uart    ${SPACE*3}*
    Wait For Line On Uart    ${SPACE*2}*
    Wait For Line On Uart    ${SPACE}* * * * * * * *

*** Test Cases ***
Run Magic Wand Demo
    Execute Command           using sysbus

    Execute Command           mach create
    Execute Command           machine LoadPlatformDescription @${CURDIR}/../renode/magic-wand.repl

    Execute Command           showAnalyzer sysbus.uart Antmicro.Renode.Analyzers.LoggingUartAnalyzer

    Execute Command           sysbus LoadELF @${CURDIR}/../binaries/magic_wand/zephyr.elf

    Execute Command           i2c.adxl345 MaxFifoDepth 1
    Execute Command           i2c.adxl345 FeedSample @${CURDIR}/../renode/circle.data
    Execute Command           i2c.adxl345 FeedSample 0 15000 15000 128
    Execute Command           i2c.adxl345 FeedSample 0 0 0 128
    Execute Command           i2c.adxl345 FeedSample @${CURDIR}/../renode/angle.data
    Execute Command           i2c.adxl345 FeedSample 0 15000 15000 128
    Execute Command           i2c.adxl345 FeedSample 0 0 0 128

    Create Terminal Tester    sysbus.uart  timeout=480

    Start Emulation

    Wait For Line On Uart     Booting Zephyr OS
    Wait For Line On Uart     Got accelerometer

    Wait For Ring
    Wait For Slope
