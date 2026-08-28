# ESPHome Config for Home Assistant

This repository contains ESPHome configuration files for various devices integrated with Home Assistant. Configs are organised per device type in subdirectories.

---

## 📁 Repository Structure

```
ESPHome/
├── base/                          # Primary per-device configs (reference shared files via packages:)
├── DBE/
│   └── DBEmain.yml               # Temp-differential fan controller (dual Dallas sensors + hard-off relay)
├── LVGL/
│   ├── displayconfig.yml         # Shared display/touch driver config (ESP32-S3, 800x480 mipi_rgb, GT911)
│   └── Masterbedroom/
│       ├── lvglconfig.yml        # Master Bedroom control panel UI (widgets, pages, actions)
│       └── lvglsensors.yml       # Sensors/scripts backing the panel (brightness, screen state, etc.)
├── USB-Switch/
│   └── usbswitch.yaml            # USB port switch: relay + status LEDs + physical button toggle
├── bluetooth-proxy/
│   └── proxy.yml                 # Bluetooth proxy config
├── generic/
│   ├── generic_sensors.yml       # Generic reusable sensor definitions
│   ├── mqtt.yml                  # MQTT configuration
│   ├── restartbutton.yml         # Restart button component
│   └── wifi.yml                  # WiFi configuration (incl. fallback AP)
├── shelly/
│   ├── one.yml                   # Shelly 1 switch config
│   └── plug_s.yml                # Shelly Plug S config with power monitoring + safety cutoffs
└── sonoff/
    ├── basic.yml                 # Sonoff Basic switch config
    └── ifan03.yml                # Sonoff iFan03 fan + light controller
```

---

## 📂 Folder Details

## 📂 `base/` (WIP)

Contains the primary ESPHome config file for each device. These reference the shared files in the other folders (`generic/`, `shelly/`, `sonoff/`, etc.) via `packages:` statements pointing back at this repo, rather than duplicating that config locally.

Example:
```yaml
packages:
  Generic_config:
    url: https://github.com/BarBaar44/ESPHome
    files: [generic/generic_sensors.yml, generic/wifi.yml, shelly/plug_s.yml]
    refresh: 1min
```

---

### 🔵 `DBE/`
| File | Description |
|------|-------------|
| `DBEmain.yml` | Temperature-differential fan controller for the DBE (custom) device. Two Dallas temp sensors feed a hysteresis + exponential PWM curve that drives the fan speed, backed by a physical relay as a hard power cutoff (tied to the fan's on/off state) rather than relying on PWM duty alone. Supports AUTO and MANUAL (HA-driven) modes. |

---

### 🖥️ `LVGL/`
| File | Description |
|------|-------------|
| `displayconfig.yml` | Shared display + touch driver config for the **Master Bedroom Control Panel**: **ESP32-S3**, 800×480 RGB panel via `mipi_rgb`, GT911 touch controller (I2C). |
| `Masterbedroom/lvglconfig.yml` | Full LVGL UI: lighting control (main + left/right bed lights), fan speed (OFF/LOW/MID/HIGH), cover/blind control with open/close scheduling, weather widget (OpenWeatherMap), and climate control (thermostat setpoint, Eco/Warm/Off modes). Syncs all states bidirectionally with Home Assistant. |
| `Masterbedroom/lvglsensors.yml` | Supporting sensors and scripts for the panel: screen wake/timeout logic, backlight brightness control, and related state tracking. |

---

### 🔌 `USB-Switch/`
| File | Description |
|------|-------------|
| `usbswitch.yaml` | Controls a [Sinilink XY-WFUSB USB Switch Relay](https://devices.esphome.io/devices/sinilink-xy-wfusb-usb-switch-relay/). A template switch (exposed to HA as `${friendly_name}`) drives both the relay and a green status LED (LED on = relay on), a separate blue status LED is tied to `status_led:`, and a physical front-panel button toggles the switch via a debounced `binary_sensor` on `GPIO04`. |

---

### 📡 `bluetooth-proxy/`
| File | Description |
|------|-------------|
| `proxy.yml` | Configures a [GL-iNet GL-S10](https://devices.esphome.io/devices/gl-inet-gl-s10/) as a **Bluetooth proxy** for Home Assistant, extending BLE range throughout the home. |

---

### ⚙️ `generic/`
Reusable package snippets that can be included in other configs.

| File | Description |
|------|-------------|
| `generic_sensors.yml` | Common sensor definitions (e.g. WiFi signal strength, uptime) reusable across devices. |
| `mqtt.yml` | MQTT broker configuration for devices that use MQTT instead of the native API. |
| `restartbutton.yml` | Adds a restart button entity to any device for easy remote rebooting from Home Assistant. |
| `wifi.yml` | WiFi settings including SSID, password, fast connect, power save mode, and a fallback AP. |

---

### 🔌 `shelly/`
Configurations for Shelly devices flashed with ESPHome.

| File | Description |
|------|-------------|
| `one.yml` | ESPHome config for the **Shelly 1** — a single-channel relay switch with multi-click (single/double/long press) detached button handling and a wifi/API-down fallback. |
| `plug_s.yml` | ESPHome config for the **Shelly Plug S** — a smart plug with power/voltage/current monitoring, NTC temperature sensing, and automatic relay cutoff (with a one-shot HA notification) if temperature, current, or power exceed configured limits. |

---

### 🔌 `sonoff/`
Configurations for Sonoff devices flashed with ESPHome.

| File | Description |
|------|-------------|
| `basic.yml` | ESPHome config for the **Sonoff Basic** — a simple single-channel relay switch. |
| `ifan03.yml` | ESPHome config for the **Sonoff iFan03** — controls both a ceiling fan (3-speed) and a light, integrated with Home Assistant and the original RF remote. Publishes an explicit off/low/mid/high fan state for consumers like the LVGL panel. |

---

## 🏠 Home Assistant Integration

All devices use either:
- **Native ESPHome API** with encryption for direct HA integration
- **MQTT** for devices configured with `generic/mqtt.yml`

WiFi network: `BarBaar_IoT`

---

## 📋 Notes
- Secrets (passwords, API keys) are stored in `secrets.yaml` and not committed to this repository.
- The ESPHome Fleet add-on is used for scheduled OTA updates on supported devices.
