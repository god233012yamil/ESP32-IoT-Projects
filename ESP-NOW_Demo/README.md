# ESP-NOW Beginner Demo for ESP32-S3

A minimal, well-documented ESP-NOW project for ESP32-S3 using ESP-IDF and FreeRTOS. Perfect for beginners learning wireless communication between ESP32 devices without requiring Wi-Fi infrastructure.

## 📋 Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Hardware Requirements](#-hardware-requirements)
- [Software Requirements](#-software-requirements)
- [Project Structure](#-project-structure)
- [How It Works](#-how-it-works)
- [Getting Started](#-getting-started)
- [Configuration](#-configuration)
- [Building and Flashing](#-building-and-flashing)
- [Usage Examples](#-usage-examples)
- [Troubleshooting](#-troubleshooting)
- [Advanced Topics](#-advanced-topics)
- [Contributing](#-contributing)
- [License](#-license)

## 🔍 Overview

ESP-NOW is a connectionless Wi-Fi communication protocol developed by Espressif that enables quick and low-power control of smart devices without the need for a router. This project demonstrates the fundamental concepts of ESP-NOW communication between two ESP32-S3 devices.

### What is ESP-NOW?

ESP-NOW is a wireless communication protocol that allows multiple devices to communicate without using Wi-Fi. Key characteristics:

- **Connectionless**: No need to connect to a Wi-Fi access point
- **Fast**: Low latency communication (typically <20ms)
- **Low Power**: Efficient for battery-powered devices
- **Peer-to-Peer**: Direct device-to-device communication
- **Range**: Similar to Wi-Fi (up to 200m line-of-sight in open space)

## ✨ Features

This beginner-friendly implementation demonstrates:

- ✅ Wi-Fi initialization in STA mode without AP connection
- ✅ ESP-NOW initialization and configuration
- ✅ Dual-role firmware (Sender/Receiver)
- ✅ FreeRTOS task-based architecture
- ✅ Callback-to-task handoff using queues
- ✅ Send and receive callbacks
- ✅ Broadcast and unicast addressing
- ✅ Packet structure with version and sequence numbering
- ✅ Comprehensive logging and error handling
- ✅ Menuconfig-based configuration

## 🛠 Hardware Requirements

- **2x ESP32-S3 Development Boards** (any variant)
  - One configured as Sender
  - One configured as Receiver
- **USB cables** for programming and power
- **Computer** with USB ports

**Note**: While this project is designed for ESP32-S3, it can be adapted for ESP32, ESP32-C3, or ESP32-C6 with minimal changes.

## 💻 Software Requirements

- **ESP-IDF v5.0 or later** (v5.1+ recommended)
- **Python 3.8+** (for ESP-IDF toolchain)
- **Serial terminal** (built-in with ESP-IDF monitor)

### Installing ESP-IDF

Follow the official Espressif installation guide:
- [ESP-IDF Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)

Quick setup:
```bash
# Clone ESP-IDF
git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git

# Install prerequisites
cd esp-idf
./install.sh esp32s3

# Set up environment (run this in every new terminal)
. ./export.sh
```

## 📁 Project Structure

```
espnow_beginner_espidf/
├── CMakeLists.txt              # Top-level CMake configuration
├── sdkconfig.defaults          # Default SDK configuration
├── README.md                   # This file
├── main/
│   ├── CMakeLists.txt          # Main component CMake
│   ├── Kconfig.projbuild       # Project configuration menu
│   └── main.c                  # Main application code
└── FLOWCHART.md                # Mermaid flowchart diagram
```

### Key Files

- **main.c**: Contains all application logic including:
  - Initialization routines (NVS, Wi-Fi, ESP-NOW)
  - Sender task (transmits packets)
  - Receiver task (processes incoming packets)
  - ESP-NOW callbacks
  - Helper functions

- **Kconfig.projbuild**: Defines configurable parameters:
  - Device role (Sender/Receiver)
  - Wi-Fi channel
  - Peer MAC address

- **sdkconfig.defaults**: Sets sensible defaults for logging and Wi-Fi

## 🔄 How It Works

### Architecture Overview

The project uses a clean separation between interrupt context (callbacks) and task context (application logic):

1. **Initialization Phase**:
   - NVS (Non-Volatile Storage) initialization
   - Wi-Fi driver initialization in STA mode
   - ESP-NOW protocol initialization
   - Peer configuration
   - FreeRTOS queue and event group creation

2. **Sender Mode**:
   - Creates a periodic task that runs every 1 second
   - Builds a packet with version, type, sequence, and counter
   - Transmits via `esp_now_send()`
   - Waits for send confirmation via event group

3. **Receiver Mode**:
   - ESP-NOW callback receives incoming packets
   - Packets are queued to avoid blocking the callback
   - Receiver task processes queued packets
   - Logs packet contents with source MAC address

### Packet Structure

```c
typedef struct {
    uint8_t  version;    // Protocol version (1)
    uint8_t  msg_type;   // Message type identifier (1)
    uint16_t seq;        // Sequence number (increments)
    uint32_t counter;    // Payload counter (increments)
} app_packet_t;
```

**Size**: 8 bytes total

This simple structure includes:
- **version**: Protocol versioning for future compatibility
- **msg_type**: Allows for multiple message types
- **seq**: Sequence number to detect packet loss
- **counter**: Application-specific counter value

### Communication Flow

**Sender Device**:
```
[Sender Task] → Build Packet → esp_now_send() → [Wi-Fi Driver]
       ↓                                              ↓
[Wait Event] ← Set Event Bit ← [Send Callback] ← [TX Complete]
```

**Receiver Device**:
```
[Wi-Fi Driver] → [Receive Callback] → Queue Packet → [Receiver Task] → Log Data
```

## 🚀 Getting Started

### Quick Start Guide

#### Step 1: Clone the Repository

```bash
git clone https://github.com/yourusername/espnow_beginner_espidf.git
cd espnow_beginner_espidf
```

#### Step 2: Set Up ESP-IDF Environment

```bash
# Run this in the terminal (or add to .bashrc/.zshrc)
. $HOME/esp/esp-idf/export.sh
```

#### Step 3: Configure the Receiver

```bash
idf.py menuconfig
```

Navigate to: `ESP-NOW Beginner Demo` and configure:
- **Build as Receiver**: Enable
- **ESP-NOW Wi-Fi channel**: 1 (or any channel 1-13)
- **Peer MAC address**: `FF:FF:FF:FF:FF:FF` (broadcast)

Save and exit.

#### Step 4: Build and Flash Receiver

```bash
# Build
idf.py build

# Flash and monitor (replace PORT with your serial port)
# Linux/Mac: /dev/ttyUSB0, /dev/ttyACM0, /dev/cu.usbserial-*
# Windows: COM3, COM4, etc.
idf.py -p PORT flash monitor
```

#### Step 5: Note the Receiver MAC Address

From the monitor output, find the Wi-Fi MAC address:
```
I (xxx) wifi:mode : sta (xx:xx:xx:xx:xx:xx)
```

Note this MAC address for the sender configuration.

Press `Ctrl+]` to exit the monitor.

#### Step 6: Configure the Sender

```bash
idf.py menuconfig
```

Configure:
- **Build as Sender**: Enable
- **ESP-NOW Wi-Fi channel**: 1 (same as receiver!)
- **Peer MAC address**: Enter the receiver's MAC from Step 5

#### Step 7: Build and Flash Sender

Connect the second ESP32-S3 board:

```bash
idf.py -p PORT flash monitor
```

#### Step 8: Observe Communication

You should see output on the **Sender**:
```
I (xxx) espnow_demo: Configured channel=1 peer=XX:XX:XX:XX:XX:XX
I (xxx) espnow_demo: Role: SENDER
I (xxx) espnow_demo: Send status: SUCCESS
I (xxx) espnow_demo: Send status: SUCCESS
...
```

And on the **Receiver**:
```
I (xxx) espnow_demo: Configured channel=1 peer=FF:FF:FF:FF:FF:FF
I (xxx) espnow_demo: Role: RECEIVER
I (xxx) espnow_demo: RX from XX:XX:XX:XX:XX:XX | ver=1 type=1 seq=0 counter=0 (len=8)
I (xxx) espnow_demo: RX from XX:XX:XX:XX:XX:XX | ver=1 type=1 seq=1 counter=1 (len=8)
...
```

## ⚙️ Configuration

### Using menuconfig

```bash
idf.py menuconfig
```

Navigate to `ESP-NOW Beginner Demo` to configure:

#### Device Role

**Sender (Transmitter)**:
- Sends a packet every 1 second
- Increments sequence and counter values
- Waits for send confirmation

**Receiver (Gateway)**:
- Listens for incoming ESP-NOW packets
- Processes and logs received data
- Can receive from multiple senders

#### Wi-Fi Channel

**Range**: 1-13 (channel 1 by default)

**Important**: Both sender and receiver MUST use the same channel!

Common channels:
- **1, 6, 11**: Non-overlapping channels (recommended)
- **Others**: May work but can overlap

#### Peer MAC Address

**Broadcast Mode** (`FF:FF:FF:FF:FF:FF`):
- Sends to all ESP-NOW devices on the same channel
- No pairing required
- Not encrypted
- Good for testing and one-to-many communication

**Unicast Mode** (specific MAC like `AA:BB:CC:DD:EE:FF`):
- Sends to a specific device
- Requires peer pairing
- Can be encrypted
- Better for production and point-to-point communication

### Configuration Examples

**Broadcast Testing**:
```
Sender Config:
- Role: Sender
- Channel: 1
- Peer MAC: FF:FF:FF:FF:FF:FF

Receiver Config:
- Role: Receiver
- Channel: 1
- Peer MAC: FF:FF:FF:FF:FF:FF (not used in receiver mode)
```

**Unicast Communication**:
```
Sender Config:
- Role: Sender
- Channel: 1
- Peer MAC: 24:6F:28:XX:XX:XX (receiver's MAC)

Receiver Config:
- Role: Receiver
- Channel: 1
- Peer MAC: FF:FF:FF:FF:FF:FF (receives from all)
```

## 🔨 Building and Flashing

### Standard Build

```bash
# Clean previous builds
idf.py fullclean

# Build the project
idf.py build

# Flash to device
idf.py -p PORT flash

# Monitor serial output
idf.py -p PORT monitor

# Or do all in one command
idf.py -p PORT flash monitor
```

### Build Options

**Set target chip** (if not ESP32-S3):
```bash
idf.py set-target esp32
idf.py set-target esp32c3
```

**Optimize for size**:
```bash
idf.py menuconfig
# Navigate to: Compiler options → Optimization Level → Optimize for size (-Os)
```

**Enable debug logging**:
```bash
idf.py menuconfig
# Navigate to: Component config → Log output → Default log verbosity → Debug
```

### Finding Your Serial Port

**Linux**:
```bash
ls /dev/ttyUSB* /dev/ttyACM*
# Usually /dev/ttyUSB0 or /dev/ttyACM0
```

**macOS**:
```bash
ls /dev/cu.*
# Usually /dev/cu.usbserial-* or /dev/cu.SLAB_USBtoUART
```

**Windows**:
- Open Device Manager
- Look under "Ports (COM & LPT)"
- Note the COM port number (e.g., COM3)

## 📊 Usage Examples

### Example 1: Basic Communication Test

**Setup**: One sender, one receiver, broadcast mode

**Expected Output (Sender)**:
```
I (312) espnow_demo: Configured channel=1 peer=FF:FF:FF:FF:FF:FF
I (312) espnow_demo: Role: SENDER
I (1322) espnow_demo: Send status: SUCCESS
I (2332) espnow_demo: Send status: SUCCESS
I (3342) espnow_demo: Send status: SUCCESS
```

**Expected Output (Receiver)**:
```
I (289) espnow_demo: Configured channel=1 peer=FF:FF:FF:FF:FF:FF
I (289) espnow_demo: Role: RECEIVER
I (1301) espnow_demo: RX from 24:6F:28:12:34:56 | ver=1 type=1 seq=0 counter=0 (len=8)
I (2311) espnow_demo: RX from 24:6F:28:12:34:56 | ver=1 type=1 seq=1 counter=1 (len=8)
I (3321) espnow_demo: RX from 24:6F:28:12:34:56 | ver=1 type=1 seq=2 counter=2 (len=8)
```

### Example 2: Multiple Receivers

**Setup**: One sender (broadcast), multiple receivers, same channel

All receivers will receive the same packets simultaneously. This is useful for:
- Environmental monitoring (one sensor, multiple displays)
- Broadcast notifications
- Synchronized actions

### Example 3: Bidirectional Communication

**Setup**: Both devices as sender and receiver (requires code modification)

You can modify the code to create tasks for both sending and receiving on the same device.

## 🐛 Troubleshooting

### Issue: "Send status: FAIL"

**Possible Causes**:
1. Receiver not powered on
2. Devices on different Wi-Fi channels
3. Devices out of range
4. Peer MAC address incorrect (for unicast)

**Solutions**:
- Verify both devices are on the same channel
- Check peer MAC address configuration
- Move devices closer together
- Use broadcast mode for testing

### Issue: No packets received

**Possible Causes**:
1. Different Wi-Fi channels
2. ESP-NOW not initialized properly
3. Receiver task not running

**Solutions**:
```bash
# Check channel configuration on both devices
idf.py menuconfig

# Verify logs show proper initialization
I (xxx) espnow_demo: Role: RECEIVER

# Ensure no error messages during init
```

### Issue: "Invalid peer MAC string"

**Problem**: MAC address format incorrect

**Solution**: Use format `AA:BB:CC:DD:EE:FF` with colons and hex digits
```
Correct:   24:6F:28:12:34:56
Incorrect: 24-6F-28-12-34-56
Incorrect: 246F28123456
Incorrect: 24:6f:28:12:34:56 (lowercase hex is OK but use uppercase)
```

### Issue: Build errors

**Problem**: ESP-IDF not properly set up

**Solution**:
```bash
# Re-run ESP-IDF setup
. $HOME/esp/esp-idf/export.sh

# Clean and rebuild
idf.py fullclean
idf.py build
```

### Issue: Device not detected

**Problem**: Serial port access or driver issues

**Linux Solution**:
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and log back in

# Check permissions
ls -l /dev/ttyUSB0
```

**Windows Solution**:
- Install CP210x or CH340 USB drivers
- Check Device Manager for COM port

**macOS Solution**:
- Install Silicon Labs CP210x driver if needed
- Check System Preferences → Security & Privacy

### Issue: Packets lost or intermittent

**Possible Causes**:
1. Wi-Fi interference
2. Distance too far
3. Obstacles blocking signal
4. Channel congestion

**Solutions**:
- Try different Wi-Fi channels (1, 6, 11)
- Reduce distance between devices
- Remove obstacles (metal, walls)
- Add retry logic (see Advanced Topics)

## 🎓 Advanced Topics

### Adding Encryption

ESP-NOW supports AES-128 encryption. Modify `espnow_config_peer()`:

```c
esp_now_peer_info_t peer = {0};
memcpy(peer.peer_addr, peer_mac, 6);
peer.ifidx = WIFI_IF_STA;
peer.channel = channel;
peer.encrypt = true;  // Enable encryption

// Set encryption key (16 bytes)
uint8_t key[16] = {0x00, 0x01, 0x02, ..., 0x0F};
memcpy(peer.lmk, key, 16);
```

Both sender and receiver must use the same encryption key.

### Adding Reliability (ACKs and Retries)

Implement application-level acknowledgments:

**Sender**:
```c
// Send packet and wait for ACK
send_packet(&pkt);
wait_for_ack(pkt.seq, timeout);
// Retry if no ACK received
```

**Receiver**:
```c
// Send ACK after receiving packet
send_ack(received_seq);
```

### Power Optimization

For battery-powered devices:

```c
// Use light sleep between sends
esp_sleep_enable_timer_wakeup(1000000); // 1 second
esp_light_sleep_start();

// Or use modem sleep
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
```

### Multiple Peers

Send to multiple specific devices:

```c
uint8_t peer1[6] = {0x24, 0x6F, 0x28, 0x12, 0x34, 0x56};
uint8_t peer2[6] = {0x24, 0x6F, 0x28, 0x78, 0x9A, 0xBC};

espnow_config_peer(peer1, channel);
espnow_config_peer(peer2, channel);

// Send to peer1
esp_now_send(peer1, data, len);
// Send to peer2
esp_now_send(peer2, data, len);
```

### Custom Packet Types

Extend the packet structure:

```c
typedef struct {
    uint8_t  version;
    uint8_t  msg_type;  // 1=counter, 2=sensor, 3=control
    uint16_t seq;
    union {
        uint32_t counter;
        struct {
            float temperature;
            float humidity;
        } sensor;
        struct {
            uint8_t relay_state;
            uint8_t led_brightness;
        } control;
    } payload;
} app_packet_t;
```

### Performance Tuning

**Reduce send interval**:
```c
vTaskDelay(pdMS_TO_TICKS(100));  // 100ms instead of 1000ms
```

**Increase queue size**:
```c
#define RX_QUEUE_LEN 32  // Handle burst traffic
```

**Adjust task priorities**:
```c
xTaskCreate(receiver_task, "receiver_task", 4096, NULL, 10, NULL);  // Higher priority
```

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

### Development Guidelines

1. Follow the existing code style
2. Add comments for complex logic
3. Update documentation for new features
4. Test on actual hardware before submitting
5. Keep commits focused and atomic

### Reporting Issues

When reporting issues, please include:
- ESP-IDF version
- Hardware model (ESP32-S3, etc.)
- Complete error logs
- Configuration settings
- Steps to reproduce

## 📄 License

This project is provided as-is for educational purposes. Feel free to use, modify, and distribute.

## 🙏 Acknowledgments

- Espressif Systems for ESP-IDF and ESP-NOW protocol
- ESP32 community for documentation and examples

## 📚 Additional Resources

- [ESP-NOW Official Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

## 📞 Support

For questions and support:
- Open an issue on GitHub
- Check ESP-IDF forums
- Review ESP32 community resources

---

**Happy ESP-NOW coding! 🚀**
