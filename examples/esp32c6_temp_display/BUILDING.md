# Building the Project

This guide explains how to build and compile the ESP32-C6 Temperature Display project.

## Prerequisites

### System Requirements
- Linux, macOS, or Windows (with WSL/MSYS2)
- Python 3.7 or newer
- Git

### ESP-IDF Installation
Follow the [official ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/)

Verify installation:
```bash
idf.py --version
# Should output something like: ESP-IDF v5.1.2
```

### Environment Setup
Ensure `IDF_PATH` is set:
```bash
echo $IDF_PATH
# Should show ESP-IDF installation path, e.g., /home/user/esp/esp-idf
```

If not set, source the activation script:
```bash
source ~/esp/esp-idf/export.sh  # On Linux/macOS
# or
%IDF_PATH%\export.bat           # On Windows cmd
# or
. $env:IDF_PATH/export.ps1      # On Windows PowerShell
```

## Building Steps

### 1. Navigate to Project
```bash
cd examples/esp32c6_temp_display
```

### 2. Configure the Project
```bash
idf.py set-target esp32c6
idf.py menuconfig
```

The first command ensures the project is configured for ESP32-C6 specifically.

In the menu:
- Navigate using arrow keys
- Enter to edit
- Space to select
- S to save
- Q to exit

**Required settings:**
- `Temperature Display Configuration` → `MQTT Configuration`:
  - Set MQTT Broker URI
  - Set MQTT Username and Password
  - Set Temperature Topic

**Optional settings:**
- `Temperature Display Configuration` → `NeoPixel Display Configuration`:
  - GPIO pin for display (default: 8)
  - Brightness (default: 200)

### 3. Build the Project
```bash
idf.py build
```

This will:
1. Download required components (first build only)
2. Compile all source files
3. Link the final binary
4. Generate the firmware image

**Expected output:**
```
[100%] Linking CXX executable esp32c6_temp_display.elf
[100%] Generating esp32c6_temp_display.bin
Project built successfully!
```

### 4. Verify Build
Check the `build/` directory:
```bash
ls -la build/
# You should see:
# - esp32c6_temp_display.elf     (executable)
# - esp32c6_temp_display.bin     (firmware image)
# - build.log                     (build output log)
```

## Flashing the Device

### Connect Your Device
1. Connect ESP32-C6 via Micro USB to your computer
2. Identify the port:
   - **Linux**: `/dev/ttyUSB0` or `/dev/ttyACM0`
   - **macOS**: `/dev/tty.usbserial-*`
   - **Windows**: `COM3` or higher

### Flash Command
```bash
idf.py -p /dev/ttyUSB0 flash
```

Or let ESP-IDF auto-detect:
```bash
idf.py flash
```

**Expected output:**
```
Connecting....
Detecting chip type... esp32c6
Chip type: esp32c6
Flashing will use fast UART mode
...
Hash of data verified
Leaving... Hard resetting via RTS pin...
```

### Combined Build and Flash
```bash
idf.py build flash
```

## Monitoring

### Serial Monitor
```bash
idf.py monitor
```

This opens a terminal showing:
- Boot messages
- System initialization
- MQTT connection attempts
- Temperature updates
- Error messages

**Common output:**
```
ESP-ROM:esp32c6-20220919
build:Sep 19 2022
...
I (123) app_main: ========================================
I (124) app_main: ESP32-C6 Temperature Display Starting
I (200) neopixel_display: NeoPixel display initialized successfully
I (300) wifi_event: WIFI_EVENT_STA_START
...
I (5200) mqtt_temp: Temperature updated: 22.5°C
```

### Connected Build and Flash with Monitor
```bash
idf.py flash monitor
```

This is the most convenient command - it:
1. Builds the project
2. Flashes to device
3. Immediately opens the serial monitor

**To exit monitor:** Press `Ctrl+]`

## Clean Build

If you encounter build issues:
```bash
idf.py clean
idf.py build
```

Or complete clean:
```bash
rm -rf build/
idf.py build
```

## Advanced Build Options

### Verbose Output
```bash
idf.py -v build
```

Shows all compiler commands and detailed build steps.

### Specify Multiple Targets
Build and flash in one command with custom port:
```bash
idf.py -p COM3 build flash monitor
```

### Check Dependencies
```bash
idf.py dependency-tree
```

Shows all component dependencies for this project.

### Size Analysis
```bash
idf.py size
```

Shows binary size breakdown:
```
RAM:   used:     xxxxxx  available: yyyyyy
Flash: used:    xxxxxx  available: yyyyyy
```

### Partition Analysis
```bash
idf.py partition-table
```

Shows flash memory layout.

## Troubleshooting Build Issues

### Component Not Found
**Error**: `Cannot find ESP component with name: neopixel`

**Solution**:
1. Ensure ESP-IDF Component Manager is set up
2. Run `idf.py clean` and rebuild
3. Check internet connection (components are downloaded)

### Compilation Error in mqtt_temp.c
**Error**: `undefined reference to 'mqtt_client'`

**Solution**:
1. Verify components in `main/CMakeLists.txt`
2. Run `idf.py clean` then `idf.py build`

### Flash Size Exceeded
**Error**: `Total size of application with menu config is X bytes`

**Solutions**:
1. Reduce binary size:
   ```bash
   idf.py set-target esp32c6
   idf.py menuconfig
   # In menuconfig, disable unused features
   ```
2. Increase flash memory partition (modify `partitions.csv`)

### Port Not Found
**Error**: `Cannot open port /dev/ttyUSB0`

**On Linux**:
```bash
# List available ports
ls /dev/tty*

# Grant permissions
sudo usermod -a -G dialout $USER
# Then log out and back in
```

**On Windows**:
- Open Device Manager
- Find your ESP32 under "Ports (COM & LPT)"
- Note the COM port number

## Build Configuration Files

### `CMakeLists.txt` (Root)
- Project configuration
- Sets target and includes IDF tools

### `main/CMakeLists.txt`
- Component registration
- Lists source files and dependencies

### `main/Kconfig.projbuild`
- Configuration menu options
- MQTT settings, GPIO pins, etc.

### `sdkconfig.defaults`
- Default configuration values
- Applied on first build
- Can be overridden with menuconfig

### `idf_component.yml`
- Component dependencies
- Specifies neopixel library version

### `partitions.csv`
- Flash memory layout
- Defines NVS, firmware, and factory app partitions

## Customizing the Build

### Change Target Chip
```bash
idf.py set-target esp32s3  # Won't work, but shows syntax
idf.py set-target esp32c6  # Correct for this project
```

### Optimize for Size
In `menuconfig`:
- `Compiler Options` → `Optimization Level` → `-Os` (Optimize for size)

### Enable Logging
In `menuconfig`:
- `Component config` → `Log output` → `Default log level` → `Debug` (for verbose output)

### Pre-configured Builds
Create `sdkconfig.xxx` variants:
```bash
cp sdkconfig.defaults sdkconfig.debug
# Edit sdkconfig.debug for debug settings
idf.py set-config sdkconfig.debug build
```

## CI/CD Integration

### GitHub Actions Example
```yaml
- name: Build ESP32-C6 Temperature Display
  run: |
    . $IDF_PATH/export.sh
    cd examples/esp32c6_temp_display
    idf.py build
```

### Jenkins Pipeline
```groovy
stage('Build') {
    steps {
        sh '''
            source $IDF_PATH/export.sh
            cd examples/esp32c6_temp_display
            idf.py build
        '''
    }
}
```

## Performance Notes

### Build Time
- **First build**: 2-5 minutes (downloads components)
- **Incremental**: 10-30 seconds (only changed files)
- **Clean build**: 1-3 minutes

### Binary Size
- **Total firmware**: ~500 KB
- **RAM usage**: ~200 KB (of 320 KB available)
- **Flash usage**: ~400 KB (of 1 MB app partition)

## Next Steps

1. **Flash to device**: `idf.py flash monitor`
2. **Configure WiFi**: Connect to `TempDisplay_XXXXXX` hotspot
3. **Set MQTT**: Configure temperature topic in Home Assistant
4. **Verify operation**: Check serial monitor for "Temperature updated"

## Additional Resources

- [ESP-IDF Build System Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/build-system.html)
- [ESP-IDF CMake Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/tools/cmake.html)
- [Component Manager Documentation](https://docs.espressif.com/projects/idf-component-manager/)
- [ESP32-C6 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
