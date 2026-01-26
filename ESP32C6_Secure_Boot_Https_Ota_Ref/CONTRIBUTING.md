# Contributing to ESP32-C6 Secure Boot HTTPS OTA Reference

Thank you for your interest in contributing! This document provides guidelines for contributing to this project.

## How to Contribute

### Reporting Issues
- Check if the issue already exists in the [Issues](https://github.com/yourusername/ESP32C6_Secure_Boot_Https_Ota_Ref/issues) section
- Provide detailed information including:
  - ESP-IDF version
  - Hardware configuration
  - Steps to reproduce
  - Expected vs actual behavior
  - Relevant log output

### Suggesting Enhancements
- Open an issue with a clear title and description
- Explain why this enhancement would be useful
- Provide examples of how it would work

### Pull Requests

1. **Fork the repository** and create your branch from `main`
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes**
   - Follow the existing code style
   - Add comments for complex logic
   - Update documentation if needed

3. **Test your changes**
   - Build and test on actual hardware
   - Verify secure boot and OTA functionality
   - Check for memory leaks

4. **Commit your changes**
   - Use clear, descriptive commit messages
   - Reference relevant issues (e.g., "Fixes #123")

5. **Push to your fork** and submit a pull request
   ```bash
   git push origin feature/your-feature-name
   ```

6. **Wait for review**
   - Address any requested changes
   - Be responsive to feedback

## Code Style Guidelines

### C Code
- Follow ESP-IDF coding standards
- Use 4 spaces for indentation (no tabs)
- Place braces on the same line for functions and control structures
- Use descriptive variable and function names
- Add Doxygen-style comments for functions

Example:
```c
/**
 * @brief Brief description of function
 * 
 * @param param1 Description of param1
 * @return Return value description
 */
esp_err_t my_function(int param1)
{
    // Implementation
}
```

### Documentation
- Update README.md if adding new features
- Add comments to explain non-obvious code
- Include examples for new functionality

## Security Considerations

When contributing security-related changes:
- Never commit real private keys or certificates
- Follow secure coding practices
- Document security implications of changes
- Consider impact on secure boot and encryption

## Testing

Before submitting:
- Test on ESP32-C6 hardware
- Verify secure boot functionality
- Test OTA update process
- Check for memory leaks with heap tracing

## Questions?

Feel free to open an issue for any questions about contributing!

## Code of Conduct

- Be respectful and constructive
- Welcome newcomers and help them learn
- Focus on what is best for the community
- Show empathy towards others

Thank you for contributing! 🎉
