# DBE - Radiator Booster Fan Controller

## What this is

DBE is a small ESPHome-based controller that sits on a radiator and drives a **fan to force convective airflow across the radiator fins**, boosting how much heat that radiator actually transfers into the room air - rather than relying purely on natural convection.

Radiators heat a room mostly by warming the air that passively rises past their fins. That passive convection is relatively slow and inefficient at getting heat away from the radiator and into the room's air volume. By adding a fan that blows air directly across/through the radiator whenever there's meaningful heat still available to extract, significantly more heat gets moved from the radiator into the room per unit time - the radiator itself doesn't need to run hotter, the room just absorbs the heat it's already producing more effectively.

This is one part of the broader multi-room heating system documented in the [**thermostat**](https://github.com/BarBaar44/thermostat) repo - DBE units are deployed per-room (see the `DBE_*` sensor naming across rooms like Front, Hallway, Master Left/Right, Office, Room Gijs, Room Tim in that repo) to squeeze more usable heat out of the same water temperature the boiler is already producing.

## How it works (`DBEmain.yml`)

- **Two Dallas (DS18B20) temperature sensors** (`temp_1`, `temp_2`) on a shared OneWire bus, each polled every 30s, measuring the radiator's **flow pipe** (hot water entering the radiator) and **return pipe** (cooled water leaving it) respectively.
- Every **5 seconds**, an `interval:` trigger runs the `control_fans` script, which computes `deltaT = |temp_1 - temp_2|` - the flow-vs-return temperature drop across the radiator - and decides what the fan should do next.
- **Why flow-vs-return is the right signal:** the size of that temperature drop directly reflects how much heat the radiator is currently giving up to its surroundings. A large flow/return delta means the water is losing a lot of heat as it passes through - there's still plenty of heat available to extract, so it's worth actively fanning it into the room. As the radiator naturally depletes the available heat from that water pass (or the boiler eases off, see the [thermostat repo's modulation notes](https://github.com/BarBaar44/thermostat#why-each-trv-target-gets-a-1c-overshoot-buffer)), the flow/return gap narrows on its own - signalling there's much less benefit left to boosting further, at which point the fan winds back down.
- **Hysteresis hard-off band:** the fan only turns *on* once `deltaT` exceeds `1.5°C`, and only turns back *off* once `deltaT` drops below `0.8°C`. This prevents the fan from rapidly flickering on/off right at the boundary - once it's running, it keeps running until the flow/return gap has genuinely narrowed further than the point that triggered it.
- **Exponential fan curve:** once active, fan speed isn't linear - it scales from 25% at `deltaT ≈ 2°C` up to 80% at `deltaT ≈ 20°C`, using an exponential curve (`exponent: 1.1`). This means a small flow/return gap gets a gentle, quiet fan speed, while a large gap (radiator freshly full of hot water, lots of heat still to give up) gets a noticeably stronger boost.
- **Hard power cutoff via relay:** rather than relying solely on PWM duty cycle to represent "fan off" (which can still let some current/noise through on many cheap fans), a physical relay (`powerswitch`) is wired to the fan's `on_turn_on`/`on_turn_off` events, so the fan is genuinely disconnected from power when it should be off - not just PWM'd to near-zero.
- **AUTO / MANUAL modes**, exposed to Home Assistant:
  - `select.*_fan_mode` - `AUTO` (the ΔT-driven logic above) or `MANUAL`.
  - `number.*_manual_fanspeed` - in `MANUAL` mode, sets the fan speed directly (0-100), bypassing the automatic curve entirely. Values below 1 are treated as fully off.
- **Fan RPM sensor** (`pulse_counter` on `D5`) reports actual measured RPM back to Home Assistant, so you can verify the fan is physically responding as expected, independent of the commanded PWM level.

## Hardware

The physical board (custom MOSFET driver PCB) that this firmware runs on is designed in EasyEDA and lives in a separate repo:

➡️ **[BarBaar44/EasyEDA](https://github.com/BarBaar44/EasyEDA)** - schematic/PCB source project, Gerber manufacturing files, and a rendered PCB preview image.

That repo's README also has a pointer to the enclosure design (FreeCAD) for this same board.

## Related

- [BarBaar44/thermostat](https://github.com/BarBaar44/thermostat) - the overall multi-room heating system this fan controller supports, including the virtual-thermostat/TRV/boiler logic that decides *when* and *how hot* each room's radiator should be running in the first place.
