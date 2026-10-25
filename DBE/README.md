# DBE - Radiator Booster Fan Controller

## What this is

DBE is a small ESPHome-based controller that sits on a radiator and drives a **fan to force convective airflow across the radiator fins**, boosting how much heat that radiator actually transfers into the room air - rather than relying purely on natural convection.

Radiators heat a room mostly by warming the air that passively rises past their fins. That passive convection is relatively slow and inefficient at getting heat away from the radiator and into the room's air volume. By adding a fan that blows air directly across/through the radiator whenever there's meaningful heat still available to extract, significantly more heat gets moved from the radiator into the room per unit time - the radiator itself doesn't need to run hotter, the room just absorbs the heat it's already producing more effectively.

This is one part of the broader multi-room heating system documented in the [**thermostat**](https://github.com/BarBaar44/thermostat) repo - DBE units are deployed per-room (see the `DBE_*` sensor naming across rooms like Front, Hallway, Master Left/Right, Office, Room Gijs, Room Tim in that repo) to squeeze more usable heat out of the same water temperature the boiler is already producing.

> **Not a Jaga product.** This is a fully custom board. It shares only the name with Jaga's DBE units; the hardware, firmware and control logic are unrelated.

## Contents

- [How the fan decides (short version)](#how-the-fan-decides-short-version)
- [Why delta T, and why inverted](#why-delta-t-and-why-inverted)
- [What changed in this version](#what-changed-in-this-version)
- [How it works in detail](#how-it-works-in-detail)
- [Worked examples](#worked-examples)
- [What appears in Home Assistant](#what-appears-in-home-assistant)
- [Tuning](#tuning)
- [Testing](#testing)
- [Hardware](#hardware-relay-switched-fan-power-not-mosfet)

## How the fan decides (short version)

Every 5 seconds the controller compares its two pipe temperatures: **supply** (water going into the radiator) and **return** (water going back to the boiler). The difference between them is **delta T**.

1. **Is there warm water?** The fan starts when the supply pipe reaches 28 °C while warm water is actually arriving (delta T at least 1.5 °C). Once running, it keeps going until the supply pipe drops below 24 °C, so the heat left in the radiator after the boiler stops is still blown into the room.
2. **How fast?** The smaller delta T, the faster the fan. At a delta T of 10 °C or more the radiator already cools the water well and the fan runs at its quiet minimum (25 %). As delta T shrinks towards 2 °C the fan speeds up to the **Max Fan Speed** (80 % by default).

The board works entirely on its own. Home Assistant only shows what it is doing and holds its settings; the fan behaves the same when HA is down.

## Why delta T, and why inverted

**The goal is a cold return.** The boiler (Atag, max supply 45 °C, modulated by the [thermostat](https://github.com/BarBaar44/thermostat) PID) works most efficiently when the water coming back is well below the water going out. The design target is a delta T of at least 10 °C.

**At low supply temperatures radiators struggle.** Radiators are rated at 75/65 °C. At 45 °C or lower they give off only a fraction of that by natural convection, so water can pass through and come back almost as warm as it left. A fan forcing air across the fins makes the radiator shed much more of that heat.

**So the fan works hardest when delta T is small.** A small delta T means the radiator is not taking enough heat out of the water. Speeding up the fan pulls more heat into the room, the return gets colder, and delta T grows. As delta T approaches the 10 °C target the fan slows to its minimum again. The loop settles by itself: the fan's own effect (a colder return) is what slows it down.

**Why the extra start condition is needed.** A small delta T also occurs when there is no heat at all: with the boiler off, both pipes sit at room temperature and delta T is near zero. On its own, an inverted delta T would then run the fan at full speed on a cold radiator. The supply temperature gate prevents that, and requiring a measurable delta T to *start* prevents a pipe that is merely warm (a hot summer day) from starting the fan.

## What changed in this version

The previous firmware had the speed curve the wrong way round compared to the design intent: speed **rose** with delta T (25 % at 2 °C up to 80 % at 20 °C), and the fan switched off below 0.8 °C. So it slowed down exactly when the return was too warm. This version:

| | Before | Now |
|---|---|---|
| Speed vs delta T | faster as delta T grows | **faster as delta T shrinks**, minimum at 10 °C or more |
| Starts when | delta T above 1.5 °C | supply at least 28 °C **and** delta T at least 1.5 °C |
| Stops when | delta T below 0.8 °C | supply below 24 °C (uses residual heat) |
| Cold radiator | fan off (delta T near zero) | fan off (supply gate) |
| Modes | AUTO, MANUAL | AUTO, MANUAL, **OFF**; mode remembered across reboots |
| Speed limit | fixed 80 % | **Max Fan Speed** setting, 25 to 100 % |
| Diagnostics | fan RPM only | supply, return, delta T, fan speed, reason |

Unchanged: sensor fault handling, the relay hard off, PWM output and pins, and the existing entity names.

## How it works in detail

### Inputs

Two DS18B20 temperature sensors, one clamped on the supply pipe and one on the return pipe (`Dallas 1`, `Dallas 2`), read every 30 s. The **warmer** reading is taken as supply and the cooler as return, so it doesn't matter which sensor sits on which pipe.

**Terminology:** *supply* is the pipe bringing hot water into the radiator (heating installers often call it the *flow* pipe), *return* is the pipe taking the cooled water back to the boiler. Both are temperatures; nothing here measures how much water flows.

### Gate: is there warm water? (with hysteresis)

| | Value |
|---|---|
| Start | supply 28 °C or more **and** delta T 1.5 °C or more |
| Stop | supply below 24 °C |

The gap between start and stop (hysteresis) keeps the fan from flicking on and off. After a stop, the fan only restarts when the start condition is met again.

### Speed: smaller delta T, faster fan

| Delta T | Fan speed |
|---|---|
| 10 °C or more | 25 % (minimum) |
| between 10 and 2 °C | rises gradually (slight curve, exponent 1.1) |
| 2 °C or less | Max Fan Speed (default 80 %) |

The PWM output is additionally limited to 90 % duty in hardware config (`max_power: 90%`), as before.

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
| Summer, warm pipes, no water moving | 29 / 28.8 | 0.2 | off (no start) |
| Warm water arriving | 30 / 22 | 8 | 37 % |
| Radiator cools the water well | 40 / 28 | 12 | 25 % (minimum) |
| Return too warm | 40 / 34 | 6 | 51 % |
| Return almost as warm as supply | 40 / 39 | 1 | 80 % |
| Boiler stopped, radiator still warm | 30 / 29 | 1 | 80 % (residual heat) |
| Still cooling | 25 / 24.5 | 0.5 | 80 % |
| Supply below 24 °C | 23.8 / 23.5 | 0.3 | off |

## What appears in Home Assistant

Existing entities keep their names, so dashboards and history keep working: `Dallas 1`, `Dallas 2`, `Fan RPM`, `Fan Mode`, `Manual Fan Speed`.

New entities:

| Entity | What it tells you |
|---|---|
| `Fan Speed` | The commanded speed in %. |
| `Max Fan Speed` (number) | Upper limit in AUTO mode, 25 to 100, default 80. Stored on the device. |
| `Fan Reason` (diagnostic) | Why the fan is on or off right now, e.g. `Running (deltaT below target)`. |
| `Supply Temperature` (diagnostic) | The warmer pipe sensor: water going into the radiator. |
| `Return Temperature` (diagnostic) | The cooler pipe sensor: water leaving the radiator. |
| `Delta T` (diagnostic) | Supply minus return. The main control signal. |

Diagnostics update every 30 s; the control loop itself runs every 5 s. All settings are stored on the device, so they survive a reboot even when HA is unavailable.

**Quiet nights in bedrooms** (optional, the board doesn't need it): lower Max Fan Speed from an HA automation, for example:

```yaml
alias: DBE bedrooms quiet at night
triggers:
  - trigger: time
    at: "22:00:00"
    id: night
  - trigger: time
    at: "07:00:00"
    id: day
actions:
  - action: number.set_value
    target:
      entity_id:
        - number.dbemasleft_max_fan_speed
        - number.dbemasright_max_fan_speed
    data:
      value: "{{ 40 if trigger.id == 'night' else 80 }}"
```

Check the actual number entity IDs in HA before using this.

## Tuning

All thresholds are constants at the top of the `control_fans` script in `DBEmain.yml`, each with a comment:

| Constant | Default | Raise it to... | Lower it to... |
|---|---|---|---|
| `supply_on` | 28 °C | start only on warmer water | start sooner when the PID runs a low supply |
| `delta_on` | 1.5 °C | demand clearer evidence of water moving before starting | start on smaller signs of warm water |
| `supply_off` | 24 °C | stop earlier (less residual run time, less noise) | squeeze out more residual heat |
| `delta_target` | 10 °C | aim for a colder return (fan runs faster for longer) | accept a warmer return (quieter) |
| `delta_full` | 2 °C | reach full speed sooner | reach full speed only when delta T is very small |
| `speed_min` | 25 % | if fans stall at low speed | if 25 % is audible |

Keep `supply_off` below `supply_on`, and `delta_full` below `delta_target`. After changing a constant, run the test (next section) and update its expectations if behaviour changed on purpose.

**Two things to watch in practice:**
- **Mild days:** the PID may hold the supply below 28 °C for long periods. If the fans then never start, lower `supply_on`.
- **Residual run time:** after the boiler stops, delta T is small, so the fan runs at Max Fan Speed until the supply drops below 24 °C. If that is too long or too loud, raise `supply_off` or use the Max Fan Speed setting.

**Watching it work:** in HA, put `Supply Temperature`, `Return Temperature`, `Delta T` and `Fan Speed` on one history graph. While the boiler runs, delta T should settle near 10 °C at a moderate fan speed. If the fan regularly starts and stops within a few minutes, widen the gap between `supply_on` and `supply_off`.

## Testing

Two checks run in GitHub Actions on every change to `DBE/`:

1. **Schema check** (`.ci/dbe.yaml`): `esphome config`, catches YAML and component errors.
2. **Logic test** (`.ci/dbe-logic/`): copies the `control_fans` lambda out of `DBEmain.yml`, compiles it on the build machine against small stand-ins for ESPHome, and runs the scenarios from the [Worked examples](#worked-examples) table plus speed ordering, swapped sensors, the speed cap, sensor faults and modes. This catches C++ mistakes in the lambda and changes in behaviour, neither of which the schema check sees.

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
