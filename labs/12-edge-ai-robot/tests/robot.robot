*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${UART}                       sysbus.uart
${REPL}                       ${CURDIR}/../renode/robot.repl
${ELF}                        ${CURDIR}/../robot.elf

*** Keywords ***
Create Machine
    Execute Command           mach create "robot"
    Execute Command           i @${CURDIR}/../peripherals/RobotActuator.cs
    Execute Command           machine LoadPlatformDescription @${REPL}
    Execute Command           sysbus LoadELF @${ELF}
    Create Terminal Tester    ${UART}
    Start Emulation

*** Test Cases ***
All Commands Should Classify And Plan
    [Documentation]           Boots the firmware and asserts each sample
    ...                       command is classified into the expected intent,
    ...                       ending in the idle banner.
    Create Machine
    Wait For Line On Uart     *** Edge-AI robot: natural-language command -> skill plan ***
    Wait For Line On Uart     input : "walk forward"
    Wait For Line On Uart     intent: ADVANCE
    Wait For Line On Uart     input : "walk backward"
    Wait For Line On Uart     intent: RETREAT
    Wait For Line On Uart     input : "kneel and hold still"
    Wait For Line On Uart     intent: KNEEL_HOLD
    Wait For Line On Uart     input : "turn around"
    Wait For Line On Uart     intent: TURN_AROUND
    Wait For Line On Uart     input : "patrol the area"
    Wait For Line On Uart     intent: PATROL
    Wait For Line On Uart     input : "return home"
    Wait For Line On Uart     intent: RETURN_HOME
    Wait For Line On Uart     input : "go to the dock"
    Wait For Line On Uart     intent: RETURN_HOME
    Wait For Line On Uart     input : "stop and wait"
    Wait For Line On Uart     intent: STOP
    Wait For Line On Uart     All commands executed. Robot idle (wfi).

Platform Should Expose Actuator MMIO
    [Documentation]           The board exposes cpu, uart, ram, and the
    ...                       actuator register window the planner writes.
    Create Machine
    ${tree}=                  Execute Command    peripherals
    Should Contain            ${tree}            uart
    Should Contain            ${tree}            cpu
    Should Contain            ${tree}            actuator
