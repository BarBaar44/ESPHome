# DBE - Radiator Booster Fan Controller

## What this is

DBE is a small ESPHome-based controller that sits on a radiator and drives a **fan to force convective airflow across the radiator fins**, boosting how much heat that radiator actually transfers into the room air - rather than relying purely on natural convection.

Radiators heat a room mostly by warming the air that passively rises past their fins. That passive convection is relatively slow and inefficient at getting heat away from the radiator and into the room's air volume. By adding a fan that blows air directly across/through the radiator whenever there's meaningful heat still available to extract, significantly more heat gets moved from the radiator into the room per unit time - the radiator itself doesn't need to run hotter, the room just absorbs the heat it's already producing more effectively.

This is one part of the broader multi-room heating system documented in the [**thermostat**](https://github.com/BarBaar44/thermostat) repo - DBE units are deployed per-room (see the `DBE_*` sensor naming across rooms like Front, Hallway, Master Left/Right, Office, Room Gijs, Room Tim in that repo) to squeeze more usable heat out of the same water temperature the boiler is already producing.

## Contents

- [How the fan decides (short version)](#how-the-fan-decides-short-version)
- [Why the control logic changed](#why-the-control-logic-changed)
- [How it works in detail](#how-it-works-in-detail)
- [Worked examples](#worked-examples)
- [Setting it up per device](#setting-it-up-per-device)
- [What appears in Home Assistant](#what-appears-in-home-assistant)
- [Tuning](#tuning)
- [Testing](#testing)
- [Hardware](#hardware-relay-switched-fan-power-not-mosfet)

## How the fan decides (short version)

Every 5 seconds the controller asks two questions:

1. **Is there heat in the radiator worth moving?** The radiator's average temperature has to be clearly above the room temperature (at least 10 °C), and the flow pipe at least 28 °C. Jaga uses the same 28 °C rule in its own DBE units.
2. **Does the room want heat?** The room temperature, read from the room's climate entity in Home Assistant, has to be below its setpoint.

Only when both answers are yes does the fan run. How fast depends on how much heat is available: a radiator that is only a little warmer than the room gets a quiet 25 %, a radiator that is much warmer gets up to the **Max Fan Speed** (80 % by default).

If Home Assistant is unreachable, or no room is configured yet, question 2 is skipped and the room is assumed to be at 20 °C. The unit then works on its own, purely on question 1.

## Why the control logic changed

The previous firmware used one signal: **deltaT, the difference between the two pipe sensors** (flow minus return). Small deltaT meant fan off, large deltaT meant fan fast. The reasoning was that a large drop across the radiator means lots of heat is available. That turns out to be backwards, and it caused three problems.

**1. A large deltaT means low water flow, not lots of heat.**
The temperature drop across a radiator depends mainly on how fast water moves through it. When the valve is wide open and hot water rushes through, the whole radiator is hot and the return is only a few degrees cooler than the flow: deltaT is *small*, yet this is exactly when there is the most heat to move. When a TRV throttles the valve because the room is nearly warm enough, water creeps through and cools on the way: deltaT becomes *large*, while there is little heat left and the room doesn't need it. The old logic ran the fan hardest in that second situation.

**2. It fed itself.**
A running fan pulls heat out of the radiator, so the return pipe gets colder, so deltaT grows, so the fan speeds up, which cools the return further. The fan's own effect pushed it towards full speed.

**3. It didn't know whether the room needed heat.**
Nothing in the old logic looked at the room. A bedroom already at its target would still get a running fan if the pipes showed a gap. That meant noise for nothing, and the fan working against the TRV that was trying to close.

Note that inverting deltaT (small gap = fast fan) doesn't fix it either: when the boiler is off, both pipes cool to room temperature and the gap is also near zero. An inverted rule would run the fan at full speed on a cold radiator.

**What the new logic uses instead:** how much warmer the radiator is than the room. That number (called **headroom** below) is large exactly when blowing air through the radiator moves real heat, and it falls naturally as the radiator cools. It doesn't depend on flow rate, and the fan pulling heat out lowers it, so the loop calms itself down instead of winding itself up. The room demand check is the second half: it stops the fan once the room is warm, so the fan and the TRV work together instead of against each other.

## How it works in detail

### Inputs

| Input | Source | Notes |
|---|---|---|
| Pipe temperatures | Two DS18B20 sensors on the radiator pipes (`Dallas 1`, `Dallas 2`), read every 30 s | The **warmer** of the two is taken as the flow pipe, the cooler as the return. You don't need to know which sensor sits on which pipe. |
| Room temperature | Attribute `current_temperature` of the room's climate entity in HA | Set per device with the `room_climate` substitution. |
| Room setpoint | Attribute `temperature` of that same climate entity | Empty when the room climate is Off, which counts as "no demand". |

From the pipe temperatures the controller computes:

- **Mean radiator temperature** = (flow + return) / 2
- **Headroom** = mean radiator temperature minus room temperature

### Gate 1: heat available (with hysteresis)

| | Fan allowed | Fan stops |
|---|---|---|
| Headroom | 10 °C or more | below 6 °C |
| Flow pipe | 28 °C or more | below 25 °C |

Both conditions are needed to switch on; either one dropping below its lower value switches off. The gap between the on and off values (hysteresis) keeps the fan from flicking on and off when a temperature hovers around a threshold.

### Gate 2: room demand (with hysteresis)

| | Fan allowed | Fan stops |
|---|---|---|
| Setpoint minus room temp | 0.1 °C or more (room below target) | 0.2 °C above target |

This works with the thermostat integration: when the room is the "winner" its TRVs get target + 1 °C overshoot buffer, but the fan stops at target + 0.2 °C. The fan helps the room reach its target quickly and then goes quiet while the TRV finishes the job.

### Speed

Once both gates are open, speed follows headroom:

| Headroom | Fan speed |
|---|---|
| 10 °C or less | 25 % (minimum, quiet) |
| between 10 and 35 °C | rises gradually (slight curve, exponent 1.1) |
| 35 °C or more | Max Fan Speed (default 80 %) |

The output to the fan is additionally limited to 90 % PWM duty in hardware config (`max_power: 90%`), as before.

### Standalone fallback

The unit falls back to working on its own when:

- the HA API connection is down, or
- the room temperature from HA is missing or implausible (outside 5 to 35 °C), which includes the default when no `room_climate` is configured.

In that case the room temperature is assumed to be 20 °C and gate 2 is treated as always open. The fan still only runs when there is real heat in the radiator, so the worst case is a fan running in a warm room, never a fan running on a cold radiator. The **Fan Reason** sensor shows `Running (standalone, no HA room data)` when this happens.

### Safety (unchanged from before)

- A pipe reading that is not a number, exactly 85 °C (the DS18B20 power on value), below 0 or above 95 °C counts as a fault.
- A pipe sensor that hasn't delivered a new reading for 90 s counts as a fault.
- Any fault stops the fan. The relay then cuts fan power completely.

### Modes

| Mode | Behaviour |
|---|---|
| `AUTO` | The logic above. |
| `MANUAL` | Runs at **Manual Fan Speed** (below 1 % = off). Ignores sensors and HA. |
| `OFF` | Fan off, whatever the conditions. New in this version. |

The selected mode now survives a reboot (`restore_value: true`).

## Worked examples

These come straight from the automated test (see [Testing](#testing)), room setpoint 20 °C unless noted:

| Situation | Flow / return | Room | Headroom | Fan |
|---|---|---|---|---|
| Boiler off, pipes near room temperature | 20 / 19.5 | 19 | 0.8 | off, no heat |
| Boiler fires, room cold | 55 / 48 | 18 | 33.5 | 76 % |
| Low temperature water | 35 / 30 | 19 | 13.5 | 31 % |
| Radiator cooling down | 30 / 27 | 19 | 9.5 | 25 % (still on, hysteresis) |
| Radiator nearly cold | 25 / 23 | 19 | 5.0 | off, no heat |
| Throttled TRV, room still below target | 50 / 30 | 19.8 | 20.2 | 46 % |
| Throttled TRV, room above target | 50 / 30 | 20.3 | 19.7 | off, room at setpoint |
| Room climate set to Off in HA | 55 / 48 | 18 | 33.5 | off |
| HA unreachable, hot radiator | 55 / 48 | (20 assumed) | 31.5 | 72 %, standalone |

The "throttled TRV" rows show the difference with the old firmware. Old logic: a 20 °C gap between the pipes meant about 80 % fan, whether or not the room needed heat. New logic: the fan helps while the room is below target and stops when it isn't.

## Setting it up per device

Each device config in `base/` pulls in this package. To link a device to its room, add one line to its `substitutions:` on the ESPHome dashboard:

```yaml
substitutions:
  device_name: dbeoffice
  # ... existing lines unchanged ...
  room_climate: "climate.office"   # the room climate entity from the thermostat integration
```

**Finding the right entity:** in HA go to Developer Tools > States and filter on `climate.`. Use the room climate created by the Cloudless Multiroom Thermostat integration (one per room), not a TRV climate. Its attributes should include `current_temperature` and `temperature`.

Suggested mapping (verify the entity IDs in your HA, they depend on the room names):

| Device config | Room climate |
|---|---|
| `dbe-office.yaml` | the Office room |
| `dbefront.yaml` | the Living Room |
| `dbe-hallway.yaml` | the Living Room (the hallway TRV follows the living room setpoint) |
| `dbe-master-left.yaml`, `dbe-master-right.yaml` | the Master Bedroom |
| `dbe-room-gijs.yaml` | Room Gijs |
| `dbe-room-tim.yaml` | Room Tim |

**Allow the device to read HA:** ESPHome devices can read HA states by default. If a device shows `Running (standalone, no HA room data)` while HA is up, check that the entity ID is correct.

**Without this line** the device keeps working in standalone mode. Nothing breaks on update; you can add the room links one device at a time.

## What appears in Home Assistant

Existing entities keep their names, so dashboards and history keep working: `Dallas 1`, `Dallas 2`, `Fan RPM`, `Fan Mode`, `Manual Fan Speed`.

New entities:

| Entity | What it tells you |
|---|---|
| `Fan Speed` | The commanded speed in %. |
| `Max Fan Speed` (number) | Upper limit in AUTO mode, 25 to 100, default 80. Remembered across reboots. |
| `Fan Reason` (diagnostic) | Why the fan is on or off right now, e.g. `Off (room at setpoint)`. |
| `Flow Temperature` (diagnostic) | The warmer pipe sensor. |
| `Return Temperature` (diagnostic) | The cooler pipe sensor. |
| `Room Temperature Used` (diagnostic) | The room temperature the logic is using (from HA, or 20 in standalone). |
| `Heat Headroom` (diagnostic) | Radiator mean minus room temperature. The main control signal. |

Diagnostics update every 30 s; the control loop itself still runs every 5 s.

**Quiet nights in bedrooms**: lower Max Fan Speed from an HA automation, for example:

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
| `headroom_on` / `headroom_off` | 10 / 6 °C | run the fan only on hotter radiators | run it longer on cooling radiators |
| `flow_min_on` / `flow_min_off` | 28 / 25 °C | same, as an absolute floor | same |
| `gap_on` / `gap_off` | 0.1 / -0.2 °C | start the fan only when the room is further below target | keep it running further past target |
| `hr_min` / `hr_max` | 10 / 35 °C | need more heat for higher speeds (quieter) | reach high speed sooner |
| `speed_min` | 25 % | if fans stall at low speed | if 25 % is audible |
| `room_default` | 20 °C | standalone assumes a warmer room (fan runs less) | the opposite |

Keep the `_off` value of each pair below its `_on` value, otherwise the fan will flicker. After changing a constant, run the test (next section) and update its expectations if behaviour changed on purpose.

**Watching it work:** in HA, put `Heat Headroom`, `Fan Speed`, `Fan Reason` and the room temperature on one history graph. If the fan regularly starts and stops within a few minutes, widen the hysteresis gap.

## Testing

Two checks run in GitHub Actions on every change to `DBE/`:

1. **Schema check** (`.ci/dbe.yaml`): `esphome config`, catches YAML and component errors.
2. **Logic test** (`.ci/dbe-logic/`): copies the `control_fans` lambda out of `DBEmain.yml`, compiles it on the build machine against small stand-ins for ESPHome, and runs the scenarios from the [Worked examples](#worked-examples) table (plus sensor faults, modes and the speed cap). This catches C++ mistakes in the lambda and changes in behaviour, neither of which the schema check sees.

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
