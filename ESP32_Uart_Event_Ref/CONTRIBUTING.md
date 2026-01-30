# Contributing to ESP32 UART Event Reference

First off, thank you for considering contributing to this project! It's people like you that make this project a great learning resource for the ESP32 community.

## How Can I Contribute?

### Reporting Bugs

Before creating bug reports, please check the existing issues to avoid duplicates. When you create a bug report, include as many details as possible:

- **Use a clear and descriptive title**
- **Describe the exact steps to reproduce the problem**
- **Provide specific examples** (code snippets, terminal output, etc.)
- **Describe the behavior you observed** and what you expected
- **Include details about your environment**:
  - ESP-IDF version
  - ESP32 board/module type
  - Operating system
  - Build configuration

### Suggesting Enhancements

Enhancement suggestions are tracked as GitHub issues. When creating an enhancement suggestion:

- **Use a clear and descriptive title**
- **Provide a detailed description** of the suggested enhancement
- **Explain why this enhancement would be useful** to most users
- **List any alternative solutions** you've considered

### Pull Requests

1. **Fork the repository** and create your branch from `main`
2. **Make your changes** following the coding standards below
3. **Test your changes** thoroughly on actual hardware
4. **Update documentation** if you've changed functionality
5. **Write clear commit messages** explaining what and why
6. **Submit a pull request** with a comprehensive description

## Coding Standards

### C Code Style

This project follows these conventions:

- **Indentation**: 4 spaces (no tabs)
- **Naming**:
  - Functions: `snake_case`
  - Variables: `snake_case`
  - Constants/Macros: `UPPER_SNAKE_CASE`
  - Static variables: `s_` prefix
- **Comments**:
  - Use Doxygen-style comments for functions
  - Add inline comments for complex logic
  - Keep comments concise and meaningful

### Example Function Documentation

```c
/**
 * @brief Brief description of the function.
 *
 * More detailed description if needed, explaining the purpose,
 * algorithm, or any important implementation details.
 *
 * @param param1 Description of first parameter.
 * @param param2 Description of second parameter.
 * @return Description of return value (if applicable).
 */
static esp_err_t example_function(int param1, const char *param2)
{
    // Implementation
}
```

### Git Commit Messages

- Use present tense ("Add feature" not "Added feature")
- Use imperative mood ("Move cursor to..." not "Moves cursor to...")
- Limit the first line to 72 characters
- Reference issues and pull requests after the first line

Example:
```
Add support for hardware flow control

- Implement RTS/CTS configuration
- Update documentation with flow control examples
- Add menuconfig option for enabling flow control

Fixes #123
```

## Testing Guidelines

### Hardware Testing

- Test on actual ESP32 hardware, not just simulation
- Verify with different baud rates (9600, 115200, 921600)
- Test error handling by intentionally causing errors
- Verify behavior under high data rates

### Code Review Checklist

- [ ] Code compiles without warnings
- [ ] No memory leaks (check with heap tracing if applicable)
- [ ] Proper error handling for all ESP-IDF API calls
- [ ] Documentation updated (README, code comments)
- [ ] No hardcoded values that should be configurable
- [ ] Thread-safe if accessing shared resources

## Documentation

- Keep README.md up to date with any functional changes
- Update flowcharts if the architecture changes
- Add inline code comments for non-obvious logic
- Include examples for new features

## Code of Conduct

### Our Standards

- Be respectful and inclusive
- Welcome newcomers and help them learn
- Accept constructive criticism gracefully
- Focus on what's best for the community
- Show empathy towards other community members

### Unacceptable Behavior

- Harassment or discriminatory language
- Trolling or insulting comments
- Publishing others' private information
- Other conduct that could reasonably be considered inappropriate

## Questions?

Feel free to open an issue with the "question" label if you need help or clarification on anything related to contributing.

## License

By contributing to this project, you agree that your contributions will be licensed under the MIT License.

---

Thank you for contributing! 🎉
