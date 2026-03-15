# ESP32-C6 Home Assistant Temperature Display

A WiFi-connected temperature display using an ESP32-C6 microcontroller that fetches temperature data from Home Assistant via MQTT and displays it on a 3-digit 7-segment NeoPixel display with dynamic color coding.

## Features

- **WiFi Provisioning**: Easy setup using SoftAP provisioning (no hardcoded credentials)
- **MQTT Integration**: Subscribes to Home Assistant temperature sensors
- **Dynamic Color Display**: Temperature-based LED color changes
  - Green: Cold temperatures (-20°C and below)
  - Cyan: Cool temperatures (5°C)
  - Yellow: Warm temperatures (20°C)
  - Orange: Hot temperatures (35°C)
  - Red: Very hot temperatures (50°C and above)
- **3-Digit 7-Segment Display**: Clear temperature reading with smooth color transitions

## Hardware Requirements

### Components
- **MCU**: ESP32-C6 development board
- **Display**: 3x WS2812B (NeoPixel) 7-segment displays
  - Total: 84 RGB LEDs (3 digits × 7 segments × 4 LEDs per segment)
- **Power**: USB power or separate 5V supply for LEDs (recommended)
- **Connector**: Micro USB for flashing and power

### Wiring
```
ESP32-C6             NeoPixel Display
=====================================
GPIO 8     ------>   DIN (Data In)
GND        ------>   GND
5V         ------>   VCC

LED Power Supply:
5V Power Supply ----> VCC
GND             ----> GND + Ground of ESP32
```

## Software Requirements

- **ESP-IDF**: v4.4 or newer (recommend v5.0+)
- **Python**: 3.7+ (for ESP-IDF tools)
- **Home Assistant**: With MQTT broker configured

## Building and Flashing

### Prerequisites
1. Install ESP-IDF following [official documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/)
2. Ensure `IDF_PATH` environment variable is set
3. Have Home Assistant running with MQTT broker enabled

### Build Steps

1. Navigate to project directory:
```bash
cd examples/esp32c6_temp_display
```

2. Configure the project:
```bash
idf.py menuconfig
```

   Key configurations to set:
   - **MQTT Broker URI**: Your Home Assistant MQTT broker address
     - Format: `mqtt://homeassistant.local:1883` or `mqtt://192.168.1.100:1883`
   - **MQTT Username**: Your MQTT credentials
   - **MQTT Password**: Your MQTT password
   - **Temperature Topic**: The MQTT topic for the temperature sensor
   - **NeoPixel GPIO**: GPIO pin for the display (default: GPIO 8)

3. Build the project:
```bash
idf.py build
```

4. Flash the device:
```bash
idf.py flash
```

5. Monitor the serial output:
```bash
idf.py monitor
```

### Or do all in one command:
```bash
idf.py flash monitor
```

## Configuration

### WiFi Provisioning

On first boot, the device will start a WiFi provisioning hotspot:
- **SSID**: `TempDisplay_XXXXXX` (where XXXXXX is part of the MAC address)
- **Password**: Check the serial output for the provisioning PIN or use the default
- **Access URL**: http://192.168.4.1 (open in browser after connecting)

### MQTT Configuration

#### Finding Your Temperature Sensor Topic

In Home Assistant:

1. Go to **Settings** → **Devices & Services** → **MQTT**
2. Find your temperature entity
3. Click on it to see the **Availability Topic** and **State Topic**
4. Use the **State Topic** in the configuration

Example for a Zigbee temperature sensor:
- Topic: `zigbee2mqtt/living_room_sensor/temperature`

Example for a Home Assistant template sensor:
- Topic: `homeassistant/sensor/living_room_temperature/state`

### Editing Configuration

To reconfigure MQTT settings or WiFi after initial setup:

```bash
idf.py menuconfig
# Make changes in the MQTT Configuration menu
idf.py build flash
```

## Understanding the Code

### Project Structure
```
esp32c6_temp_display/
├── CMakeLists.txt          # Top-level build configuration
├── idf_component.yml       # Component dependencies (NeoPixel library)
├── sdkconfig.defaults      # Default build configuration
├── partitions.csv          # Flash memory layout
├── README.md               # This file
└── main/
    ├── CMakeLists.txt      # Main component build config
    ├── Kconfig.projbuild   # Configuration menu options
    ├── app_main.c          # Main application entry point
    ├── neopixel_display.c  # 7-segment display driver
    ├── neopixel_display.h  # Display driver API
    ├── mqtt_temp.c         # MQTT temperature subscriber
    └── mqtt_temp.h         # MQTT subscriber API
```

### Key Components

#### 1. **app_main.c**
- Initializes WiFi and provisioning
- Sets up MQTT client
- Manages the main display update loop
- Event handling for WiFi and MQTT events

#### 2. **neopixel_display.c/h**
- Controls the NeoPixel 7-segment displays
- Renders digits (0-9) on the display
- Calculates color based on temperature
- Manages brightness and LED animations

#### 3. **mqtt_temp.c/h**
- MQTT client initialization and management
- Subscribes to temperature topics
- Parses JSON and plain text payloads
- Provides thread-safe temperature value access

## Troubleshooting

### Device won't connect to WiFi
1. Check the MQTT broker is accessible from your network
2. Use IP address instead of hostname if DNS resolution fails
3. Check firewall allows port 1883 (or your configured MQTT port)

### Temperature not updating
1. Verify MQTT topic is correct in configuration
2. Check Home Assistant MQTT integration is working
3. Monitor serial output: `idf.py monitor`
4. Verify the MQTT message format (plain number or JSON)

### Display not lighting up
1. Check power supply to LEDs (should be 5V)
2. Verify GPIO pin configuration (default: GPIO 8)
3. Check LED strip is properly wired
4. Try reduced brightness in configuration

### Compilation errors
1. Ensure ESP-IDF is properly installed
2. Run `idf.py clean` then rebuild
3. Check all dependencies: `idf.py dependency-tree`

## Color Temperature Mapping

| Temperature | Color | R | G | B |
|-------------|-------|---|---|---|
| ≤ -20°C   | Green | 0 | 255 | 0 |
| -20° to 5°C | Green→Cyan | 0 | 255 | 0→255 |
| 5° to 20°C  | Cyan→Yellow | 0→255 | 255 | 255→0 |
| 20° to 35°C | Yellow→Orange | 255 | 255→165 | 0 |
| 35° to 50°C | Orange→Red | 255 | 165→0 | 0 |
| ≥ 50°C    | Red | 255 | 0 | 0 |

## Serial Monitor Output

Example of normal operation:
```
I (123) app_main: ========================================
I (124) app_main: ESP32-C6 Temperature Display Starting
I (125) app_main: ========================================
I (126) app_main: Initializing NeoPixel display on GPIO 8
I (200) neopixel_display: NeoPixel display initialized successfully
I (201) app_main: Application initialized successfully
I (202) wifi_event: WIFI_EVENT_STA_START
I (300) wifi_prov_mgr: Starting WiFi provisioning...
...
I (5000) tcp_transport: SSL/TLS handshake complete
I (5100) mqtt_client: Connected to MQTT broker
I (5200) mqtt_temp: Temperature updated: 22.5°C
```

## Power Considerations

- **ESP32-C6**: ~80-120mA during normal operation
- **WS2812B LEDs (full white)**: ~60mA per LED × 84 = ~5A maximum
- **Recommended PSU**: 5V/2A minimum for safe operation

## Future Enhancements

- [ ] Multiple temperature sensor support
- [ ] Low-power sleep mode with periodic updates
- [ ] OTA firmware updates
- [ ] Web interface for configuration
- [ ] Humidity display (if using multi-sensor)
- [ ] Custom color themes
- [ ] Temperature high/low alerts

## License

Apache License 2.0 - See LICENSE file

## Resources

- [Home Assistant MQTT Integration](https://www.home-assistant.io/integrations/mqtt/)
- [ESP32-C6 Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/)
- [NeoPixel WS2812B Datasheet](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf)
- [ESP-IDF MQTT Client](https://github.com/espressif/esp-idf/tree/master/components/mqtt)

## Support

If you encounter issues:
1. Check the troubleshooting section above
2. Review serial monitor output with `idf.py monitor`
3. Check Home Assistant MQTT broker logs
4. Verify network connectivity
