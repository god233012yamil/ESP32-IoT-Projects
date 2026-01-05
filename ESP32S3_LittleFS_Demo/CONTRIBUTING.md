# Contributing to ESP32-S3 LittleFS Demo

Thank you for your interest in contributing to this project! This document provides guidelines for contributing.

## How to Contribute

### Reporting Issues

If you find a bug or have a suggestion for improvement:

1. Check if the issue already exists in the [Issues](https://github.com/yourusername/ESP32S3_LittleFS_Demo/issues) section
2. If not, create a new issue with:
   - A clear, descriptive title
   - Detailed description of the problem or suggestion
   - Steps to reproduce (for bugs)
   - Expected vs actual behavior
   - ESP-IDF version and hardware details
   - Relevant logs or error messages

### Submitting Pull Requests

1. Fork the repository
2. Create a new branch for your feature or fix:
   ```bash
   git checkout -b feature/your-feature-name
   ```
3. Make your changes following the coding standards below
4. Test your changes thoroughly on ESP32-S3 hardware
5. Commit your changes with clear, descriptive messages
6. Push to your fork and submit a pull request

### Coding Standards

- Follow the existing code style and formatting
- Use meaningful variable and function names
- Add comments for complex logic
- Include error handling for all operations
- Update documentation if adding new features
- Keep commits atomic and well-described

### Testing

Before submitting a pull request:

- Build the project without warnings: `idf.py build`
- Test on actual ESP32-S3 hardware
- Verify all file operations work correctly
- Check for memory leaks if applicable
- Ensure filesystem operations don't corrupt data

### Documentation

- Update README.md if adding new features
- Update FLOWCHART.md if changing program flow
- Add inline comments for complex code sections
- Document any new configuration options

## Development Setup

1. Install ESP-IDF v5.0 or later
2. Clone your fork:
   ```bash
   git clone https://github.com/yourusername/ESP32S3_LittleFS_Demo.git
   ```
3. Set up ESP-IDF environment:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```
4. Build and test:
   ```bash
   cd ESP32S3_LittleFS_Demo
   idf.py build
   idf.py -p PORT flash monitor
   ```

## Feature Requests

We welcome feature requests! When suggesting new features:

- Explain the use case and benefits
- Provide examples of how it would work
- Consider backward compatibility
- Discuss implementation approach if possible

## Code of Conduct

- Be respectful and constructive
- Welcome newcomers and help them learn
- Focus on what is best for the community
- Show empathy towards other contributors

## Questions?

If you have questions about contributing, feel free to:

- Open an issue with the "question" label
- Reach out to the maintainers
- Check existing discussions and documentation

Thank you for contributing! 🚀
