# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned
- Add ESP32-S3 specific optimizations
- Add performance metrics collection
- Web-based configuration interface
- Additional cooperative scheduling patterns

## [1.0.0] - 2024-01-03

### Added
- Initial release of ESP32 Scheduling Demo
- Preemptive scheduling mode with FreeRTOS tasks
- Cooperative scheduling mode with event loop
- Mutex-based synchronization for preemptive mode
- Queue-based event handling for cooperative mode
- Comprehensive README with architecture diagrams
- Detailed Mermaid flowcharts (FLOWCHARTS.md)
- Menuconfig integration for mode selection
- Configurable queue length and timer period
- Educational code comments and documentation
- MIT-0 License (public domain equivalent)
- CI/CD workflow with GitHub Actions
- Contributing guidelines
- Example outputs for both modes

### Features

#### Preemptive Mode
- Three concurrent FreeRTOS tasks (Sensor, Network, High Priority)
- Priority-based preemption demonstration
- Mutex-protected shared counter
- Configurable task priorities and periods
- Visible CPU time sharing in logs

#### Cooperative Mode
- Single event loop task
- Timer-driven event generation
- Three event types (SENSOR, NET, UI)
- Run-to-completion event handlers
- Queue overflow handling
- Configurable queue depth and timer period

### Technical Specifications
- ESP-IDF v4.4+ compatible (v5.0+ recommended)
- Supports ESP32, ESP32-S2, ESP32-C3, and other variants
- Stack size: 4096 bytes per task
- Default queue length: 16 events
- Default timer period: 250ms
- Comprehensive logging with ESP_LOG macros

### Documentation
- Architecture overview with system diagrams
- Detailed API documentation in code
- Usage examples and expected outputs
- Troubleshooting guide
- Comparison table: Preemptive vs Cooperative
- Use case recommendations

[Unreleased]: https://github.com/yourusername/esp32-scheduling-demo/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/yourusername/esp32-scheduling-demo/releases/tag/v1.0.0
