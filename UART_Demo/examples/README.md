# Examples

This directory contains example scripts and utilities for testing and interacting with the ESP32 UART Reference project.

## Available Examples

### 1. Simple Example (`simple_example.py`)

A basic script demonstrating how to communicate with the ESP32 over UART.

**Features:**
- Connects to ESP32
- Sends PING, VERSION, and UPTIME commands
- Interactive command mode
- Simple and easy to understand

**Usage:**
```bash
python3 simple_example.py /dev/ttyUSB0
```

**Requirements:**
```bash
pip install pyserial
```

### 2. Test Suite (`test_uart.py`)

Comprehensive automated testing script for the UART interface.

**Features:**
- Tests all available commands
- Stress testing with configurable iteration count
- Buffer overflow handling tests
- Colored output with pass/fail indicators
- Performance metrics (commands per second)
- Error rate analysis

**Usage:**
```bash
# Basic tests
python3 test_uart.py /dev/ttyUSB0

# Stress test with 1000 iterations
python3 test_uart.py /dev/ttyUSB0 --stress --count 1000

# Run all tests including stress test
python3 test_uart.py /dev/ttyUSB0 --all
```

**Options:**
- `--baud`: Set baud rate (default: 115200)
- `--timeout`: Set response timeout in seconds (default: 1.0)
- `--stress`: Run stress test
- `--count`: Number of stress test iterations (default: 100)
- `--all`: Run all tests including stress

**Example Output:**
```
ESP32 UART Reference - Test Suite
==================================================

[TEST] PING command
  ✓ PASS: Got expected response: 'PONG'

[TEST] VERSION command
  ✓ PASS: Got expected response: 'ESP32S3_UART_REF v1'

[TEST] UPTIME command
  ✓ PASS: Got uptime: 12543 ms (12.54 seconds)

==================================================
Test Summary
==================================================
Total Tests: 5
Passed: 5
Failed: 0

✓ ALL TESTS PASSED
```

## Installation

Install Python dependencies:

```bash
pip install -r requirements.txt
```

Or install manually:

```bash
pip install pyserial
```

## Creating Your Own Examples

### Template Script

```python
#!/usr/bin/env python3
import serial
import time

# Configuration
PORT = '/dev/ttyUSB0'
BAUD = 115200

# Connect
ser = serial.Serial(PORT, BAUD, timeout=1.0)
time.sleep(0.5)

# Send command
ser.write(b'PING\n')

# Read response
response = ser.readline().decode('utf-8').strip()
print(f"Response: {response}")

# Close
ser.close()
```

### Best Practices

1. **Always include newline**: Commands must end with `\n`
2. **Handle timeouts**: Set reasonable timeout values
3. **Clear buffers**: Use `ser.reset_input_buffer()` after connecting
4. **Error handling**: Catch `serial.SerialException` and other errors
5. **Close connections**: Always close the serial port when done

## Integration Examples

### Node.js Example

```javascript
const SerialPort = require('serialport');
const Readline = require('@serialport/parser-readline');

const port = new SerialPort('/dev/ttyUSB0', { baudRate: 115200 });
const parser = port.pipe(new Readline({ delimiter: '\n' }));

parser.on('data', (line) => {
  console.log('Received:', line);
});

port.write('PING\n');
```

### C Example (Linux)

```c
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

int main() {
    int fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
    
    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    tcsetattr(fd, TCSANOW, &options);
    
    write(fd, "PING\n", 5);
    
    char buf[256];
    int n = read(fd, buf, sizeof(buf)-1);
    buf[n] = '\0';
    printf("Response: %s\n", buf);
    
    close(fd);
    return 0;
}
```

## Troubleshooting

### Permission Denied (Linux)

Add your user to the dialout group:
```bash
sudo usermod -a -G dialout $USER
# Log out and log back in
```

Or use sudo (not recommended):
```bash
sudo python3 simple_example.py /dev/ttyUSB0
```

### Port Not Found

**Linux:** List available ports:
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

**Windows:** Check Device Manager for COM ports

**macOS:** List available ports:
```bash
ls /dev/tty.*
```

### No Response

1. Check baud rate matches (115200)
2. Verify TX/RX connections are correct (crossed)
3. Ensure common ground connection
4. Try loopback test (connect ESP32 TX to RX)

### Garbled Output

1. Verify baud rate is correct
2. Check for electrical noise
3. Try shorter cables
4. Ensure stable power supply

## Contributing Examples

Have a useful example? Contributions are welcome!

1. Create your example in this directory
2. Add documentation explaining usage
3. Test on multiple platforms if possible
4. Submit a pull request

See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

## Additional Resources

- [PySerial Documentation](https://pyserial.readthedocs.io/)
- [ESP-IDF UART Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/uart.html)
- [Serial Communication Tutorial](https://learn.sparkfun.com/tutorials/serial-communication)
