# Configuration Guide

This guide will help you configure the ESP32-S3 ThingsBoard DHT Sensor project.

## Step 1: WiFi Configuration

Open `main/main.c` and locate the WiFi configuration section:

```c
// WiFi Configuration
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASS      "YOUR_WIFI_PASSWORD"
```

Replace these values with your WiFi credentials:
- `WIFI_SSID`: Your WiFi network name (case-sensitive)
- `WIFI_PASS`: Your WiFi password

Example:
```c
#define WIFI_SSID      "MyHomeNetwork"
#define WIFI_PASS      "MySecurePassword123"
```

**Important Notes:**
- ESP32 only supports 2.4GHz WiFi networks (not 5GHz)
- SSID and password are case-sensitive
- Make sure your network uses WPA2 security

## Step 2: ThingsBoard Configuration

### 2.1 Create ThingsBoard Account and Device

1. Go to https://thingsboard.cloud/ (or your ThingsBoard instance)
2. Sign up for a free account or login
3. After logging in, click on "Devices" in the left menu
4. Click the "+" button to add a new device
5. Fill in the device details:
   - Name: `ESP32-DHT-Sensor` (or any name you prefer)
   - Device profile: Default
6. Click "Add"
7. The device will appear in your device list

### 2.2 Get Device Access Token

1. Click on your newly created device in the device list
2. Click on "Details" tab (or "Credentials" depending on version)
3. You will see "Access token" field
4. Copy this token (it looks like: `A1B2C3D4E5F6G7H8I9J0`)
5. Keep this token secure - it's like a password for your device

### 2.3 Configure in Code

Open `main/main.c` and locate the ThingsBoard configuration:

```c
// ThingsBoard Configuration
#define THINGSBOARD_HOST   "thingsboard.cloud"
#define THINGSBOARD_PORT   1883
#define THINGSBOARD_TOKEN  "YOUR_DEVICE_TOKEN"
```

Replace `YOUR_DEVICE_TOKEN` with your actual access token:

```c
#define THINGSBOARD_TOKEN  "A1B2C3D4E5F6G7H8I9J0"
```

**If using self-hosted ThingsBoard:**
- Change `THINGSBOARD_HOST` to your server's IP or domain
- Example: `#define THINGSBOARD_HOST   "192.168.1.100"`
- Or: `#define THINGSBOARD_HOST   "my-thingsboard.example.com"`

## Step 3: DHT Sensor Configuration

### 3.1 Choose Your Sensor Type

The project supports both DHT11 and DHT22 sensors. Locate this line in `main/main.c`:

```c
#define DHT_TYPE       DHT_TYPE_DHT22
```

**For DHT11 sensor:**
```c
#define DHT_TYPE       DHT_TYPE_DHT11
```

**For DHT22 sensor (AM2302):**
```c
#define DHT_TYPE       DHT_TYPE_DHT22
```

**Differences:**
- DHT11: Temperature range 0-50°C, Humidity 20-90%, ±2°C/±5% accuracy, integer values
- DHT22: Temperature range -40-80°C, Humidity 0-100%, ±0.5°C/±2% accuracy, decimal values

### 3.2 Configure GPIO Pin

By default, the DHT sensor is connected to GPIO4. If you're using a different pin:

```c
#define DHT_GPIO       GPIO_NUM_4
```

Change to your pin number. Available pins on ESP32-S3:
- GPIO1-GPIO21 (avoid strapping pins 0, 3, 45, 46)
- Recommended pins: 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14

Example for GPIO5:
```c
#define DHT_GPIO       GPIO_NUM_5
```

## Step 4: Reading Interval Configuration

Control how often the sensor reads and sends data:

```c
#define READING_INTERVAL_SEC 10
```

- Default: 10 seconds
- Minimum recommended: 5 seconds (DHT sensors need time between readings)
- For DHT11: minimum 1 second
- For DHT22: minimum 2 seconds
- For battery operation: increase to 60+ seconds

## Step 5: Hardware Connection

### DHT Sensor Pinout

**DHT11/DHT22 (3-pin module):**
```
+  (VCC)  -> ESP32-S3 3.3V
-  (GND)  -> ESP32-S3 GND
S  (DATA) -> ESP32-S3 GPIO4 (or your configured pin)
```

**DHT11/DHT22 (4-pin standalone):**
```
Pin 1 (VCC)  -> ESP32-S3 3.3V
Pin 2 (DATA) -> ESP32-S3 GPIO4 (with 10kΩ pull-up to 3.3V)
Pin 3 (NC)   -> Not connected
Pin 4 (GND)  -> ESP32-S3 GND
```

**Pull-up Resistor:**
- Required for 4-pin standalone sensors
- Usually built-in for 3-pin modules
- Value: 4.7kΩ to 10kΩ between DATA and VCC
- Improves signal reliability

### Example Wiring Diagram

```
ESP32-S3                    DHT22
                         ┌─────────┐
3.3V ────────────────────┤ VCC (+) │
                         │         │
GPIO4 ───────────────────┤ DATA(S) │
     │                   │         │
     └───[10kΩ]───3.3V   │         │
                         │         │
GND ─────────────────────┤ GND (-) │
                         └─────────┘
```

## Configuration Verification Checklist

Before building, verify:

- [ ] WiFi SSID is correct and matches your network
- [ ] WiFi password is correct
- [ ] Your network is 2.4GHz
- [ ] ThingsBoard token is copied correctly (no extra spaces)
- [ ] DHT sensor type matches your hardware (DHT11 or DHT22)
- [ ] GPIO pin number matches your wiring
- [ ] Pull-up resistor is installed (for 4-pin sensors)
- [ ] All connections are secure

## Quick Test Configuration

For initial testing, you can use these conservative settings:

```c
#define READING_INTERVAL_SEC 30    // Read every 30 seconds
#define DHT_TYPE       DHT_TYPE_DHT22  // Or DHT11 for your sensor
#define DHT_GPIO       GPIO_NUM_4      // Standard GPIO pin
```

This gives you time to verify each reading without overwhelming ThingsBoard.

## Next Steps

After configuration:
1. Save all changes to `main/main.c`
2. Proceed to building the project (see README.md)
3. Flash to your ESP32-S3
4. Monitor output to verify connection
5. Check ThingsBoard dashboard for incoming data

## Troubleshooting Configuration Issues

**WiFi won't connect:**
- Double-check SSID and password (case-sensitive)
- Verify 2.4GHz network
- Check for special characters in password (some need escaping)

**MQTT won't connect:**
- Verify token is correct (copy-paste recommended)
- Check ThingsBoard host address
- Ensure device exists in ThingsBoard

**Sensor reads incorrect values:**
- Verify DHT_TYPE matches your actual sensor
- Check wiring and pull-up resistor
- Increase READING_INTERVAL_SEC
- Try different GPIO pin

**Device keeps resetting:**
- Check power supply (needs stable 3.3V)
- Verify USB cable quality
- Reduce reading frequency
