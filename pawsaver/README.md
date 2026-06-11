# PawSaver
Made by the WWU Engineers Without Borders Club for the Blue Mountain Humane Society
## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Tech Stack](#tech-stack)
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Acknowledgements](#acknowledgements)

---

## Overview
PawSaver is an ESP32-C6 based sensor that monitors ground (pavement) temperature to help keep dogs safe during walks on hot days. It periodically measures the surface temperature beneath it with an infrared thermometer, reports the reading (along with ambient temperature and battery status) to a Home Assistant instance over MQTT, and then drops into deep sleep to conserve battery. Battery-aware power modes automatically slow down reporting as the battery drains, and a button press lets you re-provision the device to a new Wi-Fi network at any time.

---

## Features
- **Ground & ambient temperature sensing** via an MLX90614 IR thermometer (GY-906 breakout) over I2C
- **Battery monitoring** through an ADC + voltage divider, with calibrated voltage readings
- **Adaptive power modes** (Normal, Low Power, Dead, Debug) that adjust reporting frequency and deep-sleep duration based on battery voltage or USB connection
- **Deep sleep with timer + GPIO wakeup** to maximize battery life between readings
- **Wi-Fi provisioning** over BLE or SoftAP (with QR code) using the ESP-IDF Wi-Fi Provisioning Manager
- **Button-triggered re-provisioning** for switching the device to a new Wi-Fi network
- **MQTT publishing with Home Assistant auto-discovery**, including mDNS resolution of `homeassistant.local`
- **Status LEDs** for power and provisioning state

---

## Tech Stack
- **Hardware:**
  - Seeed Studio XIAO ESP32-C6
  - MLX90614 IR thermometer (GY-906 module) over I2C
  - Battery with voltage divider into an ADC input
  - Push button (re-provisioning / wakeup) and status LEDs
- **Software:**
  - [ESP-IDF](https://github.com/espressif/esp-idf) (C, FreeRTOS)
  - ESP-IDF Wi-Fi Provisioning Manager (BLE / SoftAP, QR code)
  - `esp-mqtt`, `mdns`, `cJSON`, `qrcode` components
- **Integrations:**
  - MQTT broker (e.g., [Home Assistant](https://www.home-assistant.io/) Mosquitto add-on) with MQTT auto-discovery

---

## Getting Started

### Prerequisites
- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/index.html) (matching the version pinned in this project) installed (run `export.bat` on Windows, or `. ./export.sh` on Linux/macOS, from your ESP-IDF install directory)
- A Seeed Studio XIAO ESP32-C6 (or compatible ESP32-C6 board)
- An MLX90614 (GY-906) IR thermometer module wired to I2C
- An MQTT broker reachable on your network (e.g., Home Assistant with the Mosquitto broker add-on)

### Installation
```bash
# Clone the repository
git clone https://github.com/nathantgray/esp-idf-wwu.git

# Set up the ESP-IDF environment (run from your ESP-IDF install directory)
export.bat

# Move into the project directory
cd esp-idf-wwu/pawsaver

# Set the target chip
idf.py set-target esp32c6

# Then set the Flash size to 4MB, not 2MB, in menuconfig (Serial Flasher Config -> Flash Size)
idf.py menuconfig

# Then build the project
idf.py build

# Finally, flash the build to the ESP32-C6 and monitor the serial output
idf.py flash monitor
```
---
### Home Assistant
TBD


---

### Configuration
Run `idf.py menuconfig` and open the **PawSaver Configuration** menu to set:
- `PAWSAVER_MQTT_BROKER_URI` – MQTT broker URI (default `mqtt://homeassistant.local:1883`)
- `PAWSAVER_MQTT_USERNAME` / `PAWSAVER_MQTT_PASSWORD` – MQTT credentials
- `PAWSAVER_MQTT_TOPIC` – MQTT topic for publishing sensor data
- `PAWSAVER_I2C_SDA_PIN` / `PAWSAVER_I2C_SCL_PIN` – I2C pins for the MLX90614
- `PAWSAVER_BATTERY_ADC_CHANNEL` / `PAWSAVER_VOLTAGE_DIVIDER_RATIO` – battery voltage measurement settings

Wi-Fi credentials are not stored in the firmware. On first boot (or after a reset/re-provision), the device starts the Wi-Fi Provisioning Manager — scan the printed QR code (or use the URL printed over serial) with the ESP provisioning app to send it your network's SSID and password.

---

## Usage
1. On first boot, the device enters provisioning mode (provisioning LED on) and prints a QR code/URL for Wi-Fi setup.
2. Once provisioned and connected to Wi-Fi, the device:
   - Reads ground (object) and ambient temperature from the MLX90614
   - Reads battery voltage and determines the current power mode (Normal / Low Power / Dead / Debug)
   - Connects to the configured MQTT broker, publishes Home Assistant discovery info, then publishes the sensor reading
3. The device then enters deep sleep for a duration based on the current power mode, waking on a timer or on a button press.
4. To re-provision to a new Wi-Fi network, press the wakeup button to wake the device from deep sleep — this forces it back into provisioning mode.

---

## Acknowledgements
- Built on Espressif's [ESP-IDF](https://github.com/espressif/esp-idf) and the Wi-Fi Provisioning example
- Uses the `esp-mqtt`, `mdns`, and `qrcode` ESP-IDF components
- GitHub Copilot and Claude were used in the development of this code
