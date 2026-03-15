# Quick Start Guide - ESP32-C6 Temperature Display

Get your temperature display up and running in 10 minutes!

## What You'll Need

1. **Hardware**:
   - ESP32-C6 development board
   - 3x WS2812B 7-segment displays (or similar addressable RGB LEDs)
   - Micro USB cable for programming
   - 5V power supply (USB or external)

2. **Software**:
   - ESP-IDF v4.4+ installed
   - Home Assistant with MQTT broker
   - Your temperature sensor already working in Home Assistant

## Wiring

Connect your NeoPixel display to ESP32-C6:
```
Display DIN  → GPIO 8
Display GND  → GND
Display VCC  → 5V
```

## Configuration (5 Minutes)

1. **Open configuration menu**:
```bash
cd examples/esp32c6_temp_display
idf.py menuconfig
```

2. **Set these values** (use arrow keys to navigate, hit Enter to edit):

   Navigate to `Temperature Display Configuration` :
   
   - **NeoPixel Display Configuration**
     - Change `NeoPixel Data GPIO` if not using GPIO 8
   
   - **MQTT Configuration**
     - Set `MQTT Broker URI` to your Home Assistant broker
       - Example: `mqtt://192.168.1.100:1883`
     - Set `MQTT Username` to your Home Assistant username
     - Set `MQTT Password` to your Home Assistant password
     - Set `Temperature Topic` to your sensor's MQTT topic
       - See HA_SETUP.md for how to find this
       - Example: `zigbee2mqtt/living_room/temperature`

3. **Save and exit**: Press `S` to save, then `Q` to exit

## Build and Flash (3 Minutes)

```bash
# Build the project
idf.py build

# Flash to your ESP32-C6
idf.py flash

# Monitor the output
idf.py monitor
```

Or all-in-one command:
```bash
idf.py flash monitor
```

## First Boot (2 Minutes)

When the device first boots, it will create a WiFi hotspot:

1. **Find the WiFi network** named `TempDisplay_XXXXXX`
2. **Connect to it** from your phone/computer
3. **Open browser** and go to `http://192.168.4.1`
4. **Select your WiFi** network and enter the password
5. The display should **restart and connect** to the internet

## What to Expect

Once connected:
1. The NeoPixel display will light up
2. You should see your temperature reading appear
3. The color will change based on temperature:
   - 🟢 **Green**: Cold (-20°C and below)
   - 🔵 **Cyan**: Cool (5°C)
   - 🟡 **Yellow**: Warm (20°C)
   - 🟠 **Orange**: Hot (35°C)
   - 🔴 **Red**: Very hot (50°C and above)

## Check the Serial Monitor

In the monitor output, you should see:
```
I (123) app_main: ESP32-C6 Temperature Display Starting
I (200) neopixel_display: NeoPixel display initialized successfully
...
I (5200) mqtt_temp: Temperature updated: 22.5°C
```

## Troubleshooting

**Display not lighting up?**
- Check power supply to LEDs
- Verify GPIO 8 is connected (or your configured GPIO)
- Check LED strip polarity (DIN, GND, VCC)

**Not connecting to WiFi?**
- Look in serial monitor for the access point name
- Try connecting to `TempDisplay_XXXXXX` hotspot

**No temperature showing?**
- Verify MQTT topic is correct (topics are case-sensitive!)
- Check Home Assistant MQTT is working
- See HA_SETUP.md for detailed troubleshooting

**Want to change settings later?**
```bash
idf.py menuconfig
# Make changes
idf.py build flash monitor
```

## Next Steps

- Read [README.md](README.md) for detailed documentation
- Check [HA_SETUP.md](HA_SETUP.md) for Home Assistant integration details
- Explore configuration options in `menuconfig`
- Customize colors or behavior by editing `neopixel_display.c`

## Common Customizations

### Change the GPIO pin
In `menuconfig`:
- Navigate to `Temperature Display Configuration` → `NeoPixel Display Configuration`
- Change `NeoPixel Data GPIO` (valid: 0-48 on ESP32-C6)

### Change the MQTT topic
In `menuconfig`:
- Navigate to `Temperature Display Configuration` → `MQTT Configuration`
- Set `Temperature Topic` to a different MQTT topic

### Change display brightness
In `menuconfig`:
- Navigate to `Temperature Display Configuration` → `NeoPixel Display Configuration`
- Set `Initial Brightness` (0-255, where 255 is maximum)

### Adjust temperature colors
Edit `main/neopixel_display.c`, function `neopixel_get_color_for_temp()` to modify:
- `TEMP_COLD_MIN`: Coldest temperature threshold
- `TEMP_COOL`, `TEMP_WARM`, `TEMP_HOT`: Transition points
- `TEMP_HOT_MAX`: Hottest temperature threshold

## Hardware Issues?

If your display isn't working:

1. **Check LED strip**: Use an online WS2812B tester to verify LEDs work
2. **Check connections**: Try a shorter cable from ESP32 to display
3. **Add capacitor**: Place 100µF capacitor across LED power and ground
4. **Reduce brightness**: Lower brightness in configuration
5. **Change GPIO**: Try a different GPIO pin and reconfigure

## Support

- Read the full [README.md](README.md)
- Check [HA_SETUP.md](HA_SETUP.md) for Home Assistant details
- Review [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/)

## Success!

Your temperature display should now be working! 🎉

The display will automatically update whenever the temperature changes in Home Assistant, and the colors will dynamically reflect the temperature readings.
