# ESP32-S3 Custom eFuse Generation Demo

![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Supported-green)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.0+-blue)
![License](https://img.shields.io/badge/license-MIT-blue)

A comprehensive demonstration of defining, generating, and using custom eFuse fields on the ESP32-S3 using ESP-IDF's build-time eFuse table generator. This project shows how to integrate custom eFuse definitions into your build system and safely program and read device-specific data.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Custom eFuse Fields](#custom-efuse-fields)
- [How It Works](#how-it-works)
- [Project Structure](#project-structure)
- [Build and Flash](#build-and-flash)
- [Configuration Options](#configuration-options)
- [Safety Considerations](#safety-considerations)
- [Example Output](#example-output)
- [Technical Details](#technical-details)
- [Troubleshooting](#troubleshooting)
- [Resources](#resources)
- [Contributing](#contributing)
- [License](#license)

## Overview

eFuses (electronic fuses) are one-time programmable (OTP) memory bits in the ESP32-S3 that can store critical device information such as serial numbers, hardware revisions, feature flags, MAC addresses, and encryption keys. Once programmed, eFuses cannot be erased or changed (bits can only transition from 0 to 1).

This project demonstrates:
- Defining custom user eFuse fields in a CSV table
- Automatically generating C code and header files during the build process
- Reading eFuse values using the ESP-IDF eFuse API
- Safely programming eFuse values with batch mode and CRC validation
- Using virtual eFuses for safe testing without permanent hardware changes

## Features

- **Build-Time Code Generation**: Automatically generates `esp_efuse_custom_table.c` and `esp_efuse_custom_table.h` from CSV definitions
- **Custom Field Definitions**: Example fields for serial number, hardware revision, feature flags, and CRC checksum
- **CRC-16 Validation**: Implements CRC-16/CCITT-FALSE for data integrity verification
- **Batch Write Mode**: Properly handles Reed-Solomon encoded user blocks on ESP32-S3
- **Virtual eFuse Support**: Safe testing mode that doesn't burn real silicon
- **Complete API Example**: Demonstrates both reading and writing custom eFuse fields
- **Production-Ready Pattern**: Easily adaptable for real manufacturing workflows

## Prerequisites

- **ESP-IDF**: v5.0 or later ([Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/))
- **ESP32-S3 Development Board**: Any ESP32-S3 based board
- **Python**: 3.8 or later (included with ESP-IDF)
- **Build Tools**: CMake, Ninja (included with ESP-IDF)

## Custom eFuse Fields

This project defines the following custom fields in `EFUSE_BLK3` (USER_DATA block):

| Field Name | Block | Bit Start | Bit Count | Description |
|------------|-------|-----------|-----------|-------------|
| `USER_DATA.SERIAL_NUMBER` | EFUSE_BLK3 | 0 | 128 | Device serial number (16 bytes ASCII, zero-padded) |
| `USER_DATA.HW_REV` | EFUSE_BLK3 | 128 | 16 | Hardware revision (uint16) |
| `USER_DATA.FEATURE_FLAGS` | EFUSE_BLK3 | 144 | 32 | Feature flags (uint32) |
| `USER_DATA.PROVISIONING_CRC16` | EFUSE_BLK3 | 176 | 16 | CRC16 over SERIAL_NUMBER+HW_REV+FEATURE_FLAGS |

These fields are defined in `main/esp_efuse_custom_table.csv` and can be easily modified for your specific requirements.

## How It Works

See [FLOWCHARTS.md](FLOWCHARTS.md) for detailed visual flowcharts of the build process and runtime operation.

### Build Process

1. CMake detects `main/esp_efuse_custom_table.csv`
2. Runs `efuse_table_gen.py` with common and custom CSV files
3. Generates `esp_efuse_custom_table.c` and `.h` files
4. Compiles generated code into the application
5. Creates firmware with custom eFuse accessors

### Runtime Operation

1. Application starts and optionally provisions fields
2. Uses batch write mode for Reed-Solomon encoded blocks
3. Reads back values and validates with CRC-16
4. Displays results with integrity check

## Project Structure

```
ESP32S3_Custom_Efuse_Gen_Demo/
├── CMakeLists.txt                  # Top-level CMake configuration
├── README.md                        # This file
├── FLOWCHARTS.md                   # Detailed visual flowcharts
├── FAQ.md                          # Frequently asked questions
├── CONTRIBUTING.md                 # Contribution guidelines
├── LICENSE                         # MIT License
├── .gitignore                      # Git ignore rules
├── sdkconfig.defaults              # Default SDK configuration
├── .github/
│   └── workflows/
│       └── build.yml              # CI/CD pipeline
└── main/
    ├── CMakeLists.txt              # Component CMake with code generation
    ├── Kconfig.projbuild           # Configuration menu
    ├── esp_efuse_custom_table.csv  # Custom eFuse definitions
    ├── main.c                      # Main application code
    └── include/                    # Generated header files
        └── esp_efuse_custom_table.h  # Auto-generated header
```

## Build and Flash

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/ESP32S3_Custom_Efuse_Gen_Demo.git
cd ESP32S3_Custom_Efuse_Gen_Demo
```

### 2. Set up ESP-IDF Environment

```bash
. $HOME/esp/esp-idf/export.sh  # Linux/macOS
# or
%IDF_TOOLS_PATH%\export.bat    # Windows
```

### 3. Set Target

```bash
idf.py set-target esp32s3
```

### 4. Configure (Optional)

```bash
idf.py menuconfig
```

Navigate to **Custom eFuse Demo** to enable/disable programming mode.

### 5. Build

```bash
idf.py build
```

### 6. Flash and Monitor

```bash
idf.py -p /dev/ttyUSB0 flash monitor
# Replace /dev/ttyUSB0 with your serial port
```

## Configuration Options

### Virtual eFuse Mode (Default - SAFE)

Default configuration uses virtual eFuses:

```
CONFIG_EFUSE_VIRTUAL=y
CONFIG_EFUSE_VIRTUAL_KEEP_IN_FLASH=y
```

✅ **Safe for testing** - No permanent changes to hardware

### Real eFuse Mode (⚠️ PERMANENT)

To burn real eFuses:

1. `idf.py menuconfig`
2. **Component config → eFuse Bit Manager**
3. Disable **Virtual eFuses**
4. Save and rebuild

⚠️ **WARNING**: This is IRREVERSIBLE!

### Enable Programming

1. `idf.py menuconfig`
2. **Custom eFuse Demo**
3. Enable **Program custom eFuse fields on boot**
4. Rebuild and flash

## Safety Considerations

### ⚠️ CRITICAL: eFuses are One-Time Programmable

- **Bits can only change from 0 → 1, NEVER back to 0**
- **Programming is PERMANENT and IRREVERSIBLE**
- **ALWAYS test with virtual eFuses first** (enabled by default)
- **Verify field definitions** before burning real silicon
- **Use batch write mode** for user blocks (required on ESP32-S3)
- **Implement data validation** (CRC/checksums)

### Best Practices

1. ✅ **Development**: Use `CONFIG_EFUSE_VIRTUAL=y`
2. ✅ **Testing**: Verify all operations in virtual mode
3. ✅ **Staging**: Test on dev boards with real eFuses
4. ✅ **Production**: Deploy with manufacturing safeguards
5. ✅ **Validation**: Always read back and verify

## Example Output

### Unprogrammed (First Run)

```
I (329) custom_efuse_demo: Custom eFuse demo starting (target=esp32s3)
I (339) custom_efuse_demo: SERIAL_NUMBER: ''
I (339) custom_efuse_demo: HW_REV: 0x0000 (0)
I (349) custom_efuse_demo: FEATURE_FLAGS: 0x00000000
I (359) custom_efuse_demo: PROVISIONING_CRC16: 0x0000
W (369) custom_efuse_demo: CRC16 stored is 0x0000 (likely not provisioned yet)
```

### After Programming

```
I (329) custom_efuse_demo: Custom eFuse demo starting (target=esp32s3)
W (339) custom_efuse_demo: CONFIG_DEMO_PROGRAM_EFUSE is enabled
I (349) custom_efuse_demo: Provisioning committed (CRC16=0x8F3A)
I (359) custom_efuse_demo: SERIAL_NUMBER: 'SN-ESP32S3-0001'
I (369) custom_efuse_demo: HW_REV: 0x0001 (1)
I (379) custom_efuse_demo: FEATURE_FLAGS: 0x0000000F
I (389) custom_efuse_demo: PROVISIONING_CRC16: 0x8F3A
I (399) custom_efuse_demo: CRC16 recalculated: 0x8F3A
I (409) custom_efuse_demo: CRC16 check: OK
```

## Technical Details

### Generated Code Structure

The build system generates accessor constants:

```c
extern const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_SERIAL_NUMBER[];
extern const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_HW_REV[];
extern const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_FEATURE_FLAGS[];
extern const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_PROVISIONING_CRC16[];
```

### CRC-16 Algorithm

Implements CRC-16/CCITT-FALSE:
- Polynomial: 0x1021
- Init: 0xFFFF
- No reflection
- XOR out: 0x0000

### Reed-Solomon Encoding

ESP32-S3 USER_DATA blocks use RS error correction:
- Requires batch write mode
- Automatic error detection/correction
- Managed by ESP-IDF eFuse API

## Troubleshooting

See [FAQ.md](FAQ.md) for comprehensive troubleshooting guide.

### Common Issues

**Build fails**: Ensure ESP-IDF environment is sourced
```bash
. $HOME/esp/esp-idf/export.sh
```

**ESP_ERR_EFUSE_CNT_IS_FULL**: Bits already programmed
- Use virtual eFuses for testing
- Cannot reprogram bits that are already 1

**CRC mismatch**: Check byte order and payload construction

## Resources

- [ESP-IDF eFuse Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/efuse.html)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Espressif Systems for ESP-IDF and comprehensive eFuse documentation
- ESP32 community for feedback and testing

---

**⚠️ Remember**: eFuse programming is permanent. Always test thoroughly with virtual eFuses before deploying to production hardware!
