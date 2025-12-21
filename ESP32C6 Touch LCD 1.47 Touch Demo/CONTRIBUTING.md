# Contributing to ESP32-C6 Touch Demo

Thank you for your interest in contributing! This document provides guidelines for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [How to Contribute](#how-to-contribute)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Pull Request Process](#pull-request-process)
- [Reporting Bugs](#reporting-bugs)
- [Suggesting Enhancements](#suggesting-enhancements)

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inclusive environment for all contributors.

### Expected Behavior

- Be respectful and considerate
- Accept constructive criticism gracefully
- Focus on what is best for the community
- Show empathy towards others

### Unacceptable Behavior

- Harassment or discriminatory language
- Trolling or insulting comments
- Public or private harassment
- Publishing others' private information

## Getting Started

### Prerequisites

- ESP-IDF v5.1.0 or later
- Git
- Python 3.8+
- Hardware: WaveShare ESP32-C6-Touch-LCD-1.47

### Fork and Clone

```bash
# Fork the repository on GitHub
# Then clone your fork
git clone https://github.com/YOUR_USERNAME/esp32c6-touch-demo.git
cd esp32c6-touch-demo

# Add upstream remote
git remote add upstream https://github.com/ORIGINAL_OWNER/esp32c6-touch-demo.git
```

## How to Contribute

### Types of Contributions

We welcome various types of contributions:

- **Bug fixes** - Fix issues in existing code
- **New features** - Add new functionality
- **Documentation** - Improve or add documentation
- **Examples** - Add usage examples
- **Tests** - Add or improve tests
- **Performance** - Optimize existing code

### Before You Start

1. Check existing issues and PRs
2. Open an issue to discuss major changes
3. Fork the repository
4. Create a feature branch

## Development Setup

### Environment Setup

```bash
# Install ESP-IDF
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c6
. ./export.sh

# Clone and setup project
git clone https://github.com/YOUR_USERNAME/esp32c6-touch-demo.git
cd esp32c6-touch-demo
idf.py set-target esp32c6
```

### Build and Test

```bash
# Clean build
idf.py fullclean
idf.py build

# Flash and monitor
idf.py flash monitor
```

## Coding Standards

### Style Guidelines

Follow ESP-IDF coding style:

#### Naming Conventions

```c
// Functions: snake_case
void initialize_display(void);

// Variables: snake_case
int touch_x_coord = 0;

// Constants: UPPER_SNAKE_CASE
#define MAX_TOUCH_POINTS 5

// Macros: UPPER_SNAKE_CASE
#define PIN_TOUCH_SDA 18

// Enums: snake_case with prefix
typedef enum {
    touch_state_idle,
    touch_state_pressed,
    touch_state_released
} touch_state_t;
```

#### Code Formatting

```c
// Indentation: 4 spaces (no tabs)
void example_function(void)
{
    if (condition) {
        do_something();
    } else {
        do_something_else();
    }
}

// Braces: K&R style
if (condition) {
    statement;
}

// Line length: 120 characters max
```

#### Comments

```c
// Single line comment for brief explanations

/*
 * Multi-line comment for detailed
 * explanations and documentation
 */

/**
 * @brief Function brief description
 * 
 * Detailed description of what the function does
 * 
 * @param param1 Description of param1
 * @param param2 Description of param2
 * @return Description of return value
 */
int example_function(int param1, int param2);
```

### File Organization

```c
// 1. License header
// 2. Includes (system headers first)
#include <stdio.h>
#include <string.h>

// 3. ESP-IDF includes
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

// 4. Local includes
#include "esp_lcd_jd9853.h"

// 5. Defines
#define TAG "MAIN"
#define PIN_SDA 18

// 6. Type definitions
typedef struct {
    int x;
    int y;
} point_t;

// 7. Static variables
static int counter = 0;

// 8. Function declarations
static void helper_function(void);

// 9. Function implementations
```

### Best Practices

#### Error Handling

```c
// Always check return values
esp_err_t ret = initialize_component();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize: %s", esp_err_to_name(ret));
    return ret;
}

// Use ESP_ERROR_CHECK for critical operations
ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
```

#### Logging

```c
// Use appropriate log levels
ESP_LOGE(TAG, "Error message");    // Errors
ESP_LOGW(TAG, "Warning message");  // Warnings
ESP_LOGI(TAG, "Info message");     // Information
ESP_LOGD(TAG, "Debug message");    // Debug (disabled in release)
ESP_LOGV(TAG, "Verbose message");  // Verbose (disabled in release)
```

#### Memory Management

```c
// Always free allocated memory
uint8_t *buffer = malloc(size);
if (buffer == NULL) {
    ESP_LOGE(TAG, "Memory allocation failed");
    return ESP_ERR_NO_MEM;
}

// Use buffer...

free(buffer);
buffer = NULL;
```

#### Resource Cleanup

```c
// Clean up in reverse order of initialization
void cleanup(void) {
    if (touch_handle) {
        esp_lcd_touch_del(touch_handle);
    }
    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
    }
}
```

## Testing Guidelines

### Hardware Testing

Before submitting:

- [ ] Test on actual hardware
- [ ] Verify touch response
- [ ] Check display output
- [ ] Monitor serial output
- [ ] Test edge cases
- [ ] Verify no memory leaks

### Test Checklist

```
□ Clean build succeeds
□ No compiler warnings
□ Touch responds correctly
□ Display updates properly
□ Coordinates are accurate
□ No crashes or hangs
□ Serial output is clean
□ Memory usage is acceptable
□ Performance is good
□ Corner cases handled
```

### Performance Testing

```c
// Measure execution time
uint32_t start = esp_timer_get_time();
perform_operation();
uint32_t elapsed = esp_timer_get_time() - start;
ESP_LOGI(TAG, "Operation took %lu us", elapsed);

// Monitor memory
ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
```

## Pull Request Process

### Creating a Pull Request

1. **Update your fork**
   ```bash
   git checkout main
   git fetch upstream
   git merge upstream/main
   ```

2. **Create a feature branch**
   ```bash
   git checkout -b feature/my-new-feature
   ```

3. **Make your changes**
   - Follow coding standards
   - Write meaningful commit messages
   - Keep commits logical and atomic

4. **Test thoroughly**
   - Build and test on hardware
   - Check for regressions
   - Verify all functionality

5. **Update documentation**
   - Update README if needed
   - Add comments to code
   - Update CHANGES.md

6. **Commit and push**
   ```bash
   git add .
   git commit -m "feat: add new feature"
   git push origin feature/my-new-feature
   ```

7. **Open Pull Request**
   - Clear title and description
   - Reference related issues
   - Add screenshots if applicable

### Commit Message Format

Use conventional commits:

```
type(scope): subject

body

footer
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Formatting changes
- `refactor`: Code restructuring
- `perf`: Performance improvements
- `test`: Adding tests
- `chore`: Maintenance tasks

Examples:
```
feat(touch): add multi-touch support

Add support for detecting up to 5 simultaneous touch points.
This allows for gesture recognition in future updates.

Closes #123
```

```
fix(display): correct landscape mode gap offset

The gap offset was incorrect in landscape mode, causing
misaligned display output. Changed from (34,0) to (0,34).

Fixes #456
```

### Review Process

1. Maintainer reviews code
2. Automated tests run (if configured)
3. Address review comments
4. Make requested changes
5. Push updates to same branch
6. Maintainer approves and merges

### PR Checklist

Before submitting:

- [ ] Code follows style guidelines
- [ ] Self-review completed
- [ ] Comments added where needed
- [ ] Documentation updated
- [ ] No new warnings
- [ ] Tested on hardware
- [ ] Commit messages are clear
- [ ] Branch is up-to-date

## Reporting Bugs

### Before Reporting

- Check existing issues
- Verify it's reproducible
- Test on latest version
- Gather all information

### Bug Report Template

```markdown
**Describe the bug**
Clear description of the issue

**To Reproduce**
Steps to reproduce:
1. Go to '...'
2. Click on '...'
3. See error

**Expected behavior**
What you expected to happen

**Actual behavior**
What actually happened

**Environment:**
- ESP-IDF version: [e.g. v5.1.2]
- Hardware: [e.g. ESP32-C6-Touch-LCD-1.47]
- OS: [e.g. Ubuntu 22.04]

**Serial output:**
```
Paste relevant serial output here
```

**Additional context**
Any other relevant information
```

## Suggesting Enhancements

### Enhancement Template

```markdown
**Is your feature request related to a problem?**
Clear description of the problem

**Describe the solution you'd like**
Clear description of desired feature

**Describe alternatives considered**
Other solutions you've considered

**Additional context**
Any other relevant information

**Implementation ideas**
If you have ideas on how to implement
```

## Questions?

- Open an issue for general questions
- Use discussions for broader topics
- Check documentation first
- Search existing issues

## Recognition

Contributors will be:
- Listed in CONTRIBUTORS.md
- Credited in release notes
- Thanked in the community

Thank you for contributing! 🎉
