# Contributing to ESP-NOW Beginner Demo

Thank you for your interest in contributing to this project! This document provides guidelines and instructions for contributing.

## How to Contribute

### Reporting Bugs

If you find a bug, please open an issue with:

1. **Clear title**: Briefly describe the issue
2. **Description**: Detailed explanation of the problem
3. **Steps to reproduce**: List the exact steps to recreate the bug
4. **Expected behavior**: What should happen
5. **Actual behavior**: What actually happens
6. **Environment**:
   - ESP-IDF version
   - Hardware model (ESP32-S3, etc.)
   - Operating system
7. **Logs**: Include relevant log output
8. **Screenshots**: If applicable

### Suggesting Enhancements

For feature requests or enhancements:

1. **Check existing issues**: Make sure it hasn't been suggested already
2. **Clear use case**: Explain why this feature would be useful
3. **Proposed solution**: If you have ideas on implementation
4. **Alternatives considered**: Any other approaches you've thought about

### Pull Requests

We welcome pull requests! Here's the process:

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/your-feature-name`
3. **Make your changes**:
   - Follow the existing code style
   - Add comments for complex logic
   - Update documentation if needed
4. **Test thoroughly**: Test on actual hardware
5. **Commit your changes**: Use clear, descriptive commit messages
6. **Push to your fork**: `git push origin feature/your-feature-name`
7. **Submit a pull request**: Include a clear description of changes

### Code Style Guidelines

#### C Code

- Follow the existing formatting style
- Use 4 spaces for indentation (no tabs)
- Keep lines under 100 characters when possible
- Use descriptive variable names
- Add comments for non-obvious code
- Use `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` for logging

Example:
```c
/**
 * @brief Brief description of function
 *
 * Detailed description if needed
 *
 * @param param_name Description of parameter
 * @return Description of return value
 */
static esp_err_t my_function(uint8_t param_name)
{
    // Implementation
    return ESP_OK;
}
```

#### Documentation

- Use Markdown for documentation files
- Keep README.md up to date
- Add code examples where helpful
- Use clear, beginner-friendly language

### Testing

Before submitting a pull request:

1. **Build successfully**: `idf.py build` should complete without errors
2. **Test on hardware**: Verify functionality on actual ESP32-S3 board
3. **Test both roles**: If applicable, test as both sender and receiver
4. **Check for regressions**: Ensure existing functionality still works
5. **Review logs**: Make sure there are no unexpected errors or warnings

### Commit Message Guidelines

Write clear commit messages:

- Use present tense: "Add feature" not "Added feature"
- Use imperative mood: "Fix bug" not "Fixes bug"
- Keep first line under 50 characters
- Add detailed description if needed in body

Good examples:
```
Add encryption support to ESP-NOW communication

Implement AES-128 encryption for secure peer-to-peer communication.
Update documentation with encryption usage examples.
```

```
Fix packet loss issue on high-frequency sending

Increase queue size and adjust task priorities to handle
burst traffic more reliably.
```

### What to Contribute

We welcome contributions in these areas:

#### Code Enhancements
- New features (encryption, multi-peer support, etc.)
- Performance optimizations
- Bug fixes
- Code refactoring for better maintainability
- Additional error handling

#### Documentation
- README improvements
- Code comments
- Usage examples
- Tutorial additions
- Translation to other languages

#### Examples
- New use cases
- Integration examples
- Best practices demonstrations

#### Testing
- Additional test scenarios
- Validation scripts
- CI/CD improvements

### Areas Needing Help

Current priorities:

1. **Power consumption optimization**: Examples for low-power modes
2. **Encryption examples**: Working implementation of encrypted communication
3. **Multi-device scenarios**: Examples with 3+ devices
4. **Error recovery**: Robust error handling and retry mechanisms
5. **Performance benchmarks**: Latency and throughput measurements
6. **Platform support**: Testing on ESP32, ESP32-C3, ESP32-C6

### Code Review Process

1. **Automated checks**: Must pass any CI/CD checks
2. **Maintainer review**: A maintainer will review your PR
3. **Discussion**: Address any feedback or questions
4. **Approval**: Once approved, your PR will be merged
5. **Credit**: You'll be added to contributors list

### Questions?

If you have questions:

1. Check existing issues and discussions
2. Review the README and documentation
3. Open a new issue with the "question" label

### Code of Conduct

Be respectful and considerate:

- Be welcoming to newcomers
- Be patient with questions
- Provide constructive feedback
- Focus on what is best for the community
- Show empathy towards other community members

### License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

Thank you for contributing! Every contribution, no matter how small, is valued and appreciated. 🙏
