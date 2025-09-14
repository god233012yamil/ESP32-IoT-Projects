# ESP32 WiFi Scanner Project

A comprehensive WiFi scanning application for ESP32 microcontrollers using ESP-IDF framework with FreeRTOS support. Features both basic and advanced scanning capabilities with detailed network analysis.

## Project Structure

```
wifi_scanner/
├── CMakeLists.txt                 # Main project build configuration
├── sdkconfig                      # ESP-IDF SDK configuration
├── README.md                     
├── LICENSE
├── main/
│   ├── CMakeLists.txt            # Main component build file
│   ├── Kconfig.projbuild         # Custom configuration options
│   ├── wifi_scanner.c            # Basic scanner implementation
└── build/                        # Build output directory (auto-generated)
```

## Features

### Basic Scanner (wifi_scanner.c)
- **Periodic Scanning**: Configurable scan intervals (default: 10 seconds)
- **Comprehensive Display**: Shows SSID, BSSID, signal strength, channel, security
- **Memory Management**: Safe allocation and cleanup of scan results
- **Error Handling**: Robust error checking and recovery
- **FreeRTOS Integration**: Task-based architecture with proper resource management
- **Signal Quality**: Human-readable signal strength indicators
- **Security Analysis**: Detailed encryption type identification

### Advanced Scanner (wifi_scanner_advanced.c)
- **Channel Analysis**: Complete 2.4GHz spectrum usage statistics
- **Vendor Detection**: OUI-based manufacturer identification
- **Quality Scoring**: Network reliability assessment (0-100 scale)
- **Signal Tracking**: Historical RSSI monitoring with stability analysis
- **Congestion Metrics**: Channel utilization and interference detection
- **JSON Output**: Machine-readable results for integration
- **Network Persistence**: Track networks over time with statistics
- **Recommendations**: Suggest optimal channels based on analysis

## Quick Start

### Prerequisites
- ESP-IDF v4.4 or later
- ESP32 development board
- USB cable for programming and serial output

### Installation
1. **Clone the repository**:
   ```bash
   git clone <repository_url>
   cd wifi_scanner
   ```

2. **Set up ESP-IDF environment**:
   ```bash
   . $IDF_PATH/export.sh
   ```

3. **Configure the project**:
   ```bash
   idf.py menuconfig
   ```
   Navigate to "WiFi Scanner Configuration" to adjust settings.

4. **Build and flash**:
   ```bash
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

### Basic Usage
Once flashed, the scanner will automatically start and display results every 10 seconds:

```
═══════════════════════════════════════════════════════════════════════════════
                            WiFi Scan Results
═══════════════════════════════════════════════════════════════════════════════
Found 12 WiFi networks:

┌─ Access Point #1
├─ SSID: MyHomeNetwork
├─ BSSID: aa:bb:cc:dd:ee:ff
├─ Channel: 6
├─ RSSI: -45 dBm (Very Good)
├─ Security: WPA2-PSK
└─ Country: US

┌─ Access Point #2
├─ SSID: Office_WiFi_5G
├─ BSSID: 11:22:33:44:55:66
├─ Channel: 11
├─ RSSI: -67 dBm (Fair)
├─ Security: WPA2/WPA3-PSK
└─ Country: US
```

## Configuration Options

### Basic Configuration
Edit these constants in `wifi_scanner.c`:
- `WIFI_SCAN_INTERVAL_MS`: Time between scans (milliseconds)
- `MAX_AP_COUNT`: Maximum networks to process per scan
- `SCAN_TIMEOUT_MS`: Scan operation timeout

### Advanced Configuration (Kconfig)
Use `idf.py menuconfig` to configure:
- **Scan Interval**: 5-300 seconds
- **Maximum APs**: 5-50 networks per scan
- **Show Hidden Networks**: Include non-broadcast SSIDs
- **Sort by Signal**: Order results by RSSI strength
- **Channel Analysis**: Enable spectrum usage statistics
- **JSON Output**: Machine-readable format option

### Hardware Configuration
```c
// Task configuration
#define WIFI_SCANNER_TASK_STACK_SIZE    4096    // Basic: 4KB, Advanced: 8KB
#define WIFI_SCANNER_TASK_PRIORITY      5       // FreeRTOS task priority
```

## API Reference

### Core Functions

#### `esp_err_t wifi_scan_init(void)`
Initializes WiFi subsystem for scanning operations.
- **Returns**: `ESP_OK` on success, error code on failure
- **Note**: Automatically called by scanner task

#### `esp_err_t perform_wifi_scan(void)`
Executes a single WiFi scan cycle and displays results.
- **Returns**: `ESP_OK` on success, error code on failure
- **Memory**: Allocates temporary memory for scan results

#### `const char* get_auth_mode_string(wifi_auth_mode_t authmode)`
Converts ESP-IDF authentication mode to human-readable string.
- **Parameters**: WiFi authentication mode enumeration
- **Returns**: Security type string (e.g., "WPA2-PSK", "Open")

#### `const char* get_signal_strength_string(int8_t rssi)`
Converts RSSI value to qualitative signal strength.
- **Parameters**: RSSI in dBm (-100 to 0)
- **Returns**: Quality description ("Excellent", "Good", etc.)

### Advanced Functions (Advanced Scanner Only)

#### `const char* lookup_vendor(const uint8_t *bssid)`
Identifies device manufacturer from MAC address OUI.
- **Parameters**: 6-byte MAC address
- **Returns**: Vendor name or "Unknown"

#### `uint8_t calculate_quality_score(const network_stats_t *net_stats)`
Computes network quality score based on signal and stability.
- **Parameters**: Network statistics structure
- **Returns**: Quality score 0-100

#### `void update_channel_stats(const wifi_ap_record_t *ap_records, uint16_t ap_count)`
Updates channel utilization statistics from scan results.
- **Parameters**: AP records array and count
- **Side Effects**: Updates global channel statistics

## Memory Usage

### Basic Scanner
- **Task Stack**: 4KB
- **Heap Usage**: ~800 bytes per scan (temporary)
- **Static Memory**: <1KB for configuration

### Advanced Scanner
- **Task Stack**: 8KB
- **Heap Usage**: ~2-4KB per scan (includes history tracking)
- **Static Memory**: ~10KB for vendor database and statistics

## Performance Characteristics

### Scan Times
- **Active Scan**: 100-300ms per channel
- **Complete Scan**: 1.4-4.2 seconds (14 channels)
- **Processing Time**: <100ms for result analysis

### Power Consumption
- **Scanning**: ~160mA during active scan
- **Idle**: ~20mA between scans
- **Sleep Mode**: Can be integrated with deep sleep for battery applications

## Integration Examples

### JSON Output Integration
```json
{
  "metadata": {
    "version": "2.0",
    "timestamp": 1641024000,
    "scan_count": 15
  },
  "networks": [
    {
      "ssid": "MyNetwork",
      "bssid": "aa:bb:cc:dd:ee:ff",
      "channel": 6,
      "rssi": -45,
      "security": "WPA2-PSK",
      "vendor": "Apple"
    }
  ]
}
```

### Custom Event Integration
```c
// Define custom event for scan completion
ESP_EVENT_DEFINE_BASE(WIFI_SCANNER_EVENT);

// Event handler registration
esp_event_handler_register(WIFI_SCANNER_EVENT, 
                          SCAN_COMPLETE_EVENT,
                          &scan_complete_handler, 
                          NULL);
```

## Troubleshooting

### Common Issues

**1. No networks found**
- Check antenna connection
- Verify 2.4GHz availability in your region
- Increase scan timeout values

**2. Memory allocation failures**
- Reduce `MAX_AP_COUNT`
- Check available heap with `esp_get_free_heap_size()`
- Enable heap debugging for detailed analysis

**3. Scan timeouts**
- Increase `SCAN_TIMEOUT_MS`
- Check for interference sources
- Verify WiFi driver initialization

**4. Inconsistent results**
- Enable signal strength tracking
- Increase scan averaging
- Check for device mobility

### Debug Commands
```bash
# Monitor heap usage
idf.py monitor | grep "Free heap"

# Enable WiFi debug logs
idf.py menuconfig -> Component config -> Log output -> WiFi -> Debug

# Check task stack usage
CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y
```

## Contributing

### Code Style
- Follow Google C++ Style Guide
- Use ESP-IDF logging macros (`ESP_LOGI`, `ESP_LOGE`, etc.)
- Include comprehensive function documentation
- Maintain error checking for all ESP-IDF calls

### Testing
- Test on multiple ESP32 variants (ESP32, ESP32-S2, ESP32-C3)
- Verify operation in different WiFi environments
- Check memory usage under various AP counts
- Validate scan accuracy and performance

## License

This project is licensed under the MIT License. See LICENSE file for details.

## Support

For issues and questions:
- Create GitHub issues for bugs and feature requests
- Check existing documentation in `/docs` directory
- Review ESP-IDF documentation for underlying WiFi functions

## Changelog

### Version 2.0 (Advanced Scanner)
- Added channel analysis and congestion metrics
- Implemented vendor identification from OUI database
- Added network quality scoring and stability tracking
- JSON output format support
- Enhanced error handling and memory management

### Version 1.0 (Basic Scanner)  
- Initial implementation with basic WiFi scanning
- FreeRTOS task-based architecture
- Comprehensive network information display
- Memory-safe operations with proper cleanup
