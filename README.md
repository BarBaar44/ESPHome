# ESPHome Config for Home Assistant

This repository contains ESPHome configuration files for various devices integrated with Home Assistant. Configs are organised per device type in subdirectories.

---

## 📁 Repository Structure

```
ESPHome/
├── DBE/
│   └── DBEmain.yml             # Main config for the DBE device
├── LVGL/
│   └── display.yaml            # LVGL touchscreen display (ESP32-S3, 800x480)
├── bluetooth-proxy/
│   └── proxy.yml               # Bluetooth proxy config
├── generic/
│   ├── generic_sensors.yml     # Generic reusable sensor definitions
│   ├── mqtt.yml                # MQTT configuration
│   ├── restartbutton.yml       # Restart button component
│   └── wifi.yml                # WiFi configuration (incl. fallback AP)
├── shelly/
│   ├── one.yml                 # Shelly 1 switch config
│   └── plug_s.yml              # Shelly Plug S config with power monitoring
└── sonoff/
    ├── basic.yml               # Sonoff Basic switch config
    └── ifan03.yml              # Sonoff iFan03 fan + light controller
```

---

## 📂 Folder Details

### 🔵 `DBE/`
| File | Description |
|------|-------------|
| `DBEmain.yml` | Main ESPHome configuration for the DBE (custom) device, integrated with Home Assistant. |

---

### 🖥️ `LVGL/`
| File | Description |
|------|-------------|
| `display.yaml` | Full LVGL-based touchscreen UI for a **Master Bedroom Control Panel** running on an **ESP32-S3** with an 800×480 RGB display. Features: lighting control (main + left/right bed lights), fan speed (OFF/LOW/MID/HIGH), cover/blind control with open/close scheduling, weather widget (OpenWeatherMap), and climate control (thermostat setpoint, Eco/Warm/Off modes). Syncs all states bidirectionally with Home Assistant. |

---

### 📡 `bluetooth-proxy/`
| File | Description |
|------|-------------|
| `proxy.yml` | Configures an ESP device as a **Bluetooth proxy** for Home Assistant, extending BLE range throughout the home. |

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
| `one.yml` | ESPHome config for the **Shelly 1** — a single-channel relay switch. |
| `plug_s.yml` | ESPHome config for the **Shelly Plug S** — a smart plug with power/voltage/current monitoring. |

---

### 🔌 `sonoff/`
Configurations for Sonoff devices flashed with ESPHome.

| File | Description |
|------|-------------|
| `basic.yml` | ESPHome config for the **Sonoff Basic** — a simple single-channel relay switch. |
| `ifan03.yml` | ESPHome config for the **Sonoff iFan03** — controls both a ceiling fan (multi-speed) and a light, integrated with Home Assistant. |

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
