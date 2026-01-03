# Contributing to ESP32 Scheduling Demo

Thank you for your interest in contributing to this educational project! 

## How to Contribute

### Reporting Bugs

If you find a bug, please open an issue with:
- Clear description of the problem
- Steps to reproduce
- Expected vs actual behavior
- ESP-IDF version and ESP32 board type
- Relevant logs or screenshots

### Suggesting Enhancements

We welcome suggestions for:
- Additional scheduling demonstrations
- Code improvements
- Documentation clarifications
- New educational examples

Please open an issue to discuss major changes before implementing.

### Pull Requests

1. **Fork** the repository
2. **Create a branch** for your feature (`git checkout -b feature/amazing-feature`)
3. **Make your changes** following the code style below
4. **Test both modes** (Preemptive and Cooperative)
5. **Commit** with clear messages (`git commit -m 'Add amazing feature'`)
6. **Push** to your branch (`git push origin feature/amazing-feature`)
7. **Open a Pull Request** with a clear description

## Code Style Guidelines

### C Code Style

- Use **4 spaces** for indentation (no tabs)
- Follow existing naming conventions:
  - Functions: `snake_case`
  - Constants: `UPPER_CASE`
  - Types: `snake_case_t`
  - Globals: `g_` prefix
- Add **Doxygen comments** for all functions:
  ```c
  /**
   * @brief Brief function description.
   *
   * Args:
   *   param: Parameter description.
   *
   * Returns:
   *   Return value description.
   */
  ```
- Keep lines under **100 characters** where practical
- Use descriptive variable names

### Documentation

- Update README.md for feature changes
- Keep code comments clear and concise
- Update flowcharts if architecture changes
- Use proper markdown formatting

### Testing

Before submitting:
- [ ] Code compiles without warnings (`idf.py build`)
- [ ] Preemptive mode tested and working
- [ ] Cooperative mode tested and working
- [ ] No memory leaks (check with `idf.py monitor`)
- [ ] Documentation updated

## Code of Conduct

### Our Standards

- Be respectful and inclusive
- Welcome newcomers and learners
- Focus on constructive feedback
- Assume good intentions

### Unacceptable Behavior

- Harassment or discriminatory language
- Personal attacks
- Trolling or insulting comments
- Publishing others' private information

## Questions?

Feel free to open an issue for questions or clarifications. We're here to help!

## License

By contributing, you agree that your contributions will be licensed under the MIT-0 License.

---

Thank you for helping make this educational resource better! 🎓
