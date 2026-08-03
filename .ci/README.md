# CI harness files

Nothing in this repo (outside `.ci/`) is a complete, flashable ESPHome
config. Every file under `DBE/`, `bluetooth-proxy/`, `generic/`,
`shelly/`, and `sonoff/` is a **reusable package fragment** — it relies
on `${substitution}` values (device name, calibration constants,
Wi-Fi/MQTT passwords, sensor addresses, etc.) that are only ever
defined in the real per-device configs on the ESPHome dashboard, which
are not committed here (see the repo README's Notes section).

That means `esphome config` can't be run directly against, say,
`shelly/plug_s.yml` — it would fail immediately on undefined
substitutions, not because the file is broken.

Each file in this directory is a minimal wrapper that:

1. Defines dummy values for whatever substitutions the real package(s)
   expect (fake passwords, fake calibration numbers, fake sensor
   addresses, etc.) — **none of these are real secrets or real
   hardware values**, they exist purely so the config resolves.
2. Uses ESPHome's `packages:` mechanism to pull in the actual package
   file(s) from the repo, the same way a real device config would.
3. Adds whatever top-level boilerplate (`esphome:`, `esp8266:`/`esp32:`,
   `api:`) the real device config would normally supply, since several
   packages assume that scaffolding exists already.

The CI workflow (`.github/workflows/validate.yml`) runs
`esphome config <harness>.yaml` against each of these. That validates
YAML syntax, indentation, and ESPHome component schema — i.e. it will
catch typos, bad keys, and structural mistakes before they'd otherwise
only surface at flash time.

**What this does *not* catch:** C++ syntax errors inside `lambda:`
blocks. That only surfaces at `esphome compile`, which needs the full
toolchain and is much slower — not included here to keep CI fast.
Logic bugs (like the DBE manual-fanspeed NaN issue) also aren't
schema errors, so CI won't catch those either — it's a syntax/schema
safety net, not a correctness proof.

If you add a new package/device file outside `LVGL/`, add a matching
harness here and list it in the workflow's matrix.

<!-- CI trigger check: this comment can be removed once the workflow is confirmed working. -->
