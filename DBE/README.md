# DBE - Radiator Booster Fan Controller

## What this is

DBE is a small ESPHome-based controller that sits on a radiator and drives a **fan to force convective airflow across the radiator fins**, boosting how much heat that radiator actually transfers into the room air - rather than relying purely on natural convection.

Radiators heat a room mostly by warming the air that passively rises past their fins. That passive convection is relatively slow and inefficient at getting heat away from the radiator and into the room's air volume. By adding a fan that blows air directly across/through the radiator whenever there's meaningful heat still available to extract, significantly more heat gets moved from the radiator into the room per unit time - the radiator itself doesn't need to run hotter, the room just absorbs the heat it's already producing more effectively.

This is one part of the broader multi-room heating system documented in the [**thermostat**](https://github.com/BarBaar44/thermostat) repo - DBE units are deployed per-room (see the `DBE_*` sensor naming across rooms like Front, Hallway, Master Left/Right, Office, Room Gijs, Room Tim in that repo) to squeeze more usable heat out of the same water temperature the boiler is already producing.

## Firmware logic (`DBEmain.yml`)

- **Two Dallas (DS18B20) temperature sensors** (`temp_1`, `temp_2`) on a shared OneWire bus, each polled every 30s, measuring the radiator's **flow pipe** (hot water entering the radiator) and **return pipe** (cooled water leaving it) respectively.
- Every **5 seconds**, an `interval:` trigger runs the `control_fans` script, which computes `deltaT = |temp_1 - temp_2|` - the flow-vs-return temperature drop across the radiator - and decides what the fan should do next.
- **Why flow-vs-return is the right signal:** the size of that temperature drop directly reflects how much heat the radiator is currently giving up to its surroundings. A large flow/return delta means the water is losing a lot of heat as it passes through - there's still plenty of heat available to extract, so it's worth actively fanning it into the room. As the radiator naturally depletes the available heat from that water pass (or the boiler eases off, see the [thermostat repo's modulation notes](https://github.com/BarBaar44/thermostat#why-each-trv-target-gets-a-1c-overshoot-buffer)), the flow/return gap narrows on its own - signalling there's much less benefit left to boosting further, at which point the fan winds back down.
- **Hysteresis hard-off band:** the fan only turns *on* once `deltaT` exceeds `1.5°C`, and only turns back *off* once `deltaT` drops below `0.8°C`. This prevents the fan from rapidly flickering on/off right at the boundary - once it's running, it keeps running until the flow/return gap has genuinely narrowed further than the point that triggered it.
- **Exponential fan curve:** once active, fan speed isn't linear - it scales from 25% at `deltaT ≈ 2°C` up to 80% at `deltaT ≈ 20°C`, using an exponential curve (`exponent: 1.1`). This means a small flow/return gap gets a gentle, quiet fan speed, while a large gap (radiator freshly full of hot water, lots of heat still to give up) gets a noticeably stronger boost.
- **AUTO / MANUAL modes**, exposed to Home Assistant:
  - `select.*_fan_mode` - `AUTO` (the ΔT-driven logic above) or `MANUAL`.
  - `number.*_manual_fanspeed` - in `MANUAL` mode, sets the fan speed directly (0-100), bypassing the automatic curve entirely. Values below 1 are treated as fully off.
- **Fan RPM sensor** (`pulse_counter` on `D5`) reports actual measured RPM back to Home Assistant, so you can verify the fan is physically responding as expected, independent of the commanded PWM level.

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
