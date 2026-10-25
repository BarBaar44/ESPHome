# DBE - Radiator Booster Fan Controller

## What this is

DBE is a small ESPHome-based controller that sits on a radiator and drives a **fan to force convective airflow across the radiator fins**, boosting how much heat that radiator actually transfers into the room air - rather than relying purely on natural convection.

Radiators heat a room mostly by warming the air that passively rises past their fins. That passive convection is relatively slow and inefficient at getting heat away from the radiator and into the room's air volume. By adding a fan that blows air directly across/through the radiator whenever there's meaningful heat still available to extract, significantly more heat gets moved from the radiator into the room per unit time - the radiator itself doesn't need to run hotter, the room just absorbs the heat it's already producing more effectively.

This is one part of the broader multi-room heating system documented in the [**thermostat**](https://github.com/BarBaar44/thermostat) repo - DBE units are deployed per-room (see the `DBE_*` sensor naming across rooms like Front, Hallway, Master Left/Right, Office, Room Gijs, Room Tim in that repo) to squeeze more usable heat out of the same water temperature the boiler is already producing.

> **Not a Jaga product.** This is a fully custom board. It shares only the name with Jaga's DBE units; the hardware, firmware and control logic are unrelated.

## Contents

- [How the fan decides (short version)](#how-the-fan-decides-short-version)
- [The two speed curves](#the-two-speed-curves)
- [What changed in this version](#what-changed-in-this-version)
- [How it works in detail](#how-it-works-in-detail)
- [Worked examples](#worked-examples)
- [Comparing the curves](#comparing-the-curves)
- [What appears in Home Assistant](#what-appears-in-home-assistant)
- [Tuning](#tuning)
- [Testing](#testing)
- [Hardware](#hardware-relay-switched-fan-power-not-mosfet)

## How the fan decides (short version)

Every 5 seconds the controller compares its two pipe temperatures: **supply** (water going into the radiator) and **return** (water going back to the boiler). The difference between them is **delta T**.

1. **Is there warm water?** The fan starts when the supply pipe reaches 25 °C while warm water is actually arriving (delta T at least 1.5 °C). Once running, it keeps going until the supply pipe drops below 24 °C, so the heat left in the radiator after the boiler stops is still blown into the room.
2. **How fast?** That depends on the **Speed Curve** setting:
   - **Rising** (default, the original behaviour): faster as delta T grows.
   - **Falling**: faster as delta T shrinks, aiming for a colder return.

The board works entirely on its own. Home Assistant only shows what it is doing and holds its settings; the fan behaves the same when HA is down.

## The two speed curves

**Rising** is how the board has always worked: a large delta T means the radiator is busy shedding heat (typically right after hot water reaches a cold radiator), so the fan helps most then. Speed goes from 25 % at a delta T of 2 °C up to **Max Fan Speed** (80 % by default) at 20 °C.

**Falling** follows a different idea: the boiler (Atag, max supply 45 °C, modulated by the [thermostat](https://github.com/BarBaar44/thermostat) PID) is most efficient with a cold return, and the design target is a delta T of at least 10 °C. At these low supply temperatures radiators give off only a fraction of their rated output (rated at 75/65 °C), so water can come back almost as warm as it left. A small delta T then means the radiator isn't taking enough heat out of the water, so the fan speeds up: Max Fan Speed at a delta T of 2 °C or less, down to 25 % at 10 °C or more. The fan's own effect (a colder return, so a larger delta T) slows it again, so the loop settles by itself.

**Which is better is not known yet.** With supply capped at 45 °C the boiler condenses either way, so the efficiency difference from a colder return is small; the bigger effect is probably how much heat reaches the room at low supply temperatures. Falling runs the fans faster more often, so it is also louder. That's why both curves are in the firmware and can be compared on the same radiator (see [Comparing the curves](#comparing-the-curves)).

**Why the start condition matters for Falling.** Delta T is also near zero when there is no heat at all: with the boiler off both pipes sit at room temperature. Without the gate, Falling would run the fan at full speed on a cold radiator. The supply temperature gate prevents that, and requiring a measurable delta T to *start* prevents a pipe that is merely warm (a hot summer day) from starting the fan.

## What changed in this version

| | Before | Now |
|---|---|---|
| Speed vs delta T | faster as delta T grows | selectable: **Rising** (same as before, default) or **Falling** |
| Starts when | delta T above 1.5 °C | supply at least 25 °C **and** delta T at least 1.5 °C |
| Stops when | delta T below 0.8 °C | supply below 24 °C (uses residual heat) |
| Modes | AUTO, MANUAL | AUTO, MANUAL, **OFF**; mode remembered across reboots |
| Speed limit | fixed 80 % | **Max Fan Speed** setting, 25 to 100 % |
| Diagnostics | fan RPM only | supply, return, delta T, fan speed, reason |

**The one change in normal heating behaviour** (with Speed Curve left on Rising) is the stop condition. Before, the fan stopped once delta T fell below 0.8 °C. After the boiler stops, both pipes cool together and delta T collapses, so the fan used to stop while the radiator was still warm. Now it keeps running (at minimum speed on the Rising curve) until the supply pipe drops below 24 °C.

Unchanged: sensor fault handling, the relay hard off, PWM output and pins, and the existing entity names.

## How it works in detail

### Inputs

Two DS18B20 temperature sensors, one clamped on the supply pipe and one on the return pipe (`Dallas 1`, `Dallas 2`), read every 30 s. The **warmer** reading is taken as supply and the cooler as return, so it doesn't matter which sensor sits on which pipe.

**Terminology:** *supply* is the pipe bringing hot water into the radiator (heating installers often call it the *flow* pipe), *return* is the pipe taking the cooled water back to the boiler. Both are temperatures; nothing here measures how much water flows.

### Gate: is there warm water? (with hysteresis)

| | Value |
|---|---|
| Start | supply 25 °C or more **and** delta T 1.5 °C or more |
| Stop | supply below 24 °C |

The start threshold sits 1 °C above the stop threshold so that two sensors reading slightly differently on a cold pipe (DS18B20s are accurate to about ±0.5 °C) can't fake a delta T and start the fan. After a stop, the fan only restarts when the start condition is met again.

### Speed

| Delta T | Rising | Falling |
|---|---|---|
| 1 °C | 25 % | Max Fan Speed (80 %) |
| 6 °C | 36 % | 51 % |
| 10 °C | 48 % | 25 % |
| 15 °C | 64 % | 25 % |
| 20 °C or more | Max Fan Speed (80 %) | 25 % |

Both curves rise gradually between their end points (slight curve, exponent 1.1). Values above assume Max Fan Speed at 80 %. The PWM output is additionally limited to 90 % duty in hardware config (`max_power: 90%`), as before.

### Safety

- A pipe reading that is not a number, exactly 85 °C (the DS18B20 power on value), below 0 or above 95 °C counts as a fault.
- A pipe sensor that hasn't delivered a new reading for 90 s counts as a fault.
- Any fault stops the fan, and the relay cuts fan power completely. After a fault the start condition has to be met again.

### Modes

| Mode | Behaviour |
|---|---|
| `AUTO` | The logic above. |
| `MANUAL` | Runs at **Manual Fan Speed** (below 1 % = off). Ignores the sensors. |
| `OFF` | Fan off, whatever the temperatures. |

## Worked examples

These come straight from the automated test (see [Testing](#testing)):

| Situation | Supply / return | Delta T | Fan |
|---|---|---|---|
| Boiler off, cold pipes | 19 / 18.5 | 0.5 | off |
| Summer, warm pipes, no water moving | 27 / 26.8 | 0.2 | off (no start) |
| Sensor offset on a cold pipe | 21 / 19.5 | 1.5 | off (supply below 25) |
| Mild day, low supply | 26 / 22 | 4 | Rising 30 % |
| Return almost as warm as supply | 40 / 39 | 1 | Rising 25 %, Falling 80 % |
| Radiator cools the water well | 40 / 28 | 12 | Rising 54 %, Falling 25 % |
| Boiler stopped, radiator still warm | 28 / 27.5 | 0.5 | Rising 25 %, Falling 80 % |
| Supply below 24 °C | 23.8 / 23.5 | 0.3 | off |

## Comparing the curves

With one radiator doing most of the heating, comparing curves across radiators doesn't work. Instead, switch the curve on the same radiator **in time**: alternate Rising and Falling day by day, then compare days with similar outdoor temperatures.

An HA automation that does this (copy it into your own config; check the entity IDs in HA first, they depend on each device's name):

```yaml
alias: DBE speed curve comparison
description: Alternates the DBE Speed Curve between Rising and Falling every day, to compare them on the same radiators.
triggers:
  - trigger: time
    at: "00:00:30"
  - trigger: homeassistant
    event: start
actions:
  - action: select.select_option
    target:
      entity_id:
        - select.dbe_livingroom_front_speed_curve  # example, use your own IDs
    data:
      option: "{{ 'Falling' if (now().strftime('%j') | int) % 2 == 1 else 'Rising' }}"
mode: single
```

It sets the curve from the day of the year (odd days Falling, even days Rising), so a missed run or a restart can't leave units out of step. Disable the automation when you're done and set the winning curve.

**What to compare** (all already in HA):
- Return temperature and delta T on that radiator while the boiler runs.
- How long that room takes to reach setpoint after the morning warm up.
- Gas use per degree day for the house.
- Fan noise.

## What appears in Home Assistant

Existing entities keep their names, so dashboards and history keep working: `Dallas 1`, `Dallas 2`, `Fan RPM`, `Fan Mode`, `Manual Fan Speed`.

New entities:

| Entity | What it tells you |
|---|---|
| `Speed Curve` (select) | `Rising` (default) or `Falling`. Stored on the device. |
| `Max Fan Speed` (number) | Upper limit in AUTO mode, 25 to 100, default 80. Stored on the device. |
| `Fan Speed` | The commanded speed in %. |
| `Fan Reason` (diagnostic) | Why the fan is on or off right now, e.g. `Running (falling curve)`. |
| `Supply Temperature` (diagnostic) | The warmer pipe sensor: water going into the radiator. |
| `Return Temperature` (diagnostic) | The cooler pipe sensor: water leaving the radiator. |
| `Delta T` (diagnostic) | Supply minus return. The main control signal. |

Diagnostics update every 30 s; the control loop itself runs every 5 s. All settings are stored on the device, so they survive a reboot even when HA is unavailable.

**Quiet nights in bedrooms** (optional): lower Max Fan Speed from an HA automation at night, the same way as the curve comparison above but with `number.set_value`.

## Tuning

All thresholds are constants at the top of the `control_fans` script in `DBEmain.yml`, each with a comment:

| Constant | Default | Raise it to... | Lower it to... |
|---|---|---|---|
| `supply_on` | 25 °C | start only on warmer water | start sooner when the PID runs a very low supply |
| `delta_on` | 1.5 °C | demand clearer evidence of water moving before starting | start on smaller signs of warm water |
| `supply_off` | 24 °C | stop earlier (less residual run time, less noise) | squeeze out more residual heat |
| `rise_low` / `rise_high` | 2 / 20 °C | Rising: need a larger delta T for the same speed | Rising: reach high speed sooner |
| `fall_target` | 10 °C | Falling: aim for a colder return (fan faster for longer) | Falling: accept a warmer return (quieter) |
| `fall_full` | 2 °C | Falling: reach full speed sooner | Falling: reach full speed only at a very small delta T |
| `speed_min` | 25 % | if fans stall at low speed | if 25 % is audible |

Keep `supply_off` below `supply_on`. After changing a constant, run the test (next section) and update its expectations if behaviour changed on purpose.

**Residual run time:** after the boiler stops, delta T is small. On Rising the fan then runs at its minimum, on Falling at Max Fan Speed, until the supply drops below 24 °C. If that is too long or too loud, raise `supply_off` or lower Max Fan Speed.

## Testing

Two checks run in GitHub Actions on every change to `DBE/`:

1. **Schema check** (`.ci/dbe.yaml`): `esphome config`, catches YAML and component errors.
2. **Logic test** (`.ci/dbe-logic/`): copies the `control_fans` lambda out of `DBEmain.yml`, compiles it on the build machine against small stand-ins for ESPHome, and runs scenarios: the gate (cold pipes, summer, sensor offset, mild day), both curves, swapped sensors, residual heat, the speed cap, sensor faults and modes. This catches C++ mistakes in the lambda and changes in behaviour, neither of which the schema check sees.

Run the logic test locally with `.ci/dbe-logic/run.sh` (needs `g++` and `python3` with PyYAML).

Not covered: a full firmware build for the ESP8266. The first real compile happens on the ESPHome dashboard, so check the first device's logs after updating.

## Hardware: relay-switched fan power (not MOSFET)

> **Note on naming:** the hardware repo/files are still named "MosFet" for historical continuity (that's what the board was originally designed around, and what it's still commonly referred to as in my own notes) - but the design actually in use, described below, is **relay-switched**, not a direct MOSFET drive. This section reflects the real, current hardware.

The `switch: platform: gpio, id: powerswitch` in `DBEmain.yml` is the ESP GPIO that ultimately triggers the fan's power state (wired to the fan's `on_turn_on`/`on_turn_off` events as a hard cutoff, separate from PWM speed control). What's actually between that GPIO and the fan went through a few iterations before landing on the current design.

### How the design got here

This board's switching stage evolved out of a long troubleshooting thread on the Home Assistant Community forum ([*"3 pin pc fan control via mosfet"*](https://community.home-assistant.io/t/3-pin-pc-fan-control-via-mosfet/738676), started by another user, with a lot of the later back-and-forth being me working through my own build under my forum username `beurdy`), credited here since a good chunk of the reasoning below came directly out of that discussion (with particular thanks to forum user **Karosm** for the transistor/relay guidance):

1. **First attempt - direct MOSFET switching.** An IRFZ44N MOSFET was tried initially, but it's not a logic-level MOSFET, so it never switched reliably from a 3.3V ESP GPIO. A true logic-level part (e.g. IRLZ44N) or a 3.3V-compatible MOSFET driver module would have been needed to make that approach work at all.
2. **Second attempt - direct transistor switching (NPN + PNP pair).** Instead of a MOSFET, an NPN transistor (BC337) switched the fan's ground return, driven by a small base resistor straight off the ESP GPIO. This alone wasn't enough: some fans (mine included) use the PWM signal wire as an alternate ground return path, so cutting only the main ground didn't fully stop the fan. The fix was a second stage - a PNP transistor (BC327) also gating the PWM/12V side - so both paths were switched together. This worked for small numbers of fans.
3. **The problem that forced a redesign: current at scale.** Once this board needed to drive up to 15 fans on a single unit (~2.5A combined load), the small BC327 transistor stage couldn't handle it - it overheated and the board started smelling during testing. Direct transistor switching was fine for a couple of fans, but not for a bank of them.
4. **Final, current design - relay-switched.** The BC337 NPN stage (fed from the ESP GPIO through a ~1K base resistor) no longer switches the fan directly - it switches the **coil of a 12V relay** instead, and the relay does the actual high-current switching of the fan's 12V line:
   - Relay: 12V coil, rated for several amps above the expected load for margin (the specific part used, and its exact footprint, is in the [EasyEDA](https://github.com/BarBaar44/EasyEDA) project files).
   - Relay coil wiring: 12V+ tied directly to one coil pin, the other coil pin switched to ground through the BC337.
   - A flyback/snubber diode across the relay coil (protecting the BC337 from the coil's inductive kickback when it switches off).
   - A separate flyback diode across the fan's own 12V leads (protecting against the fan motor's own inductive kickback).
   - The relay's normally-open contact feeds the fan's 12V/positive line, so **fans default to off** if the relay isn't energised - a fail-safe rather than fail-on state.
   - PWM speed control is unrelated to this switching stage - it's driven directly from a separate PWM-capable ESP GPIO straight to the fan's PWM wire, exactly as described in the firmware logic section above. The relay only ever handles the hard on/off power cutoff.

### Known-tricky bits worth revisiting

Electronics isn't my primary background, so a few details from this design are worth re-deriving/re-verifying rather than taking for granted next time this board gets touched:

- The exact reasoning for **why the PWM wire needed its own switching stage** in the earlier transistor-only design (some fans use it as an alternate ground return) - worth double-checking this doesn't resurface in a different form with the relay design if a new fan model behaves unexpectedly.
- The **flyback diode placement and rating** on both the relay coil and the fan leads - got this wrong once during the transistor-stage build (a diode placed backwards created a short) before correcting it. Worth a careful re-check against a fresh schematic read, not just memory, before any future revision.
- The **base resistor value** on the BC337 (finalized at ~1K, chosen partly for component availability rather than being a hard requirement) - fine as-is, but worth understanding *why* that range works if choosing a different transistor or resistor value later.
- An unfinished idea from the same forum discussion: replacing the onboard 12V→5V step-down regulator section with a more compact module, to shrink the board size. Not yet implemented - the plan is to wait until existing boards actually need replacing before attempting that redesign.

## Hardware design files

The physical board (relay-switched fan driver, described above) is designed in EasyEDA and lives in a separate repo:

➡️ **[BarBaar44/EasyEDA](https://github.com/BarBaar44/EasyEDA)** - schematic/PCB source project, Gerber manufacturing files, and a rendered PCB preview image. This is the authoritative source for the exact component values, footprints, and routing - more reliable than reconstructing it from memory or from the forum thread's inline work-in-progress screenshots.

That repo's README also has a pointer to the enclosure design (FreeCAD) for this same board.

## Related

- [BarBaar44/thermostat](https://github.com/BarBaar44/thermostat) - the overall multi-room heating system this fan controller supports, including the virtual-thermostat/TRV/boiler logic that decides *when* and *how hot* each room's radiator should be running in the first place.
- [Home Assistant Community: "3 pin pc fan control via mosfet"](https://community.home-assistant.io/t/3-pin-pc-fan-control-via-mosfet/738676) - the forum thread where the relay-switched design above was worked out.
