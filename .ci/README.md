# CI

The workflow in `.github/workflows/validate.yml` runs three checks on
every push to `main` and every PR that touches configs or CI.

## 1. Package harnesses (`validate`)

The files under `DBE/`, `bluetooth-proxy/`, `generic/`, `shelly/`,
`sonoff/`, `USB-Switch/` and `LVGL/` are **reusable package fragments**,
not flashable configs. They rely on `${substitution}` values (device
name, calibration constants, passwords, sensor addresses, etc.) that
only the device files in `base/` define, so `esphome config` can't run
on, say, `shelly/plug_s.yml` directly.

Each harness in this directory (and `LVGL/*/ci-harness.yaml`) is a
minimal wrapper that:

1. Defines dummy values for the substitutions the package(s) expect.
   **None of these are real secrets or real hardware values**; they
   exist purely so the config resolves.
2. Pulls in the package file(s) with `!include`, the way a device does
   through `packages:`.
3. Adds the top-level boilerplate (`esphome:`, `esp8266:`/`esp32:`)
   that a device file would normally supply.

If you add a new package, add a matching harness and list it in the
workflow's `validate` matrix.

## 2. Device configs (`validate-base`)

`.ci/validate-base.sh` runs `esphome config` on the real device files in
`base/`, one CI job per device (the list is built automatically, so a
new device needs no workflow change). It works without the HA dashboard
because:

* `secrets.yaml` is generated: every `!secret` gets a dummy value (a
  valid base64 key for names ending in `_key`).
* Package URLs pointing at this repo are rewritten to the checkout
  (`file://`), so a PR is validated against its **own** package files,
  not against `main`.

Run it locally from the repo root with `.ci/validate-base.sh` (all
devices) or `.ci/validate-base.sh base/<device>.yaml`. It clones the
committed HEAD, so commit first.

## 3. DBE logic (`dbe-logic`)

`.ci/dbe-logic/run.sh` extracts the DBE `control_fans` lambda, compiles
it on the host and runs behaviour scenarios through it. This catches C++
errors and logic regressions in that lambda. See `dbe-logic/harness.cpp`.

## What CI does not catch

`esphome config` checks YAML syntax and component schema, not C++
inside other `lambda:` blocks; those only fail at `esphome compile`,
which is not run here to keep CI fast. Logic bugs outside the DBE test
are not schema errors either. CI is a safety net, not a correctness
proof.

## ESPHome version

CI installs the version pinned in `/requirements.txt`. Dependabot opens
a PR when a new ESPHome release is out, and `auto-merge.yml` approves
and merges it once all checks pass (this needs "Allow GitHub Actions to
create and approve pull requests" enabled in the repo's Actions settings). This only sets the version CI validates with; the
ESPHome version that builds firmware is the one on the HA dashboard.
