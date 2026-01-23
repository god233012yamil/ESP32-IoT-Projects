# Contributing to ESP32 Low-Power Reference

Thank you for your interest in contributing to this project! We welcome contributions that help make low-power ESP32 development more accessible.

## How to Contribute

### Reporting Issues

If you find a bug or have a suggestion:

1. Check if the issue already exists in [Issues](https://github.com/yourusername/esp32-low-power-reference/issues)
2. If not, create a new issue with:
   - Clear description of the problem
   - Steps to reproduce (if applicable)
   - ESP32 variant and ESP-IDF version
   - Expected vs actual behavior
   - Power consumption measurements (if relevant)

### Submitting Pull Requests

1. **Fork the repository**
2. **Create a feature branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

3. **Make your changes**
   - Follow the existing code style
   - Add comments for complex logic
   - Test on actual hardware if possible

4. **Commit with clear messages**
   ```bash
   git commit -m "Add MQTT integration example"
   ```

5. **Push and create a PR**
   ```bash
   git push origin feature/your-feature-name
   ```

## Contribution Ideas

We're particularly interested in:

### Examples and Integrations
- MQTT, HTTP, CoAP protocol examples
- LoRaWAN integration
- BLE beacon mode
- Cloud platform integrations (AWS IoT, Azure IoT, Google Cloud)
- Sensor libraries (BME280, SHT31, etc.)

### Power Optimization
- ULP coprocessor examples
- Touch wake examples
- UART wake examples
- RTC GPIO optimization tricks
- Battery monitoring and fuel gauge

### Documentation
- Real-world power measurements
- Case studies from production devices
- Translation to other languages
- Video tutorials
- Troubleshooting guides

### Testing
- Automated power consumption tests
- CI/CD pipeline improvements
- Hardware-in-the-loop testing

## Code Style

- Follow [ESP-IDF coding style](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html)
- Use descriptive variable and function names
- Add Doxygen-style comments for public APIs
- Keep functions small and focused

## Testing Your Changes

Before submitting:

1. Build successfully for multiple targets
   ```bash
   idf.py set-target esp32
   idf.py build
   
   idf.py set-target esp32s3
   idf.py build
   ```

2. Test on real hardware if adding power features

3. Verify menuconfig options work correctly

4. Update documentation if adding features

## Power Measurement Standards

When contributing power optimization features:

1. **Include measurements** from production-like hardware, not dev boards
2. **Document test conditions**:
   - Supply voltage
   - Temperature
   - Measurement equipment used
   - GPIO states
   - External components
3. **Provide before/after** comparisons
4. **Specify ESP32 variant** (ESP32, S2, S3, C3, etc.)

## Documentation Standards

When updating documentation:

- Use clear, concise language
- Include code examples
- Add diagrams where helpful (Mermaid preferred)
- Link to official ESP-IDF documentation
- Specify which ESP-IDF versions are supported

## Questions?

Feel free to:
- Open a [Discussion](https://github.com/yourusername/esp32-low-power-reference/discussions)
- Ask in the [ESP32 Forum](https://www.esp32.com/)
- Contact the maintainers

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

Thank you for helping make ESP32 low-power development better for everyone!
