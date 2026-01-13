# Contributing to ESP32-S3 Custom eFuse Demo

Thank you for your interest in contributing to this project! This document provides guidelines and instructions for contributing.

## How to Contribute

### Reporting Issues

If you find a bug or have a suggestion for improvement:

1. **Check existing issues** to avoid duplicates
2. **Open a new issue** with a clear title and description
3. Include:
   - ESP-IDF version
   - Hardware board details
   - Steps to reproduce (for bugs)
   - Expected vs actual behavior
   - Relevant logs or error messages

### Submitting Pull Requests

1. **Fork the repository** to your GitHub account
2. **Create a feature branch** from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```
3. **Make your changes** following the coding guidelines below
4. **Test thoroughly** with both virtual and real eFuses (if applicable)
5. **Commit with clear messages**:
   ```bash
   git commit -m "Add feature: brief description"
   ```
6. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```
7. **Open a Pull Request** with:
   - Clear description of changes
   - Reference to related issues
   - Screenshots/logs if applicable
   - Confirmation of testing

## Coding Guidelines

### C Code Style

- Follow ESP-IDF coding conventions
- Use clear, descriptive variable and function names
- Add comments for complex logic
- Keep functions focused and modular
- Use proper error handling with `esp_err_t`

Example:
```c
/**
 * @brief Brief description of function
 *
 * Detailed description of what the function does.
 *
 * @param param1 Description of first parameter
 * @param param2 Description of second parameter
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t my_function(uint8_t param1, uint32_t param2)
{
    // Implementation
    return ESP_OK;
}
```

### CSV Table Formatting

- Use consistent spacing in CSV files
- Add clear comments for each field
- Follow bit alignment requirements
- Document any special encoding

### Documentation

- Update README.md for user-facing changes
- Update FLOWCHARTS.md if logic flow changes
- Add inline comments for complex code
- Keep documentation clear and concise

## Testing Requirements

### Before Submitting

1. **Virtual eFuse Testing**:
   ```bash
   idf.py build flash monitor
   ```
   Verify with `CONFIG_EFUSE_VIRTUAL=y`

2. **Build System Testing**:
   ```bash
   idf.py fullclean
   idf.py build
   ```
   Ensure clean builds work

3. **Configuration Testing**:
   - Test with programming enabled/disabled
   - Test with different target chips (if applicable)

4. **Code Quality**:
   - No compiler warnings
   - No memory leaks (use ESP-IDF tools)
   - Proper error handling

### Hardware Testing (Optional)

If you have access to development hardware:
- Test with real eFuses (carefully!)
- Verify on multiple board revisions
- Document any hardware-specific findings

## Commit Message Guidelines

Use clear, imperative commit messages:

- ✅ Good: "Add CRC-32 validation option"
- ✅ Good: "Fix batch write error handling"
- ✅ Good: "Update documentation for ESP-IDF v5.2"
- ❌ Bad: "Fixed stuff"
- ❌ Bad: "WIP"
- ❌ Bad: "asdfasdf"

### Commit Message Format

```
<type>: <short summary>

<optional detailed description>

<optional footer with issue references>
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `refactor`: Code refactoring
- `test`: Test additions or changes
- `chore`: Build system or tooling changes

Example:
```
feat: Add support for custom CRC algorithms

Implements configurable CRC algorithms through Kconfig menu.
Users can now choose between CRC-16, CRC-32, and CRC-8.

Closes #42
```

## Code Review Process

1. Maintainers will review your PR within a few days
2. Address any requested changes
3. Keep the conversation respectful and constructive
4. Once approved, your PR will be merged

## Feature Requests

We welcome feature requests! Please:

1. Search existing issues first
2. Open a new issue with the "enhancement" label
3. Clearly describe:
   - The problem you're trying to solve
   - Your proposed solution
   - Any alternatives you've considered
   - Example use cases

## Questions?

If you have questions about contributing:
- Open a GitHub Discussion
- Tag maintainers in issues
- Check the main README.md first

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

Thank you for helping improve this project! 🚀
