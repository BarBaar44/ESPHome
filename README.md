# ESPHome Config for Home Assistant

ESPHome configs for the devices in this house, integrated with Home Assistant. Every device has its own file in `base/`; shared behaviour lives in package files in the other folders and is pulled in with `packages:`.

---

## 📁 Repository Structure

```
ESPHome/
├── base/                      # One config per device (the files that get flashed)
├── generic/
│   ├── wifi.yml              # Wi-Fi + fallback hotspot + captive portal
│   ├── api_ota.yml           # API and OTA encryption (every device)
│   ├── generic_sensors.yml   # HA time source, uptime timestamp, Wi-Fi signal
│   ├── restartbutton.yml     # Restart button entity
│   └── mqtt.yml              # MQTT broker config (legacy)
├── DBE/
│   ├── DBEmain.yml           # Radiator booster fan controller
│   └── README.md             # How the DBE works, tuning, hardware
├── LVGL/
│   ├── displayconfig5inch.yml  # Display + touch driver, 5 inch panel (Master Bedroom)
│   ├── displayconfig7inch.yml  # Display + touch driver, 7 inch panel (Room Tim)
│   ├── Masterbedroom/          # Master Bedroom wall panel UI + sensors + CI harness
│   └── Tim Bedroom/            # Room Tim wall panel UI + sensors + CI harness
├── bluetooth-proxy/
│   └── proxy.yml             # GL-iNet GL-S10 Ethernet Bluetooth proxy
├── shelly/
│   ├── one.yml               # Shelly 1 relay, detached multi-click button
│   └── plug_s.yml            # Shelly Plug S with power monitoring + safety cutoffs
├── sonoff/
│   ├── basic.yml             # Sonoff Basic relay
│   └── ifan03.yml            # Sonoff iFan03 fan + light
├── USB-Switch/
│   └── usbswitch.yaml        # Sinilink USB switch relay
├── .ci/                      # CI harnesses and scripts (see .ci/README.md)
└── requirements.txt          # ESPHome version CI validates with
```

---

## 📂 `base/`: device configs

Each file defines the device's substitutions (name, area, secrets, calibration) and pulls the shared packages straight from this repo:

```yaml
packages:
  Generic_config:
    url: https://github.com/BarBaar44/ESPHome
    files: [generic/wifi.yml, generic/api_ota.yml, generic/generic_sensors.yml, shelly/plug_s.yml]
    refresh: 0d
```

`refresh: 0d` fetches the latest `main` on every compile, so a merged change reaches a device on its next build. Every device includes `generic/api_ota.yml`; Wi-Fi devices also include `generic/wifi.yml`.

The files are synced with the ESPHome dashboard on Home Assistant. The `# esphome-fleet:` header in each file is read by the Fleet for ESPHome add-on (scheduled OTA updates); leave it in place.

| Group | Devices |
|---|---|
| DBE fan controllers | `dbe-hallway`, `dbe-master-left`, `dbe-master-right`, `dbe-office`, `dbe-room-gijs`, `dbe-room-tim`, `dbefront` |
| Heating | `thermostat` (OpenTherm boiler control with a PID loop, stops heating 5 minutes after losing HA) |
| Wall panels | `master-bedroom-display` (5 inch), `tim-bedroom-display` (7 inch) |
| Shelly Plug S | `shelly-amp`, `shelly-boiler`, `shelly-oura`, `shelly-playstation`, `shelly-plug-tt`, `shelly-sub`, `shelly-tv` |
| Shelly 1 | `shelly1-gijs`, `shelly1-mb`, `shelly1-sr`, `shelly1-tim` |
| Sonoff | `sonoff-fan-bath`, `sonoff-gijs`, `sonoff-light-bath`, `sonoff-tim`, `sonoff-ifan-masterbedroom` |
| Bluetooth proxies | `bluetooth-proxy-attic`, `bluetooth-proxy-garden`, `bluetooth-proxy-hallway` |
| USB switches | `usb-switch-1`, `usb-switch-2`, `usb-switch-3` |
| Meters | `p1-meter-kit` (energy), `watermeterkit` (water), `cometer` (CO₂ / air quality) |
| Other | `berkey-fill-controller` (water filter fill), `led-closet` (LED strip) |

---

### ⚙️ `generic/`
Reusable packages included by the device configs.

| File | Description |
|---|---|
| `wifi.yml` | Wi-Fi (`BarBaar_IoT`, WPA2 minimum, fast connect, light power save), fallback hotspot and captive portal. |
| `api_ota.yml` | Encrypted Home Assistant API (`${device_key}`) and OTA uploads encrypted with that same key. Plaintext uploads are refused. The single place for API/OTA settings. |
| `generic_sensors.yml` | Home Assistant time source (`time_homeassistant`), boot time as an uptime timestamp, Wi-Fi signal strength. |
| `restartbutton.yml` | Restart button entity for remote reboots from HA. |
| `mqtt.yml` | MQTT broker settings. Legacy, no current device uses it. |

---

### 🔵 `DBE/`
| File | Description |
|---|---|
| `DBEmain.yml` | Radiator booster fan controller (custom board). Two DS18B20 sensors on the supply and return pipes; the fan runs while the supply pipe is warm, with speed following delta T on a selectable curve (Rising or Falling). A relay gives a hard power cutoff. Modes AUTO, MANUAL and OFF. Details in [DBE/README.md](DBE/README.md). |

---

### 🖥️ `LVGL/`
Wall panels on an ESP32-S3 with an 800×480 RGB display (`mipi_rgb`) and GT911 touch.

| File | Description |
|---|---|
| `displayconfig5inch.yml` / `displayconfig7inch.yml` | Display, touch and backlight driver config per panel size. |
| `Masterbedroom/lvglconfig.yml` | Master Bedroom UI: main and bed lights, fan speed, blinds with open/close scheduling, weather, climate (setpoint, Eco/Warm/Off) with a DBE fan indicator. Synced both ways with HA. |
| `Masterbedroom/lvglsensors.yml` | Screen wake/timeout, backlight brightness and related state. |
| `Tim Bedroom/lvglconfig.yml` | Room Tim UI: weather, agenda, light, temperature, fan, blinds. |
| `Tim Bedroom/lvglsensors.yml` | Scripts, globals and HA links for the Room Tim panel, including a notification overlay HA can show and clear (`esphome.tbr_display_notify`, `esphome.tbr_display_clear_notification`). |

---

### 📡 `bluetooth-proxy/`
| File | Description |
|---|---|
| `proxy.yml` | [GL-iNet GL-S10](https://devices.esphome.io/devices/gl-inet-gl-s10/) as an active **Bluetooth proxy** over Ethernet. Status LEDs for power and HA connection; the hardware button restarts the device. |

---

### 🔌 `shelly/`
| File | Description |
|---|---|
| `one.yml` | **Shelly 1**: single relay with a detached button. Single, double and long press go to HA as events; without Wi-Fi or API the button toggles the relay directly. |
| `plug_s.yml` | **Shelly Plug S**: power, voltage, current and daily energy, NTC temperature, and automatic relay cutoff (with a one-shot HA notification) when temperature, current or power exceed per-device limits. Measures every 2 s. |

---

### 🔌 `sonoff/`
| File | Description |
|---|---|
| `basic.yml` | **Sonoff Basic**: single relay with a debounced button and status LED. |
| `ifan03.yml` | **Sonoff iFan03**: 3-speed ceiling fan and light, with the original RF remote. Publishes an explicit off/low/mid/high fan state for consumers like the LVGL panel. |

---

### 🔌 `USB-Switch/`
| File | Description |
|---|---|
| `usbswitch.yaml` | [Sinilink XY-WFUSB USB Switch Relay](https://devices.esphome.io/devices/sinilink-xy-wfusb-usb-switch-relay/). The switch drives the relay and the green LED together and restores its state after a reboot (`default_state`); the blue LED is the status LED; the front button toggles the port. |

---

## ✅ CI

GitHub Actions validates every package harness, every device config in `base/`, and the DBE control logic on each push and PR. ESPHome is pinned in `requirements.txt`; Dependabot bumps it and the bump merges itself once all checks pass. Details in [.ci/README.md](.ci/README.md).

---

## 📋 Notes
- Secrets (passwords, API keys) are in `secrets.yaml` on the ESPHome dashboard and are not committed.
- Devices use the native ESPHome API with encryption.
