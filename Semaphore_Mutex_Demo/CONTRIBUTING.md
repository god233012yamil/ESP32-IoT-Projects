# Contributing to ESP-IDF FreeRTOS Synchronization Demo

Thank you for your interest in contributing to this project! This document provides guidelines for contributing.

## 🤝 How to Contribute

### Reporting Bugs

If you find a bug, please create an issue with the following information:

- **Description:** Clear description of the bug
- **Steps to Reproduce:** Detailed steps to reproduce the issue
- **Expected Behavior:** What you expected to happen
- **Actual Behavior:** What actually happened
- **Environment:**
  - ESP-IDF version
  - ESP32 variant (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, etc.)
  - Operating system
- **Logs:** Relevant serial monitor output or error messages

### Suggesting Enhancements

Enhancement suggestions are welcome! Please create an issue with:

- **Clear description** of the enhancement
- **Use case** or problem it solves
- **Proposed implementation** (if you have ideas)
- **Alternatives considered**

### Pull Requests

We welcome pull requests! Here's the process:

1. **Fork the repository**
2. **Create a feature branch:** `git checkout -b feature/your-feature-name`
3. **Make your changes:**
   - Follow the existing code style
   - Add comments for complex logic
   - Update documentation if needed
4. **Test your changes:**
   - Build successfully on at least one ESP32 variant
   - Verify the synchronization patterns still work correctly
5. **Commit your changes:**
   - Use clear, descriptive commit messages
   - Reference issue numbers if applicable
6. **Push to your fork:** `git push origin feature/your-feature-name`
7. **Create a Pull Request** with a clear description

## 📝 Code Style Guidelines

### C Code

- **Indentation:** 4 spaces (no tabs)
- **Braces:** K&R style (opening brace on same line)
- **Naming:**
  - Functions: `snake_case`
  - Variables: `snake_case`
  - Constants/Defines: `UPPER_CASE`
  - Static functions: prefix with `static`
- **Comments:**
  - Use `/** */` for function documentation
  - Use `//` for inline comments
  - Explain *why*, not just *what*

### Documentation

- Update README.md if adding new features
- Update FLOWCHART.md if changing program flow
- Keep documentation clear and concise
- Use proper Markdown formatting

## 🧪 Testing

Before submitting a PR, please verify:

- [ ] Code compiles without warnings
- [ ] Project builds successfully: `idf.py build`
- [ ] Flashes and runs on actual hardware
- [ ] All three synchronization patterns still work
- [ ] Serial output matches expected behavior
- [ ] No memory leaks or crashes

## 💡 Ideas for Contributions

Here are some areas where contributions would be valuable:

### New Features
- Additional synchronization primitive examples (event groups, queues)
- Real I2C device integration examples
- Performance measurement and benchmarking
- Additional RTOS patterns (producer-consumer, etc.)
- SPI bus mutex example
- Timer-based synchronization examples

### Documentation
- Video tutorial or demo
- Additional flowcharts for specific patterns
- Translation to other languages
- More detailed troubleshooting section
- Platform-specific setup guides

### Testing
- Automated testing framework
- Hardware-in-the-loop tests
- Edge case testing
- Performance profiling

### Code Quality
- Static analysis integration
- Code coverage reporting
- Continuous integration setup
- Consistent error handling patterns

## 📋 Project Structure

When adding new files, follow this structure:

```
Semaphore_Mutex_Demo/
├── main/
│   ├── CMakeLists.txt
│   └── main.c              # Main application code
├── examples/               # New: Example integrations
├── tests/                  # New: Test code
├── docs/                   # New: Additional documentation
├── CMakeLists.txt
├── sdkconfig.defaults
├── LICENSE
├── README.md
├── FLOWCHART.md
├── CONTRIBUTING.md
└── .gitignore
```

## 🔍 Review Process

All pull requests will be reviewed for:

1. **Functionality:** Does it work as intended?
2. **Code Quality:** Is it well-written and maintainable?
3. **Documentation:** Is it properly documented?
4. **Testing:** Has it been tested?
5. **Compatibility:** Does it work across ESP32 variants?

## 📜 License

By contributing, you agree that your contributions will be licensed under the MIT-0 License (same as the project).

## ❓ Questions?

If you have questions about contributing:

- Create an issue with the "question" label
- Check existing issues for similar questions
- Review the README.md and FLOWCHART.md

## 🙏 Thank You!

Your contributions help make this project better for everyone learning about FreeRTOS synchronization primitives. We appreciate your time and effort!
