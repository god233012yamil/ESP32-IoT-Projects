# Power Optimization Guide

This guide provides detailed techniques for minimizing power consumption in ESP32-based battery-powered devices.

## Table of Contents

- [Understanding ESP32 Power States](#understanding-esp32-power-states)
- [Hardware Design Considerations](#hardware-design-considerations)
- [Firmware Optimization Techniques](#firmware-optimization-techniques)
- [Measurement and Analysis](#measurement-and-analysis)
- [Real-World Examples](#real-world-examples)

## Understanding ESP32 Power States

### Power Modes Overview

| Mode | CPU | Wi-Fi | BT | RTC | Current (Typical) |
|------|-----|-------|----|----|-------------------|
| **Active** | On | Optional | Optional | On | 80-240 mA |
| **Modem Sleep** | On | Off | Off | On | 20-80 mA |
| **Light Sleep** | Off | Off | Off | On | 0.8-2 mA |
| **Deep Sleep** | Off | Off | Off | On | 10-150 µA |
| **Hibernation** | Off | Off | Off | Off | 2.5-5 µA |

### Current Consumption Breakdown

**Active Mode (240 MHz, no RF):**
- CPU core: ~40 mA
- Flash access: ~20 mA
- Peripherals: ~10-20 mA
- **Total: ~70-80 mA**

**Active Mode with Wi-Fi:**
- Base current: ~80 mA
- Wi-Fi RX: +15-20 mA
- Wi-Fi TX: +40-90 mA (up to 170 mA peak)
- **Average during transmission: ~120-170 mA**

**Deep Sleep:**
- RTC controller: ~5-10 µA
- RTC memory: ~1 µA
- External 32kHz crystal: ~1 µA (if used)
- GPIO leakage: Variable (0-100 µA depending on configuration)
- **Typical achievable: 10-50 µA**

## Hardware Design Considerations

### 1. Power Supply Design

#### Linear Regulators
**Pros:**
- Simple, low cost
- Low noise

**Cons:**
- High quiescent current (5-50 µA typical)
- Poor efficiency with voltage drop
- Not ideal for battery applications

**Recommended for deep sleep applications:**
- TPS7A02: 25 nA quiescent current
- MAX38640: 280 nA quiescent current
- TLV62595: 360 nA (buck converter)

#### Buck Converters
**Pros:**
- High efficiency (85-95%)
- Better for large voltage drops

**Cons:**
- More complex
- Potential noise issues

**Recommended:**
- TPS62840: 60 nA quiescent, 95% efficiency
- LTC3525: 65 µA quiescent, synchronous buck-boost
- RT9080: 200 nA quiescent

#### Battery Selection

**Li-ion (18650, Li-Po):**
- Nominal: 3.7V
- Range: 3.0V - 4.2V
- Self-discharge: ~2-5% per month
- Best for: Long runtime, high current

**Alkaline (AA, AAA):**
- Nominal: 1.5V (use 3 in series)
- Range: 0.9V - 1.5V per cell
- Self-discharge: ~1-2% per year
- Best for: Shelf life, temperature range

**Coin Cell (CR2032):**
- Nominal: 3.0V
- Capacity: ~220 mAh
- Self-discharge: ~1% per year
- Best for: Ultra-low power, small size
- **Warning:** Cannot handle Wi-Fi current spikes!

### 2. External Component Leakage

#### Pull-up/Pull-down Resistors

A 10 kΩ pull-up to 3.3V:
```
I = V/R = 3.3V / 10,000Ω = 330 µA
```

**This single resistor consumes more than the ESP32 in deep sleep!**

**Solutions:**
1. Use 100 kΩ or higher value resistors
2. Enable internal pull-ups (45 kΩ typical)
3. Use GPIO outputs to control pull-ups
4. Disable pulls when not needed

#### Voltage Dividers

Battery voltage monitoring with 100kΩ divider:
```
I = 3.7V / 200,000Ω = 18.5 µA
```

**Solution:** Use a GPIO to enable the divider only during measurement:

```c
// Enable divider
gpio_set_level(BATTERY_SENSE_EN, 1);
vTaskDelay(pdMS_TO_TICKS(10));  // Settling time

// Read ADC
int adc_val = adc1_get_raw(ADC1_CHANNEL_6);
float battery_voltage = adc_val * 3.3 / 4095.0 * 2.0;

// Disable divider
gpio_set_level(BATTERY_SENSE_EN, 0);
```

#### Sensors

**Always power-gate sensors:**

```c
// Load switch controlled by GPIO
gpio_set_level(SENSOR_POWER, 1);
vTaskDelay(pdMS_TO_TICKS(100));  // Startup time

// Read sensor
float temperature = read_sensor();

gpio_set_level(SENSOR_POWER, 0);  // Critical!
```

**Recommended load switches:**
- TPS22916: 1 µA off-state current, 100 mA max
- TPS22860: 10 nA off-state current, 2A max
- SIP32510: 1 µA off-state current, 4.5A max

### 3. GPIO Configuration

#### Before Deep Sleep

```c
// Put unused GPIOs in high-impedance mode
void configure_gpios_for_deep_sleep(void) {
    // List of GPIOs to configure
    const gpio_num_t unused_gpios[] = {
        GPIO_NUM_2, GPIO_NUM_4, GPIO_NUM_5,
        // Add all unused GPIOs
    };
    
    for (int i = 0; i < sizeof(unused_gpios)/sizeof(gpio_num_t); i++) {
        gpio_reset_pin(unused_gpios[i]);
        gpio_deep_sleep_hold_dis(unused_gpios[i]);
    }
    
    // Hold GPIOs that need to maintain state
    gpio_deep_sleep_hold_en(GPIO_SENSOR_POWER);
    gpio_hold_en(GPIO_SENSOR_POWER);
}
```

#### RTC GPIO Considerations

During deep sleep, only RTC GPIOs can be used as wake sources. Not all GPIOs are RTC-capable:

**ESP32 RTC GPIOs:**
- GPIO 0, 2, 4, 12-15, 25-27, 32-39

**ESP32-S3 RTC GPIOs:**
- GPIO 0-21

**ESP32-C3 RTC GPIOs:**
- GPIO 0-5

### 4. Crystal Oscillator

**Internal RC Oscillator:**
- Used by default
- ~150 kHz, ±5% accuracy
- No external components
- Higher power in deep sleep (~10 µA)

**External 32.768 kHz Crystal:**
- High accuracy (±20 ppm)
- Lower deep sleep power (~5 µA)
- Requires external components
- Better for RTC applications

## Firmware Optimization Techniques

### 1. Minimize Active Time

The key principle: **Get in, do the work, get out.**

```c
void optimized_work_cycle(void) {
    uint64_t start = esp_timer_get_time();
    
    // 1. Power on sensor (10 ms)
    gpio_set_level(SENSOR_PWR, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 2. Read sensor (5 ms)
    float data = read_sensor_i2c();
    gpio_set_level(SENSOR_PWR, 0);
    
    // 3. Connect to Wi-Fi (3-8 seconds)
    if (wifi_connect(10000) == ESP_OK) {
        // 4. Send data (0.5-2 seconds)
        send_data_mqtt(data);
        wifi_disconnect();
    }
    
    uint64_t duration_ms = (esp_timer_get_time() - start) / 1000;
    ESP_LOGI(TAG, "Work cycle: %lld ms", duration_ms);
    
    // Target: < 10 seconds total active time
}
```

### 2. Optimize Wi-Fi Connection Time

**Use Static IP:**
```c
wifi_config_t cfg = {
    .sta = {
        .ssid = "MyNetwork",
        .password = "password",
    },
};

// Disable DHCP, use static IP
esp_netif_dhcpc_stop(sta_netif);

esp_netif_ip_info_t ip_info = {
    .ip = { .addr = ESP_IP4TOADDR(192, 168, 1, 100) },
    .gw = { .addr = ESP_IP4TOADDR(192, 168, 1, 1) },
    .netmask = { .addr = ESP_IP4TOADDR(255, 255, 255, 0) },
};
esp_netif_set_ip_info(sta_netif, &ip_info);
```

**Connection time improvement:**
- With DHCP: 3-8 seconds
- With static IP: 1-3 seconds

**Cache AP Configuration:**
```c
// ESP-IDF automatically caches last successful AP
// No need to scan all channels
wifi_config_t cfg = {
    .sta = {
        .scan_method = WIFI_FAST_SCAN,  // Skip full scan
        .bssid_set = false,              // Let ESP32 choose
    },
};
```

### 3. Use Appropriate Sleep Modes

#### Light Sleep for Short Delays

```c
// Instead of this (wastes power):
vTaskDelay(pdMS_TO_TICKS(5000));

// Use this (automatic light sleep):
// Make sure all tasks are blocked, PM is enabled
esp_pm_config_t pm_config = {
    .max_freq_mhz = 240,
    .min_freq_mhz = 40,
    .light_sleep_enable = true,  // Enable automatic light sleep
};
esp_pm_configure(&pm_config);

// Now vTaskDelay automatically triggers light sleep
vTaskDelay(pdMS_TO_TICKS(5000));
```

#### Deep Sleep for Long Delays

```c
// For delays > 10 seconds, use deep sleep
esp_sleep_enable_timer_wakeup(60 * 1000000ULL);  // 60 seconds
esp_deep_sleep_start();
```

### 4. Dynamic Frequency Scaling

```c
// Let PM system handle frequency automatically
esp_pm_config_t pm_config = {
    .max_freq_mhz = 240,  // Max when needed
    .min_freq_mhz = 40,   // Minimum to save power
    .light_sleep_enable = true,
};
esp_pm_configure(&pm_config);

// Or manually lock to specific frequency:
esp_pm_lock_handle_t pm_lock;
esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "wifi", &pm_lock);

// Acquire lock when high performance needed
esp_pm_lock_acquire(pm_lock);
// ... do intensive work ...
esp_pm_lock_release(pm_lock);

// Frequency automatically reduces when lock released
```

### 5. RTC Memory for State Persistence

Store state in RTC memory to avoid full initialization after wake:

```c
RTC_DATA_ATTR int boot_count = 0;
RTC_DATA_ATTR float last_temperature = 0.0;
RTC_DATA_ATTR uint32_t wifi_failures = 0;

void app_main(void) {
    boot_count++;
    
    // Skip sensor calibration after first boot
    if (boot_count == 1) {
        calibrate_sensor();
    }
    
    // Use cached value if recent
    if (boot_count % 10 != 0) {
        // Quick reading without full calibration
    }
    
    // Adapt behavior based on history
    if (wifi_failures > 3) {
        // Skip Wi-Fi, save power
    }
    
    esp_deep_sleep_start();
}
```

### 6. ULP Coprocessor

Use ULP for extremely low-power monitoring:

```c
// ULP can run while main CPU is in deep sleep
// Power consumption: ~100 µA (vs 80 mA for main CPU)

// Example: Wake only when ADC exceeds threshold
void setup_ulp_adc_monitor(void) {
    ulp_adc_init();
    ulp_adc_set_threshold(2500);  // mV
    
    // ULP polls ADC every second
    // Only wakes main CPU when threshold exceeded
    
    esp_sleep_enable_ulp_wakeup();
    esp_deep_sleep_start();
}
```

## Measurement and Analysis

### Power Profiling Tools

**1. Nordic PPK2 (~$100)**
- 0.2 µA to 1 A range
- Excellent software
- 5V power output
- Recommended for development

**2. Joulescope JS220 (~$900)**
- 1 nA to 3 A range
- Very high accuracy
- Best for production validation

**3. µCurrent Gold (~$70)**
- 1 nA to 100 mA
- No software, use with oscilloscope
- Great for low-current debugging

**4. INA219/INA260 (~$10)**
- I2C power monitor IC
- 0-3.2A range
- DIY option for continuous monitoring

### Measurement Setup

```
Battery → [Current Meter] → ESP32 VDD
                ↓
         [Oscilloscope/PC]
```

**Critical:**
1. Disconnect USB (CP2102 draws 5-15 mA)
2. Remove power LED (2-10 mA)
3. Measure at battery voltage (3.0-4.2V for Li-ion)
4. Use twisted pair for sense lines to reduce noise

### Interpreting Results

```
┌─────────────────────────────────────────────────────┐
│  80 mA ┤        ┌────┐                               │
│        │        │    │                               │
│  40 mA ┤        │    │                               │
│        │   ┌────┘    └────┐                          │
│  20 µA ┤───┘              └──────────────────────────│
│        └───────────────────────────────────────────► │
│        Wake  Connect  TX  Sleep                      │
│        10ms   3s     500ms  297s                     │
└─────────────────────────────────────────────────────┘
```

**Calculate average current:**
```
I_avg = (T_wake × I_wake + T_active × I_active + T_sleep × I_sleep) / T_total

Example:
I_avg = (0.01s × 40mA + 3.5s × 80mA + 296.49s × 0.02mA) / 300s
      = (0.4 + 280 + 5.93) / 300
      = 0.95 mA

Battery life (2000 mAh) = 2000 / 0.95 = 2105 hours = 88 days
```

## Real-World Examples

### Example 1: Weather Station

**Requirements:**
- Read BME280 sensor every 5 minutes
- Send data via MQTT over Wi-Fi
- Battery life: > 1 year on 18650 Li-ion (3000 mAh)

**Implementation:**
```c
void weather_station_cycle(void) {
    // Power on sensor (10 ms @ 40 mA)
    gpio_set_level(SENSOR_PWR, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Read BME280 via I2C (5 ms @ 40 mA)
    float temp = bme280_read_temperature();
    float hum = bme280_read_humidity();
    float pres = bme280_read_pressure();
    
    gpio_set_level(SENSOR_PWR, 0);
    
    // Connect to Wi-Fi (2.5 s @ 80 mA)
    wifi_connect_static_ip(10000);
    
    // Send MQTT (500 ms @ 120 mA)
    mqtt_publish_sensor_data(temp, hum, pres);
    
    // Disconnect (100 ms @ 40 mA)
    wifi_disconnect();
    
    // Deep sleep (297 s @ 20 µA)
    esp_sleep_enable_timer_wakeup(300 * 1000000ULL);
    esp_deep_sleep_start();
}
```

**Power calculation:**
```
I_avg = (0.01s × 40mA + 2.5s × 80mA + 0.5s × 120mA + 0.1s × 40mA + 296.89s × 0.02mA) / 300s
      = (0.4 + 200 + 60 + 4 + 5.94) / 300
      = 0.90 mA

Battery life = 3000 mAh / 0.90 mA = 3333 hours = 139 days ✓
```

### Example 2: Door Sensor

**Requirements:**
- Wake on door open (GPIO interrupt)
- Send notification via Wi-Fi
- Battery life: > 2 years on CR2032 (220 mAh)

**Implementation:**
```c
void door_sensor_app(void) {
    // Configure EXT0 wake on GPIO
    esp_sleep_enable_ext0_wakeup(GPIO_DOOR_SENSOR, 1);
    
    // No periodic wake needed!
    // esp_sleep_enable_timer_wakeup(...);  // REMOVED
    
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        // Door opened
        wifi_connect_static_ip(10000);
        send_notification("Door opened!");
        wifi_disconnect();
        
        // Debounce: wait for door to close
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    
    esp_deep_sleep_start();
}
```

**Power calculation:**
```
Assumptions:
- Door opens 10 times per day
- Wi-Fi transaction: 3 seconds @ 80 mA
- Deep sleep: 99.9% of time @ 10 µA

Active time per day = 10 events × 3s = 30s
Sleep time per day = 86400s - 30s = 86370s

I_avg = (30s × 80mA + 86370s × 0.01mA) / 86400s
      = (2400 + 863.7) / 86400
      = 0.038 mA

Battery life = 220 mAh / 0.038 mA = 5789 hours = 241 days

NOTE: CR2032 cannot handle 80 mA peaks!
Need supercapacitor or use ESP32-C3 with BLE instead.
```

### Example 3: Data Logger

**Requirements:**
- Sample 10 channels every second
- Store locally, upload every hour
- Battery life: > 6 months on 4× AA (6000 mAh)

**Implementation:**
```c
RTC_DATA_ATTR int sample_count = 0;
RTC_DATA_ATTR float samples[600];  // 10 minutes of data

void data_logger_cycle(void) {
    // Fast sampling: no deep sleep between samples
    // Use light sleep instead
    
    for (int i = 0; i < 10; i++) {
        samples[sample_count++] = read_adc_channel(i);
    }
    
    if (sample_count >= 36000) {  // 1 hour of data
        // Upload batch
        wifi_connect_static_ip(10000);
        upload_data_batch(samples, sample_count);
        wifi_disconnect();
        
        sample_count = 0;
    }
    
    // Light sleep for 1 second
    esp_sleep_enable_timer_wakeup(1000000ULL);
    esp_light_sleep_start();
}
```

**Power calculation:**
```
Sampling: 99% @ 2 mA (light sleep)
Upload: 1% @ 80 mA (Wi-Fi)

I_avg = 0.99 × 2mA + 0.01 × 80mA = 2.78 mA

Battery life = 6000 mAh / 2.78 mA = 2158 hours = 90 days

Not meeting 6-month target!
Solutions:
- Reduce sampling rate
- Use SD card, upload less frequently
- Use lower power mode during sampling
```

## Summary Checklist

### Hardware
- [ ] Use ultra-low quiescent current regulator (< 1 µA)
- [ ] Power-gate all sensors and peripherals
- [ ] Use high-value pull-ups (> 100 kΩ) or internal pulls
- [ ] Add bulk capacitance (100-220 µF) near ESP32
- [ ] Remove unnecessary LEDs
- [ ] Use external 32 kHz crystal for lowest deep sleep current
- [ ] Select appropriate battery for current requirements

### Firmware
- [ ] Enable power management (DFS + light sleep)
- [ ] Use event-driven tasks (no polling)
- [ ] Minimize active time (< 10 seconds per wake)
- [ ] Use static IP for faster Wi-Fi connection
- [ ] Shutdown Wi-Fi completely before sleep
- [ ] Configure GPIOs appropriately before deep sleep
- [ ] Use RTC memory for state persistence
- [ ] Consider ULP for ultra-low-power monitoring

### Measurement
- [ ] Test on production hardware, not dev boards
- [ ] Measure over complete duty cycle
- [ ] Account for worst-case scenarios (Wi-Fi retry, etc.)
- [ ] Verify deep sleep current is reasonable (< 100 µA)
- [ ] Calculate average current and battery life

---

**Remember: The best power optimization is the work you don't do!**
