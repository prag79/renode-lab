# Lab 01 — Bundled STM32F4 Discovery demo

The simplest Renode invocation. Loads a pre-built ARM Cortex-M4
firmware on the STM32F4 Discovery board model that ships with
Renode. Use this to confirm the image is working before moving on
to anything custom.

## Run it

```bash
lab 01
```

A Renode monitor prompt appears. The bundled `.resc` script
auto-starts the simulation; you should see UART output in the
analyzer panel (visible in the noVNC tab on port 6080).

## What this proves

- Mono runtime is healthy.
- Renode binary launches without errors.
- The `scripts/` tree shipped inside `/opt/renode/` is reachable.

If `lab 01` fails, nothing else in this lab will work either.
