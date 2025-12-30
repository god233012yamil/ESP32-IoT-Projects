# ESP32 FreeRTOS Queue Producer-Consumer Demo

A clean, beginner-friendly demonstration of the producer-consumer pattern using FreeRTOS queues on ESP32 with ESP-IDF. This project showcases inter-task communication, proper queue handling, and timeout management.

## Overview

This demo implements a classic producer-consumer architecture where:
- A **producer task** generates messages periodically and sends them to a FreeRTOS queue
- A **consumer task** receives and processes messages from the queue
- Both tasks handle timeouts gracefully and log their activities

## Features

- ✅ FreeRTOS queue-based inter-task communication
- ✅ Structured message passing with sequence numbers and timestamps
- ✅ Timeout handling for both send and receive operations
- ✅ Queue overflow detection and logging
- ✅ Clean, well-documented code with descriptive comments
- ✅ Beginner-friendly ESP-IDF project structure

## Hardware Requirements

- ESP32 development board (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, etc.)
- USB cable for programming and serial monitoring

## Software Requirements

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) v4.4 or later
- Python 3.7+ (for ESP-IDF toolchain)

## Project Structure

```
QUEUE_Producer_Consumer_Demo/
├── CMakeLists.txt          # Top-level CMake configuration
├── main/
│   ├── CMakeLists.txt      # Component CMake configuration
│   └── main.c              # Main application code
├── sdkconfig               # ESP-IDF configuration
└── README.md               # This file
```

## Message Structure

Messages passed between tasks are structured as follows:

```c
typedef struct {
    uint32_t seq;        // Monotonic sequence number
    TickType_t tick;     // FreeRTOS tick count at send time
    int32_t payload;     // Example payload data
} message_t;
```

## Configuration Parameters

The demo uses the following configurable parameters (defined in `main.c`):

| Parameter | Value | Description |
|-----------|-------|-------------|
| `DEMO_QUEUE_LENGTH` | 8 | Maximum number of messages in queue |
| `PRODUCER_PERIOD_MS` | 1000 ms | Time between message generation |
| `PRODUCER_SEND_TIMEOUT_MS` | 50 ms | Timeout for queue send operation |
| `CONSUMER_RECV_TIMEOUT_MS` | 2000 ms | Timeout for queue receive operation |

## Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/QUEUE_Producer_Consumer_Demo.git
cd QUEUE_Producer_Consumer_Demo
```

### 2. Set Up ESP-IDF Environment

Make sure you have ESP-IDF installed and configured. If not, follow the [ESP-IDF installation guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

```bash
# Set up ESP-IDF environment (run this in each new terminal)
. $HOME/esp/esp-idf/export.sh
```

### 3. Configure the Project

Set the target ESP32 chip (adjust for your specific board):

```bash
idf.py set-target esp32
```

For other ESP32 variants:
- ESP32-S2: `idf.py set-target esp32s2`
- ESP32-S3: `idf.py set-target esp32s3`
- ESP32-C3: `idf.py set-target esp32c3`

### 4. Build the Project

```bash
idf.py build
```

### 5. Flash and Monitor

```bash
idf.py flash monitor
```

To exit the serial monitor, press `Ctrl+]`.

## Expected Output

Once running, you should see output similar to:

```
I (329) queue_demo: Queue producer-consumer demo starting...
I (329) queue_demo: Queue created: length=8 item_size=12
I (339) queue_demo: Tasks started. Monitor output via idf.py monitor.
I (1349) queue_demo: Producer sent: seq=1 payload=10 tick=134
I (1349) queue_demo: Consumer got : seq=1 payload=10 age_ticks=0
I (2349) queue_demo: Producer sent: seq=2 payload=20 tick=234
I (2349) queue_demo: Consumer got : seq=2 payload=20 age_ticks=0
I (3349) queue_demo: Producer sent: seq=3 payload=30 tick=334
I (3349) queue_demo: Consumer got : seq=3 payload=30 age_ticks=0
```

## How It Works

### System Flow

1. **Initialization (`app_main`)**:
   - Creates a FreeRTOS queue with capacity for 8 messages
   - Spawns producer and consumer tasks
   - Both tasks run continuously

2. **Producer Task**:
   - Generates a new message every 1000 ms
   - Increments sequence number
   - Captures current tick count
   - Attempts to send message to queue with 50 ms timeout
   - Logs success or queue full condition

3. **Consumer Task**:
   - Waits for messages from the queue with 2000 ms timeout
   - Processes received messages
   - Calculates message age (time spent in queue)
   - Logs received data or timeout condition

### Task Priorities

- **Producer Task**: Priority 5
- **Consumer Task**: Priority 6 (higher priority ensures prompt processing)

### Timeout Handling

- **Producer timeout**: If the queue is full, the producer logs a warning and continues
- **Consumer timeout**: If no message arrives within 2 seconds, the consumer logs a heartbeat message

## Customization Ideas

This demo can be extended in various ways:

- 🔧 **Multiple Producers**: Create multiple producer tasks sending to the same queue
- 🔧 **Multiple Consumers**: Implement multiple consumers processing messages
- 🔧 **Priority Queues**: Experiment with message priorities
- 🔧 **Real Sensors**: Replace the dummy payload with actual sensor data (temperature, accelerometer, etc.)
- 🔧 **Dynamic Timing**: Adjust producer/consumer rates based on system conditions
- 🔧 **Queue Statistics**: Add code to monitor queue depth and utilization

## Troubleshooting

### Build Errors

**Problem**: CMake configuration errors  
**Solution**: Ensure ESP-IDF is properly installed and environment is sourced

**Problem**: Compilation errors  
**Solution**: Check that you're using ESP-IDF v4.4 or later

### Runtime Issues

**Problem**: No output on serial monitor  
**Solution**: 
- Check the correct serial port is selected
- Verify baud rate (default 115200)
- Press the RESET button on your ESP32 board

**Problem**: Queue full warnings  
**Solution**: This is expected behavior if you modify the timing. Increase `DEMO_QUEUE_LENGTH` or adjust task timing.

## Learning Resources

- [FreeRTOS Queue Documentation](https://www.freertos.org/a00018.html)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html)
- [FreeRTOS Fundamentals](https://www.freertos.org/Documentation/RTOS_book.html)

## License

This project is provided as-is for educational and demonstration purposes. Feel free to use, modify, and distribute as needed.

## Contributing

Contributions are welcome! Feel free to:
- Report bugs
- Suggest improvements
- Submit pull requests
- Share your variations and extensions

## Author

Created as an educational demonstration of FreeRTOS queue usage on ESP32.

## Acknowledgments

- Espressif Systems for ESP-IDF
- FreeRTOS community for excellent documentation and examples
