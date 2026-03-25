# ESP32-S3 ThingsBoard DHT Sensor Project

This project reads temperature and humidity data from a DHT sensor (DHT11 or DHT22) using an ESP32-S3 board and sends the data to ThingsBoard IoT platform via MQTT protocol.

## Architecture

```
ESP32-S3 -> WiFi -> Internet -> ThingsBoard MQTT Broker -> Dashboard
```

## Hardware Requirements

- ESP32-S3 development board
- DHT11 or DHT22 temperature and humidity sensor
- Connecting wires
- Power supply (USB cable)

## Wiring

Connect the DHT sensor to your ESP32-S3:

- DHT VCC -> ESP32-S3 3.3V
- DHT GND -> ESP32-S3 GND
- DHT DATA -> ESP32-S3 GPIO4 (configurable in main.c)

Add a 10kΩ pull-up resistor between DHT DATA and VCC for stable operation.

## Software Requirements

- ESP-IDF v5.0 or later
- Python 3.8 or later
- Git

## ThingsBoard Setup

1. Create a free account at https://thingsboard.cloud/ or use your own ThingsBoard instance
2. Login to your ThingsBoard account
3. Navigate to "Devices" section
4. Click "+" to add a new device
5. Enter a device name (e.g., "ESP32-DHT-Sensor")
6. Click "Add"
7. Click on the newly created device and go to "Details" tab
8. Copy the "Access token" - you'll need this for configuration

## Configuration

Before building the project, you need to configure the following parameters in `main/main.c`:

### WiFi Configuration
```c
#define WIFI_SSID      "YOUR_WIFI_SSID"        // Replace with your WiFi SSID
#define WIFI_PASS      "YOUR_WIFI_PASSWORD"    // Replace with your WiFi password
```

### ThingsBoard Configuration
```c
#define THINGSBOARD_HOST   "thingsboard.cloud"      // Change if using self-hosted
#define THINGSBOARD_PORT   1883                      // Default MQTT port
#define THINGSBOARD_TOKEN  "YOUR_DEVICE_TOKEN"      // Device access token from ThingsBoard
```

### DHT Sensor Configuration
```c
#define DHT_GPIO       GPIO_NUM_4              // GPIO pin connected to DHT sensor
#define DHT_TYPE       DHT_TYPE_DHT22          // Change to DHT_TYPE_DHT11 if using DHT11
```

### Reading Interval
```c
#define READING_INTERVAL_SEC 10                // Sensor reading interval in seconds
```

## Building and Flashing

1. Install ESP-IDF by following the official guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/

2. Set up ESP-IDF environment:
```bash
. $HOME/esp/esp-idf/export.sh
```

3. Navigate to project directory:
```bash
cd esp32_thingsboard_project
```

4. Configure the project:
```bash
idf.py set-target esp32s3
idf.py menuconfig
```

5. Build the project:
```bash
idf.py build
```

6. Flash to ESP32-S3 (replace PORT with your serial port):
```bash
idf.py -p PORT flash
```

7. Monitor the output:
```bash
idf.py -p PORT monitor
```

Press `Ctrl+]` to exit the monitor.

## Creating ThingsBoard Dashboard

1. Login to ThingsBoard
2. Navigate to "Dashboards" section
3. Click "+" to create a new dashboard
4. Add widgets:
   - Add "Cards" widget for current temperature
   - Add "Cards" widget for current humidity
   - Add "Time-series chart" for temperature history
   - Add "Time-series chart" for humidity history

5. Configure each widget:
   - Select your device as the data source
   - Select "temperature" or "humidity" as the key
   - Customize appearance as desired

## Expected Output

When running successfully, you should see output similar to:

```
I (1234) MAIN: Starting ESP32 ThingsBoard DHT Sensor
I (1244) MAIN: Initializing DHT sensor on GPIO 4
I (1254) MAIN: Initializing WiFi
I (2345) MAIN: Connected to AP SSID:YourWiFi
I (2355) MAIN: Got IP:192.168.1.100
I (2365) MAIN: Initializing MQTT
I (2375) MAIN: MQTT client initialized and started
I (3456) MAIN: MQTT_EVENT_CONNECTED
I (3466) DHT: Temperature: 23.5°C, Humidity: 65.2%
I (3476) MAIN: Published telemetry: {"temperature":23.5,"humidity":65.2}
```

## Troubleshooting

### WiFi Connection Issues
- Verify SSID and password are correct
- Ensure WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Check WiFi signal strength

### MQTT Connection Issues
- Verify ThingsBoard host and port
- Confirm device access token is correct
- Check internet connectivity

### DHT Sensor Reading Errors
- Verify wiring connections
- Ensure pull-up resistor is installed
- Check sensor type configuration (DHT11 vs DHT22)
- Try increasing reading interval
- Verify GPIO pin number is correct

### Build Errors
- Ensure ESP-IDF is properly installed and environment is set
- Verify target is set to esp32s3: `idf.py set-target esp32s3`
- Clean and rebuild: `idf.py fullclean && idf.py build`

## Data Format

The device publishes telemetry data to ThingsBoard in JSON format:
```json
{
  "temperature": 23.5,
  "humidity": 65.2
}
```

## License

This project is provided as-is for educational and development purposes.

## Support

For ESP-IDF related issues, refer to: https://docs.espressif.com/projects/esp-idf/
For ThingsBoard documentation, visit: https://thingsboard.io/docs/
