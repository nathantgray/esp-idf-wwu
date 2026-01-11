# PawSaver

A ground temperature monitoring device for dog walking safety. Measures pavement temperature using an IR thermal sensor and reports to Home Assistant via MQTT.

## Hardware

- **MCU:** Seeed Studio XIAO ESP32-C6
- **Sensor:** MLX90614 IR Thermal Sensor (I2C)
- **Power:** LiPo battery with voltage monitoring

## Features

- BLE Wi-Fi provisioning (no hardcoded credentials)
- MQTT publishing to Home Assistant
- Adaptive deep sleep based on battery level
- QR code for easy mobile provisioning

## Power Modes

| Mode      | Battery Voltage | Sleep Duration |
|-----------|-----------------|----------------|
| NORMAL    | > 3.84V         | 60 seconds     |
| LOW_POWER | 3.73V - 3.84V   | 300 seconds    |
| DEAD      | < 3.73V         | 3600 seconds   |
| DEBUG     | > 4.4V (plugged)| 5 seconds      |

## MQTT Payload

```json
{
  "timestamp": 12345678,
  "ambient": 25.5,
  "object": 45.2,
  "battery": 3.9,
  "mode": 0
}
```

## Build Instructions

```bash
# Set up ESP-IDF environment
source /path/to/esp-idf/export.sh

# Set target to ESP32-C6
idf.py set-target esp32c6

# Configure project
idf.py menuconfig

# Build and flash
idf.py build flash monitor
```

## Configuration

Use `idf.py menuconfig` to configure:

- **Example Configuration → Provisioning Transport:** BLE
- **PawSaver Configuration:**
  - MQTT Broker URI
  - MQTT Username/Password
  - MQTT Topic
  - I2C GPIO pins
  - Battery ADC channel

## Provisioning

1. Power on the device
2. Use ESP BLE Provisioning app (Android/iOS)
3. Scan QR code or search for "PROV_PAWSAVER_XXXX"
4. Enter Wi-Fi credentials
5. Device connects and starts reporting

## License

Apache 2.0
