# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-01-23

### Added
- Initial release of ESP32 Low-Power Reference Project
- Event-driven FreeRTOS task pattern with blocking on notifications
- ESP-IDF power management configuration (DFS + automatic light sleep)
- Deep sleep duty-cycling implementation (wake → work → sleep)
- Explicit Wi-Fi lifecycle management (connect → transmit → shutdown)
- Multiple wake source support (timer + EXT0 GPIO)
- Sensor power gating example via GPIO
- Wi-Fi manager with timeout-based connection
- Configurable project parameters via Kconfig
- Comprehensive documentation with power optimization guide
- System architecture flowchart
- Examples for battery life calculation
- Troubleshooting guide

### Features
- Works with ESP32, ESP32-S2, ESP32-S3, and ESP32-C3
- Menuconfig integration for easy configuration
- Optional Wi-Fi connectivity with automatic shutdown
- Runtime button task demonstrating event-driven pattern
- Wake cause detection and logging
- Minimal dependencies (ESP-IDF core only)

### Documentation
- Complete README with architecture diagrams
- Mermaid flowchart of system operation
- Power consumption profiles and measurement guidelines
- Customization examples for real applications
- Contributing guidelines
- MIT License

## [Unreleased]

### Planned Features
- MQTT integration example
- HTTP/HTTPS POST example
- ULP coprocessor wake example
- Battery voltage monitoring
- OTA update integration
- Touch wake support
- Multiple sensor examples (BME280, SHT31, etc.)
- LoRaWAN integration

---

For a detailed list of changes, see the [commit history](https://github.com/yourusername/esp32-low-power-reference/commits/main).
