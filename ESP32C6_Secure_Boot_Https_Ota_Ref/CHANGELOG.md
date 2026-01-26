# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-01-26

### Added
- Initial release of ESP32-C6 Secure Boot + HTTPS OTA reference project
- Secure Boot v2 implementation with RSA-PSS signing
- Flash Encryption support (development and production modes)
- HTTPS OTA with certificate pinning
- Multi-layered OTA gating mechanism:
  - GPIO button trigger
  - Cloud command trigger (optional)
  - Maintenance window (SNTP-based)
  - Battery level check (stub implementation)
  - Network readiness validation
- Wi-Fi station connectivity with menuconfig credentials
- Dual OTA partition scheme with automatic failover
- Anti-rollback protection
- Production provisioning scripts
- Factory deployment checklist
- Comprehensive documentation and flowcharts

### Security Features
- Certificate pinning for HTTPS connections
- Bootloader and application signature verification
- Encrypted flash storage capability
- Secure version anti-rollback
- Hardware root of trust via eFuses

### Documentation
- Complete README with quick start guide
- Detailed system flowcharts (Mermaid diagrams)
- Security policy and best practices
- Contributing guidelines
- Production deployment procedures
- Troubleshooting guide

### Configuration Options
- Menuconfig-based configuration system
- Wi-Fi credentials configuration
- OTA URL and trigger settings
- Maintenance window configuration
- Battery threshold settings
- GPIO button configuration

## [Unreleased]

### Planned
- Example firmware update server implementation
- Additional cloud service integration examples
- Enhanced logging and diagnostics
- Power consumption optimization
- Example ADC battery reading implementation
- Device provisioning automation tools

---

## Version History

**[1.0.0]** - 2026-01-26
- Initial public release

---

For detailed information about each release, see the [GitHub Releases](https://github.com/yourusername/ESP32C6_Secure_Boot_Https_Ota_Ref/releases) page.
