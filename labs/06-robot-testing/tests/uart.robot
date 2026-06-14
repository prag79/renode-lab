*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

*** Variables ***
${UART}                       sysbus.uart
${REPL}                       ${CURDIR}/../renode/mini-rv.repl
${ELF}                        ${CURDIR}/../selftest.elf

*** Keywords ***
Create Machine
    Execute Command           mach create "mini-rv"
    Execute Command           machine LoadPlatformDescription @${REPL}
    Execute Command           sysbus LoadELF @${ELF}
    Create Terminal Tester    ${UART}
    Start Emulation

*** Test Cases ***
Self-Test Should Pass
    [Documentation]           Boots the firmware and asserts each self-test
    ...                       line appears, in order, ending in PASS.
    Create Machine
    Wait For Line On Uart     === Renode CI self-test ===
    Wait For Line On Uart     step 1: memory OK
    Wait For Line On Uart     step 2: alu OK
    Wait For Line On Uart     ALL TESTS PASSED              timeout=5

Platform Should Expose The UART
    [Documentation]           Drives the Monitor from the test and asserts on
    ...                       the command output (not just UART traffic).
    Create Machine
    ${tree}=                  Execute Command    peripherals
    Should Contain            ${tree}            uart
    Should Contain            ${tree}            cpu
