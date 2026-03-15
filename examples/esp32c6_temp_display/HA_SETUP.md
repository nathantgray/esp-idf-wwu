# Home Assistant MQTT Setup Guide

This guide helps you configure your Home Assistant MQTT broker and sensor to work with the ESP32-C6 Temperature Display.

## Prerequisites

- Home Assistant installed and running
- MQTT Broker integration enabled (Mosquitto or similar)
- A temperature sensor in Home Assistant (Zigbee, Zwave, or template)

## Step 1: Verify MQTT Broker is Enabled

1. In Home Assistant, go to **Settings** → **Devices & Services**
2. Look for **MQTT** in the integrations list
3. If not present, click **Create Automation** → add **MQTT** integration
4. Note the broker address and port (usually `localhost` or `core-mosquitto`, port `1883`)

## Step 2: Get MQTT Credentials

1. Go to Home Assistant **Settings** → **Users**
2. Create a dedicated user for the display device OR use your admin account
3. Note the username and password

## Step 3: Find Your Temperature Sensor's MQTT Topic

### Method A: Using Home Assistant UI

For Zigbee2MQTT sensors:
1. Go to **Settings** → **Integrations**
2. Click **Zigbee2MQTT** (or your integration)
3. Find your temperature sensor
4. The MQTT topic will be like: `zigbee2mqtt/device_name/temperature`

For other MQTT devices:
1. Go to **Developer Tools** → **MQTT**
2. Subscribe to `#` to see all MQTT topics
3. Look for your sensor's temperature topic
4. Common patterns:
   - `homeassistant/sensor/XXX/temperature/state`
   - `zigbee2mqtt/XXX/temperature`
   - `zwave/XXX/temperature`

### Method B: Create a Template Sensor

If your sensor doesn't have an accessible MQTT topic, create a template sensor:

Add to your `configuration.yaml`:
```yaml
template:
  - sensor:
      - name: "Living Room Temperature for Display"
        unique_id: "temp_display_sensor"
        unit_of_measurement: "°C"
        device_class: temperature
        state: >
          {% set sensor = state_attr('sensor.your_actual_sensor', 'unit_of_measurement') %}
          {{ states('sensor.your_actual_sensor') | float(0) }}
```

Then the MQTT topic would be: `homeassistant/sensor/template/temperature_for_display/state`

### Method C: Check Existing Entity

1. In Home Assistant, find your temperature sensor entity
2. Go to **Developer Tools** → **States**
3. Search for your entity
4. Each entity in Home Assistant typically publishes to:
   ```
   homeassistant/sensor/[DEVICE_ID]/[ENTITY_NAME]/state
   ```

## Step 4: Configure the ESP32 Display

1. Get the MQTT connection details:
   - Broker URI: `mqtt://homeassistant.local:1883`
     - Or IP-based: `mqtt://192.168.1.100:1883`
   - Username: Your Home Assistant MQTT user
   - Password: Your MQTT password
   - Topic: The MQTT topic from Step 3

2. During initial WiFi provisioning or `idf.py menuconfig`:
   - Set MQTT Broker URI
   - Set MQTT Username
   - Set MQTT Password
   - Set Temperature Topic (from Step 3)

## Step 5: Test MQTT Connectivity

### Option A: Using Home Assistant MQTT Tool

1. Go to **Developer Tools** → **MQTT**
2. Subscribe to your temperature topic
3. You should see temperature values appear
4. Send a test message:
   - Topic: `homeassistant/sensor/TEST/state`
   - Payload: `22.5`
   - You should see it in the display

### Option B: Using MQTT Explorer

Download [MQTT Explorer](http://mqtt-explorer.com/) and:
1. Connect to your MQTT broker
2. Navigate to your temperature topic
3. Verify messages are being published
4. Watch for the display connecting and subscribing

## MQTT Message Formats

The display supports multiple MQTT payload formats:

### Plain Number (Recommended)
```
22.5
-10.3
35
```

### JSON with "value" field
```json
{"value": 22.5, "unit": "°C"}
```

### JSON with "temperature" field
```json
{"temperature": 22.5, "humidity": 65}
```

### JSON with "state" field
```json
{"state": 22.5, "status": "ok"}
```

## Troubleshooting

### Broker Unreachable

**Symptom**: "MQTT error" messages in the log

**Solutions**:
1. Check the broker URI format: `mqtt://hostname:port`
2. Verify firewall allows port 1883
3. Check broker is running: `mosquitto` in Home Assistant logs
4. Use full IP address instead of hostname: `mqtt://192.168.1.100:1883`
5. Check credentials are correct

### No Temperature Update

**Symptom**: Display is blank or shows "0.0"

**Solutions**:
1. Verify MQTT topic is correct
2. Check temperature messages are being published to that topic
3. Check payload format is supported (see above)
4. Verify device is subscribed: look for "MQTT subscribed" in logs
5. Use MQTT Explorer to manually verify messages

### Connecting but No Data

**Symptom**: "MQTT connected" appears but display doesn't update

**Solutions**:
1. Check the exact MQTT topic - topics are case-sensitive
2. Verify temperature sensor is generating data
3. Check MQTT Explorer to confirm messages are published
4. Look at device logs for subscription confirmation
5. Try publishing a test value manually

## Advanced: Node-RED Integration

You can use Node-RED to transform sensor data:

1. Create an MQTT Subscribe node for your actual sensor
2. Add a Function node:
```javascript
// Ensure value is a number
msg.payload = parseFloat(msg.payload);
return msg;
```
3. Create an MQTT Publish node to `home/display/temperature`
4. Deploy

Then set the display to subscribe to `home/display/temperature`.

## SSL/TLS Support

For secure MQTT with self-signed certificates:

1. Edit `sdkconfig.defaults` and set `CONFIG_MQTT_TRANSPORT_SSL=y`
2. Add certificate handling in the MQTT initialization
3. Use `mqtts://` URI format instead of `mqtt://`

## Multiple Display Sync

To sync multiple displays:

1. Configure each display with the same temperature topic
2. They will all update simultaneously when temperature changes
3. Ensure MQTT message retain is set appropriately

## References

- [Home Assistant MQTT Integration](https://www.home-assistant.io/integrations/mqtt/)
- [Mosquitto MQTT Broker](https://mosquitto.org/)
- [MQTT Protocol Specification](https://mqtt.org/)
- [Home Assistant MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)
