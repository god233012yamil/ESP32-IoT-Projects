# Contributing to ESP32 TWDT Demo

Thank you for your interest in contributing to this project! This document provides guidelines for contributing.

## How to Contribute

### Reporting Bugs

If you find a bug, please open an issue with:
- A clear, descriptive title
- Steps to reproduce the issue
- Expected behavior vs actual behavior
- Your environment (ESP-IDF version, board type, etc.)
- Serial output or logs if applicable

### Suggesting Enhancements

Enhancement suggestions are welcome! Please open an issue with:
- A clear description of the enhancement
- Why this enhancement would be useful
- Possible implementation approach (optional)

### Pull Requests

1. **Fork the repository** and create your branch from `main`
2. **Make your changes** with clear, descriptive commits
3. **Test thoroughly** on actual hardware
4. **Update documentation** if you change functionality
5. **Submit a pull request** with a clear description of changes

#### Pull Request Guidelines

- Follow the existing code style and formatting
- Include comments for complex logic
- Test on ESP32 hardware before submitting
- Update README.md if adding features
- Keep commits atomic and well-described

### Code Style

This project follows standard ESP-IDF coding conventions:
- Use 4 spaces for indentation (no tabs)
- Function and variable names use snake_case
- Constants use UPPER_SNAKE_CASE
- Add Doxygen-style comments for functions
- Keep lines under 100 characters where practical

### Testing

Before submitting:
- Build with `idf.py build` (no warnings)
- Flash and test on actual ESP32 hardware
- Verify all demo scenarios work as expected
- Check serial output for errors

## Development Setup

1. Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/esp32-twdt-demo.git`
3. Create a branch: `git checkout -b feature/your-feature-name`
4. Make changes and test
5. Commit: `git commit -m "Add: your feature description"`
6. Push: `git push origin feature/your-feature-name`
7. Open a pull request

## Questions?

Feel free to open an issue for questions or discussions about the project.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
