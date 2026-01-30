# Contributing to ESP32-S3 Event Groups Demo

Thank you for your interest in contributing to this project! We welcome contributions from the community.

## How to Contribute

### Reporting Issues

If you find a bug or have a suggestion for improvement:

1. Check if the issue already exists in the [Issues](https://github.com/yourusername/esp32s3-event-groups-demo/issues) section
2. If not, create a new issue with:
   - A clear, descriptive title
   - Detailed description of the problem or suggestion
   - Steps to reproduce (for bugs)
   - Expected vs actual behavior
   - Environment details (ESP-IDF version, board type, OS)
   - Relevant logs or screenshots

### Submitting Pull Requests

1. **Fork the Repository**
   ```bash
   # Click "Fork" on GitHub, then clone your fork
   git clone https://github.com/YOUR_USERNAME/esp32s3-event-groups-demo.git
   cd esp32s3-event-groups-demo
   ```

2. **Create a Feature Branch**
   ```bash
   git checkout -b feature/your-feature-name
   # or
   git checkout -b fix/your-bug-fix
   ```

3. **Make Your Changes**
   - Write clear, readable code
   - Follow the existing code style
   - Add comments for complex logic
   - Update documentation if needed

4. **Test Your Changes**
   ```bash
   idf.py build
   idf.py flash monitor
   ```
   - Ensure the code compiles without errors
   - Test on actual hardware if possible
   - Verify existing functionality still works

5. **Commit Your Changes**
   ```bash
   git add .
   git commit -m "Add feature: brief description"
   ```
   
   **Commit Message Guidelines**:
   - Use present tense ("Add feature" not "Added feature")
   - Be concise but descriptive
   - Reference issue numbers if applicable (#123)

6. **Push and Create Pull Request**
   ```bash
   git push origin feature/your-feature-name
   ```
   - Go to GitHub and create a Pull Request
   - Provide a clear description of your changes
   - Reference any related issues

## Code Style Guidelines

### C Code Style

Follow ESP-IDF coding conventions:

```c
// Use snake_case for functions and variables
static void my_function_name(void)
{
    int my_variable = 0;
}

// Use ALL_CAPS for macros and constants
#define MY_CONSTANT 42
#define EVT_MY_BIT (1U << 0)

// Add descriptive comments
/**
 * @brief Brief description of function
 *
 * Detailed description if needed
 *
 * @param param1 Description of parameter
 * @return Return value description
 */
static int my_documented_function(int param1)
{
    // Implementation
    return 0;
}

// Use proper spacing
if (condition) {
    // Code here
} else {
    // Code here
}

// Align related definitions
#define SHORT_NAME    10
#define LONGER_NAME   20
#define LONGEST_NAME  30
```

### Documentation

- Update README.md for new features or significant changes
- Add inline comments for complex algorithms
- Document any new event bits or configuration options
- Update flowcharts if task flow changes

## What We're Looking For

### Feature Ideas

- Real sensor integrations (I2C, SPI devices)
- Additional synchronization patterns
- Error handling improvements
- Performance optimizations
- Additional example use cases

### Bug Fixes

- Compilation errors
- Runtime crashes or hangs
- Memory leaks
- Logic errors
- Documentation errors

### Documentation Improvements

- Clarifying existing documentation
- Adding examples
- Fixing typos
- Improving flowcharts
- Adding troubleshooting tips

## Testing Guidelines

Before submitting a pull request:

1. **Compilation Test**
   ```bash
   idf.py build
   ```
   - Must compile without errors
   - Warnings should be addressed

2. **Flash Test**
   ```bash
   idf.py flash monitor
   ```
   - Test on actual ESP32-S3 hardware
   - Verify all tasks run correctly
   - Check for runtime errors in logs

3. **Regression Test**
   - Ensure existing features still work
   - Test startup sequence
   - Verify all event synchronization patterns

## Review Process

1. Maintainers will review your pull request
2. Feedback may be provided for improvements
3. Once approved, your PR will be merged
4. You'll be credited as a contributor!

## Code of Conduct

### Our Standards

- Be respectful and inclusive
- Welcome newcomers
- Provide constructive feedback
- Focus on the code, not the person
- Accept constructive criticism gracefully

### Unacceptable Behavior

- Harassment or discrimination
- Trolling or insulting comments
- Personal attacks
- Publishing others' private information

## Getting Help

If you need help:

- Check the [README.md](README.md) for basic information
- Look at existing code for examples
- Ask questions in Issues (use "question" label)
- Reference ESP-IDF documentation

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

## Recognition

Contributors will be recognized in:
- GitHub contributors list
- Release notes (for significant contributions)

## Questions?

Feel free to open an issue with the "question" label if you have any questions about contributing.

Thank you for helping make this project better! 🎉
