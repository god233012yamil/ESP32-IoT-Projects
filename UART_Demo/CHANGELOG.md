# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned
- Binary protocol support (length-prefixed frames)
- Hardware flow control example (RTS/CTS)
- DMA transfer mode
- Multiple UART port example
- Power management integration

## [1.0.0] - 2025-01-02

### Added
- Initial release
- Event-driven UART reception architecture
- Fast RX task with StreamBuffer for burst absorption
- Parser task with line accumulator for newline-delimited commands
- Dedicated TX task for serialized transmission
- Support for PING, VERSION, and UPTIME commands
- Overflow protection and error handling
- Comprehensive documentation and examples
- Mermaid flowcharts for system architecture
- Testing guidelines and examples

### Features
- ESP32-S3, ESP32-S2, ESP32, and ESP32-C3 support
- 115200 baud default (configurable)
- 4KB RX/TX buffers with overflow handling
- Priority-based task scheduling
- FreeRTOS queue and stream buffer integration
- Production-ready error recovery

### Documentation
- Detailed README.md with usage examples
- ARCHITECTURE.md with Mermaid diagrams
- CONTRIBUTING.md with coding standards
- GitHub issue and PR templates
- MIT License

## Version History

### Version Numbering
- **MAJOR**: Incompatible API changes
- **MINOR**: Backward-compatible functionality additions
- **PATCH**: Backward-compatible bug fixes

### Upgrade Notes

#### From 0.x to 1.0.0
This is the first stable release. No migration needed.

## Future Roadmap

### v1.1.0 (Next Minor Release)
- [ ] Add binary protocol parser example
- [ ] Implement pattern detection (AT commands)
- [ ] Add CRC/checksum utilities
- [ ] Performance benchmarking tools

### v1.2.0
- [ ] Multi-port UART management
- [ ] Dynamic baud rate switching
- [ ] Advanced flow control strategies
- [ ] Wake-on-UART support

### v2.0.0 (Breaking Changes)
- [ ] API restructuring for multi-instance support
- [ ] Component-based architecture
- [ ] Plugin system for protocol handlers

---

**Legend:**
- **Added**: New features
- **Changed**: Changes in existing functionality
- **Deprecated**: Soon-to-be removed features
- **Removed**: Removed features
- **Fixed**: Bug fixes
- **Security**: Vulnerability fixes
