# Contributing to ESP32 UART Reference

Thank you for your interest in contributing to this project! This document provides guidelines and instructions for contributing.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How to Contribute](#how-to-contribute)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Testing Requirements](#testing-requirements)
- [Pull Request Process](#pull-request-process)
- [Reporting Issues](#reporting-issues)

## Code of Conduct

This project adheres to a code of conduct that all contributors are expected to follow:

- Be respectful and inclusive
- Focus on constructive feedback
- Accept differing viewpoints gracefully
- Prioritize community benefit over personal gain

## How to Contribute

### Types of Contributions

We welcome various types of contributions:

1. **Bug fixes**: Fix issues in the codebase
2. **Feature additions**: New capabilities or enhancements
3. **Documentation**: Improve README, comments, or examples
4. **Testing**: Add test cases or improve coverage
5. **Performance**: Optimize existing code
6. **Examples**: Additional use cases or demonstrations

### Before You Start

1. Check existing [issues](../../issues) and [pull requests](../../pulls)
2. Open an issue to discuss major changes before implementing
3. Ensure your contribution aligns with project goals

## Development Setup

### Prerequisites

```bash
# Install ESP-IDF (if not already installed)
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32,esp32s2,esp32s3,esp32c3
. ./export.sh
```

### Fork and Clone

```bash
# Fork the repository on GitHub, then:
git clone https://github.com/YOUR_USERNAME/esp32-uart-reference.git
cd esp32-uart-reference

# Add upstream remote
git remote add upstream https://github.com/ORIGINAL_OWNER/esp32-uart-reference.git
```

### Create a Branch

```bash
git checkout -b feature/your-feature-name
# or
git checkout -b fix/issue-number-description
```

### Build and Test

```bash
# Set target chip
idf.py set-target esp32s3

# Build
idf.py build

# Flash and monitor (with hardware connected)
idf.py -p /dev/ttyUSB0 flash monitor
```

## Coding Standards

### General Principles

- **Readability**: Code should be self-documenting where possible
- **Simplicity**: Prefer simple solutions over complex ones
- **Consistency**: Follow existing code style
- **Modularity**: Keep functions focused and small
- **Safety**: Handle errors explicitly, avoid assumptions

### C Style Guide

#### Naming Conventions

```c
// Functions: snake_case
static void uart_rx_event_task(void *arg);

// Types: snake_case_t suffix
typedef struct {
    size_t len;
    uint8_t data[256];
} uart_tx_msg_t;

// Constants/Macros: UPPER_CASE
#define UART_PORT              UART_NUM_1
#define UART_RX_BUF_SIZE       4096

// Static variables: snake_case with s_ prefix (optional)
static QueueHandle_t uart_evt_queue = NULL;
```

#### Formatting

```c
// Indentation: 4 spaces (no tabs)
void function_name(int param)
{
    if (condition) {
        // code here
    }
}

// Braces: K&R style for functions, same line for control structures
if (condition) {
    // code
} else {
    // code
}

// Line length: 100 characters maximum (prefer 80)

// Pointer declaration: asterisk with type
uint8_t *buffer;
```

#### Comments

```c
/**
 * @brief Brief function description.
 *
 * Detailed description if needed. Explain the why, not just the what.
 *
 * @param[in] input Input parameter description
 * @param[out] output Output parameter description
 * @param[in,out] inout Input/output parameter description
 * @return Return value description
 */
int example_function(int input, int *output);

// Single-line comments for brief explanations
int count = 0;  // Initialize counter

// Multi-line comments for complex logic
/*
 * This block handles the special case where...
 * Additional explanation...
 */
```

### Error Handling

```c
// Use ESP_ERROR_CHECK for initialization
ESP_ERROR_CHECK(uart_driver_install(...));

// Return error codes for runtime errors
esp_err_t result = operation();
if (result != ESP_OK) {
    ESP_LOGE(TAG, "Operation failed: %s", esp_err_to_name(result));
    return result;
}

// Check NULL pointers
if (buffer == NULL) {
    ESP_LOGE(TAG, "Buffer allocation failed");
    return ESP_ERR_NO_MEM;
}
```

### Logging

```c
static const char *TAG = "component_name";

// Use appropriate log levels
ESP_LOGE(TAG, "Error: %s", error_msg);      // Errors
ESP_LOGW(TAG, "Warning: %s", warn_msg);     // Warnings
ESP_LOGI(TAG, "Info: %s", info_msg);        // Important info
ESP_LOGD(TAG, "Debug: %s", debug_msg);      // Debug details
ESP_LOGV(TAG, "Verbose: %s", verbose_msg);  // Verbose details
```

## Testing Requirements

### Manual Testing

All contributions should be manually tested on real hardware:

1. **Build Test**
   ```bash
   idf.py fullclean
   idf.py build
   ```

2. **Flash Test**
   ```bash
   idf.py -p /dev/ttyUSB0 flash
   ```

3. **Functional Test**
   - Test all affected commands/features
   - Verify no regressions in existing functionality
   - Test edge cases (buffer overflows, invalid input, etc.)

### Test Checklist

- [ ] Code compiles without warnings
- [ ] Functionality works as expected
- [ ] No memory leaks (check with heap tracing if modifying allocation)
- [ ] No task watchdog timeouts
- [ ] Logging is appropriate (no excessive spam)
- [ ] Documentation updated if needed

### Test Report Template

Include in your PR description:

```markdown
## Test Environment
- **Chip**: ESP32-S3
- **ESP-IDF Version**: v5.1.2
- **Development Board**: ESP32-S3-DevKitC-1

## Test Cases
1. **Basic Functionality**
   - [ ] PING command works
   - [ ] VERSION command works
   - [ ] UPTIME command works

2. **Stress Testing**
   - [ ] 1000 rapid PING commands: PASS
   - [ ] Buffer overflow handling: PASS

3. **New Feature** (if applicable)
   - [ ] Feature X works correctly
   - [ ] Edge cases handled
```

## Pull Request Process

### Before Submitting

1. **Update your branch**
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Run final checks**
   ```bash
   idf.py fullclean
   idf.py build
   # Test on hardware
   ```

3. **Update documentation**
   - Update README.md if adding features
   - Add/update comments in code
   - Update ARCHITECTURE.md if changing flow

### PR Submission

1. **Push your branch**
   ```bash
   git push origin feature/your-feature-name
   ```

2. **Create Pull Request** on GitHub with:
   - Clear title (e.g., "Add binary protocol support")
   - Description of changes
   - Test results
   - Screenshots/logs if applicable
   - Reference related issues (#123)

3. **PR Template**
   ```markdown
   ## Description
   Brief description of changes

   ## Motivation
   Why is this change needed?

   ## Changes Made
   - Added feature X
   - Fixed bug Y
   - Updated documentation

   ## Testing
   - Tested on ESP32-S3
   - All existing tests pass
   - New tests added for feature X

   ## Checklist
   - [ ] Code follows style guidelines
   - [ ] Documentation updated
   - [ ] Tests pass on hardware
   - [ ] No new compiler warnings
   ```

### Review Process

1. Maintainer will review within 1-2 weeks
2. Address feedback with additional commits
3. Squash commits if requested
4. Once approved, maintainer will merge

## Reporting Issues

### Before Opening an Issue

1. Search existing issues
2. Check documentation and examples
3. Verify your ESP-IDF version is supported

### Issue Template

```markdown
## Description
Clear description of the issue

## Steps to Reproduce
1. Flash the firmware
2. Send command X
3. Observe behavior Y

## Expected Behavior
What should happen

## Actual Behavior
What actually happens

## Environment
- **Chip**: ESP32-S3
- **ESP-IDF Version**: v5.1.2
- **Development Board**: ESP32-S3-DevKitC-1
- **Baud Rate**: 115200

## Logs
```
Paste relevant logs here
```

## Additional Context
Any other relevant information
```

## Questions and Support

- **General Questions**: [GitHub Discussions](../../discussions)
- **Bug Reports**: [GitHub Issues](../../issues)
- **ESP-IDF Help**: [ESP32 Forum](https://esp32.com)

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.

---

Thank you for contributing! 🎉
