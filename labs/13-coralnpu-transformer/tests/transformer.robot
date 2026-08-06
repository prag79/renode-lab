*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

# NOTE: this suite needs Renode NIGHTLY (CPU.CoralNPU is not in stable).
# Run it with `make test`, which invokes renode-test-nightly. See README.

*** Variables ***
${UART}                       sysbus.uart
${REPL}                       ${CURDIR}/../renode/transformer.repl
${KERNEL}                     ${CURDIR}/../coralnpu/kernel/npu_transformer.bin
${KERNEL_URL}                 @https://dl.antmicro.com/projects/renode/coralnpu_v2_hello_world_add_floats.bin-s_65648-0e3f5d6ae173fa2e06f6b5f91906ef721516de4c
${ELF}                        ${CURDIR}/../host.elf

*** Keywords ***
Create Machine
    Execute Command           mach create "coral"
    Execute Command           i @${CURDIR}/../peripherals/RobotActuator.cs
    Execute Command           machine LoadPlatformDescription @${REPL}
    # Prefer the real transformer kernel (functional offload); fall back to
    # the Antmicro add sample (host runs the policy). Either way the robot
    # behaviour below holds.
    ${built}=                 Run Keyword And Return Status
    ...                       Execute Command    npu LoadBinary @${KERNEL} 0
    Run Keyword Unless        ${built}
    ...                       Execute Command    npu LoadBinary ${KERNEL_URL} 0
    Execute Command           sysbus LoadELF @${ELF}
    Create Terminal Tester    ${UART}
    Start Emulation

*** Test Cases ***
Commands Should Map To Robot Intents And Skill Plans
    [Documentation]           Boots the robot controller and asserts each
    ...                       natural-language command is routed by the
    ...                       transformer to the expected intent + plan.
    Create Machine
    Wait For Line On Uart     *** Transformer robot policy accelerated by a Coral NPU ***
    Wait For Line On Uart     input : "walk forward"
    Wait For Line On Uart     intent: ADVANCE
    Wait For Line On Uart     input : "retreat backward"
    Wait For Line On Uart     intent: RETREAT
    Wait For Line On Uart     input : "turn around"
    Wait For Line On Uart     intent: TURN_AROUND
    Wait For Line On Uart     input : "return home"
    Wait For Line On Uart     intent: RETURN_HOME
    Wait For Line On Uart     All commands executed. Robot idle (wfi).

Policy Runs And Host NPU Agree
    [Documentation]           The head-to-head block runs the policy on the
    ...                       host CPU; if the transformer kernel is loaded it
    ...                       also runs on the Coral NPU and agrees.
    Create Machine
    Wait For Line On Uart     \# host_policy_start
    Wait For Line On Uart     \# host_policy_done
    # NOTE: the firmware line is "host CPU  : intent ADVANCE, ..." -- the
    # double space would split into two Robot Framework cells, so match a
    # single-space substring instead.
    Wait For Line On Uart     host CPU

Platform Should Expose Host CPU Coral NPU And Actuator
    [Documentation]           The board exposes the host cpu, uart, ram, the
    ...                       CPU.CoralNPU accelerator, and the actuator.
    Create Machine
    ${tree}=                  Execute Command    peripherals
    Should Contain            ${tree}            uart
    Should Contain            ${tree}            cpu
    Should Contain            ${tree}            CoralNPU
    Should Contain            ${tree}            actuator
