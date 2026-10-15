# Contributing to ESP32-IoT-Projects

Thank you for your interest in contributing to this collection of ESP32 IoT projects and reference implementations! Contributions from the embedded systems and IoT community are welcome.

## Table of Contents

- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Project Structure](#project-structure)
- [How to Contribute](#how-to-contribute)
- [Pull Request Process](#pull-request-process)
- [Coding Standards](#coding-standards)
- [Reporting Issues](#reporting-issues)

## Getting Started

1. **Fork** this repository on GitHub
2. **Clone** your fork: `git clone https://github.com/YOUR-USERNAME/ESP32-IoT-Projects.git`
3. Pick a project folder to work on, or propose a new one

## Development Setup

### Hardware Requirements

- ESP32 / ESP32-S3 / ESP32-C6 development board
- USB-to-Serial adapter (if not built-in)
- Sensors/peripherals as required by the specific project

### Software Requirements

- **ESP-IDF** >= 5.0 (for ESP-IDF based projects) — [Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
- **Arduino IDE** >= 2.x (for Arduino-style projects) with ESP32 board support
- **Python** >= 3.8 (for build/automation scripts)

### Setting Up ESP-IDF

```bash
# Clone ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32,esp32s3,esp32c6
source ./export.sh
```

### Building a Project

```bash
cd <project-folder>
idf.py build
idf.py flash monitor
```

## Project Structure

Each project lives in its own directory with a self-contained `CMakeLists.txt` or `platformio.ini`. When adding a new project:

```
your-new-project/
├── main/
│   ├── CMakeLists.txt
│   └── main.c        (or main.cpp)
├── CMakeLists.txt
└── README.md         (required!)
```

## How to Contribute

### What We Welcome

- ✨ **New project examples** — New demos, peripherals, protocols, RTOS patterns
- 🐛 **Bug fixes** — Corrections to existing project code
- 📚 **Documentation** — Better README files, wiring diagrams, explanations
- 🧪 **Test cases** — Unit tests or automated CI checks
- 🔧 **Refactoring** — Improving code clarity, adding error handling
- 🌡️ **New hardware support** — Adding support for new ESP32 chip variants

### Adding a New Project

1. Create a new directory with a descriptive name: `esp32s3_your_feature_demo`
2. Include a `README.md` covering:
   - Project purpose and what it demonstrates
   - Hardware requirements and wiring diagram
   - Build and flash instructions
   - Expected output / how to verify it works
3. Ensure the project builds cleanly with no warnings

## Pull Request Process

1. Create a branch: `git checkout -b feat/esp32s3-new-demo`
2. Make your changes with clear commit messages
3. Test on real hardware whenever possible
4. Submit a PR with:
   - What the project demonstrates or what bug was fixed
   - Hardware used for testing (chip variant, board name)
   - Photo/video of it working (highly encouraged!)

### Commit Message Format

- `feat: add ESP32-S3 USB HID gamepad demo`
- `fix: correct SPI clock polarity in flash demo`
- `docs: add wiring diagram to I2C devices demo`
- `chore: update IDF version requirement in NTP client`

## Coding Standards

### C/C++

- Use **descriptive variable names** (avoid single-letter variables except loop counters)
- Add **comments** explaining non-obvious hardware interactions
- Handle **error return codes** from ESP-IDF APIs (check `ESP_ERROR_CHECK` usage)
- Use `esp_log` for debug output with appropriate log levels
- Free allocated memory and unregister event handlers in cleanup functions

### Python Scripts

- Follow **PEP 8** style
- Add docstrings to functions
- Use `argparse` for CLI arguments

## Reporting Issues

When reporting a bug, please include:
- **ESP32 chip variant** (ESP32, ESP32-S3, ESP32-C6, etc.)
- **ESP-IDF version** (`idf.py --version`)
- **Board/module** used
- **Error message** or unexpected behavior
- **Minimal reproduction steps**

Open an issue at: https://github.com/god233012yamil/ESP32-IoT-Projects/issues

---

Happy hacking! ⚡🔌