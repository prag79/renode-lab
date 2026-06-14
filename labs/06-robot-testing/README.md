# Lab 06 — Headless regression testing with Robot

Every lab so far you watched firmware run and judged it *by eye*.
That doesn't scale and it can't gate a pull request. This lab uses
Renode the way it earns its keep in industry: as a **headless,
scriptable test target**. A Robot Framework suite boots the firmware,
waits for specific UART output, and **asserts** the device behaved —
pass/fail, no human, exit code you can wire into CI.

This is the capstone: it ties together the build pipeline (lab 03),
a platform + ELF, and turns "does it work?" into an automated check.

## 1. Run the suite

```bash
lab 06
```

This mirrors `/labs/06-robot-testing/` into `~/work/06-robot-testing/`,
runs `make` to build `selftest.elf`, then runs:

```
renode-test tests/uart.robot
```

`renode-test` is Renode's own test runner (a Robot Framework wrapper
with the Renode keyword library pre-wired). You'll see Robot's
progress, then a summary:

```
Self-Test Should Pass                                          | PASS |
------------------------------------------------------------------------------
Platform Should Expose The UART                                | PASS |
------------------------------------------------------------------------------
Tests.Uart                                                     | PASS |
2 tests, 2 passed, 0 failed
```

Detailed HTML reports (`log.html`, `report.html`) and
`robot_output.xml` are written into the working directory — open
them from the VS Code file explorer.

## 2. What the test does

`tests/uart.robot` is the whole suite. The interesting parts:

```robot
*** Settings ***
Resource          ${RENODEKEYWORDS}      # Renode's keyword library

*** Keywords ***
Create Machine
    Execute Command           mach create "mini-rv"
    Execute Command           machine LoadPlatformDescription @${REPL}
    Execute Command           sysbus LoadELF @${ELF}
    Create Terminal Tester    ${UART}    # attach a virtual terminal we can assert on
    Start Emulation

*** Test Cases ***
Self-Test Should Pass
    Create Machine
    Wait For Line On Uart     === Renode CI self-test ===
    Wait For Line On Uart     step 1: memory OK
    Wait For Line On Uart     step 2: alu OK
    Wait For Line On Uart     ALL TESTS PASSED      timeout=5
```

`Create Terminal Tester` attaches to the UART and buffers its output.
`Wait For Line On Uart` blocks (in *simulated* time) until the
expected string appears, or fails the test if `timeout` seconds of
sim-time elapse first. `Execute Command` runs any Monitor command —
the same commands you typed by hand in labs 01–05 — so a test can
also poke registers, step the CPU, or inspect the platform tree
(the second test case asserts on `peripherals` output via
`Should Contain`).

The "device under test" is `src/selftest.c`: a tiny power-on
self-test that prints a line per check and ends in `ALL TESTS
PASSED`. The same mini-rv platform as lab 03 (RV64 + NS16550 UART).

## 3. Files

| File | What it is |
|---|---|
| `tests/uart.robot` | The Robot suite — keywords + two test cases. |
| `src/selftest.c` | Firmware under test; prints a self-test transcript. |
| `src/start.S`, `src/link.ld` | RV64 startup + linker script (as lab 03). |
| `Makefile` | Builds `selftest.elf`. **Real TABs in recipes.** |
| `renode/mini-rv.repl` | The platform. |
| `renode/mini-rv.resc` | Startup script for running it by hand (see §5). |

## 4. Make a test fail (the important part)

A test you've never seen fail is a test you don't trust. Break the
firmware and watch the suite catch it:

1. Edit `src/selftest.c` and sabotage a check, e.g. change
   `check_alu` to `return (x * y) == 41;` (wrong).
2. Re-run `lab 06`.

Now `step 2: alu OK` never prints — the firmware prints
`step 2: alu FAIL` and `SELF-TEST FAILED` instead. `Wait For Line On
Uart ALL TESTS PASSED` times out, and Robot reports:

```
Self-Test Should Pass                                          | FAIL |
Terminal tester failed! Line "ALL TESTS PASSED" not found ...
```

`renode-test` exits non-zero — which is exactly what fails a CI job.
Revert the change and it's green again.

## 5. Run the firmware by hand (optional)

The same firmware runs interactively, just like lab 03:

```bash
cd ~/work/06-robot-testing
make
renode --console renode/mini-rv.resc
```

The self-test transcript prints once in the `uart` analyzer, then
the CPU halts in `wfi`. Use this to eyeball behaviour before
encoding it as assertions.

## 6. Useful keywords to explore

The full list lives in Renode's docs ("Testing with Renode"). The
ones worth knowing:

| Keyword | What it does |
|---|---|
| `Create Terminal Tester ${UART}` | Attach an assertable terminal to a UART. |
| `Wait For Line On Uart <text>` | Block until a line appears (optional `timeout=`). |
| `Wait For Prompt On Uart <text>` | Like above, for shell prompts. |
| `Write Line To Uart <text>` | Send input to the firmware (interactive devices). |
| `Test If Uart Is Idle <time>` | Assert *nothing* is printed for a while. |
| `Execute Command <monitor cmd>` | Run any Monitor command; returns its output. |
| `Should Contain ${out} <text>` | Robot builtin — assert on captured output. |
| `Provides` / `Requires` | Snapshot state to share setup across tests. |

## 7. Why this matters

This is the difference between Renode-as-debugger and
Renode-as-CI-tool. In a real project, `tests/*.robot` runs on every
push (the `.github/workflows/` in this repo already builds the image;
a test job would add `renode-test`). Hardware bring-up bugs get
caught before silicon, and regressions get caught before merge —
with no physical board in the loop.

## What this lab proves

- Renode is fully scriptable and headless: the same Monitor commands
  you typed by hand become automated assertions.
- `Wait For Line On Uart` + `timeout` turns UART output into a
  deterministic pass/fail signal suitable for CI.
- A failing firmware makes the suite go red and `renode-test` exit
  non-zero — the contract every CI system relies on.
- You can test embedded firmware in pull requests with zero hardware.
