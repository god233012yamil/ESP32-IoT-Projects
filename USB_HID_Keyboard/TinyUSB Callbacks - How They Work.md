# TinyUSB Callbacks - Registration Explained

## How TinyUSB Callbacks Work

### Traditional Callback Pattern (NOT used by TinyUSB)
```c
// Many libraries do this:
typedef void (*callback_t)(void);

void register_callback(callback_t cb) {
    my_callback = cb;  // Store pointer
}

void my_mount_handler(void) { ... }
register_callback(my_mount_handler);  // Explicit registration
```

### TinyUSB's Approach (Weak Linking)
```c
// TinyUSB defines weak functions (in tinyusb/src/device/usbd.c)
__attribute__((weak)) void tud_mount_cb(void) {
    // Default empty implementation
}

// Your code simply defines the function
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "Mounted!");
}

// Linker automatically replaces weak with your strong symbol!
```

## How It Works Under the Hood

### Step 1: TinyUSB Defines Weak Callbacks
Inside TinyUSB library (`tinyusb/src/device/usbd.c`):

```c
// Weak default implementations
__attribute__((weak)) void tud_mount_cb(void) {}
__attribute__((weak)) void tud_umount_cb(void) {}
__attribute__((weak)) void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; }
__attribute__((weak)) void tud_resume_cb(void) {}
```

### Step 2: TinyUSB Calls These Functions
```c
// Inside TinyUSB USB stack
void usbd_control_xfer_cb(uint8_t rhport, uint8_t ep_addr, 
                          xfer_result_t result, uint32_t xferred_bytes) {
    if (/* device configured */) {
        tud_mount_cb();  // Calls YOUR function if you defined it!
    }
}
```

### Step 3: Your Code Overrides
When you define `tud_mount_cb()` in your `main.c`, the linker sees:
- ❌ Weak symbol from TinyUSB library
- ✅ Strong symbol from your code
- **Decision:** Use your strong symbol! 🎯

## Verification: How to See Callbacks Being Called

### Method 1: Enable Debug Logging
I've updated the code to add logging in each callback:

```c
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "✓ USB Device Mounted - Keyboard ready!");
    led_send_command(LED_CMD_BLINK, 1);  // Visual confirmation!
}
```

**Expected Output:**
```
I (1234) USB_HID_KEYBOARD: Initializing USB...
I (1235) USB_HID_KEYBOARD: USB HID initialized
I (1456) USB_HID_KEYBOARD: ✓ USB Device Mounted - Keyboard ready!
```

### Method 2: Check USB Enumeration Events

When you connect the device, you should see this sequence:

```
I (xxx) USB_HID_KEYBOARD: System ready! Press button to start automation.
[Connect USB cable]
I (xxx) USB_HID_KEYBOARD: ✓ USB Device Mounted - Keyboard ready!
[LED blinks once]
```

When you disconnect:
```
[Unplug USB cable]
I (xxx) USB_HID_KEYBOARD: ✗ USB Device Unmounted
```

### Method 3: Test Suspend/Resume

Put computer to sleep:
```
I (xxx) USB_HID_KEYBOARD: ⏸ USB Suspended (remote_wakeup=0)
[Wake computer]
I (xxx) USB_HID_KEYBOARD: ▶ USB Resumed
```

## All TinyUSB Callbacks Explained

### Device Callbacks

#### `tud_mount_cb()`
- **When Called:** Device enumeration complete, configuration selected
- **Purpose:** Device is ready to send/receive data
- **Use Case:** Start your application logic, turn on LED
- **Frequency:** Once per connection

#### `tud_umount_cb()`
- **When Called:** USB cable unplugged or host disabled device
- **Purpose:** Clean up resources, save state
- **Use Case:** Stop timers, flush buffers, turn off LED
- **Frequency:** Once per disconnection

#### `tud_suspend_cb(bool remote_wakeup_en)`
- **When Called:** USB bus idle for 3ms (host went to sleep)
- **Purpose:** Enter low-power mode
- **Use Case:** Reduce power consumption, save state
- **Parameter:** `remote_wakeup_en` - Can device wake host?
- **Frequency:** When host suspends bus

#### `tud_resume_cb()`
- **When Called:** USB bus active again after suspend
- **Purpose:** Resume normal operation
- **Use Case:** Restore from low-power mode
- **Frequency:** After suspend when bus becomes active

### HID-Specific Callbacks

#### `tud_hid_get_report_cb()`
- **When Called:** Host requests current report (rare for keyboards)
- **Purpose:** Return current keyboard state
- **Parameters:**
  - `instance` - HID interface number
  - `report_id` - Which report
  - `report_type` - INPUT/OUTPUT/FEATURE
  - `buffer` - Where to write report
  - `reqlen` - Max buffer size
- **Return:** Number of bytes written
- **Use Case:** Usually not needed for keyboards

#### `tud_hid_set_report_cb()`
- **When Called:** Host sends data to device
- **Purpose:** Receive keyboard LED status (Caps/Num/Scroll Lock)
- **Parameters:**
  - `instance` - HID interface number
  - `report_id` - Which report
  - `report_type` - Usually OUTPUT
  - `buffer` - Data from host
  - `bufsize` - Data length
- **Use Case:** Update Caps Lock LED, etc.

#### `tud_hid_descriptor_report_cb(uint8_t instance)`
- **When Called:** During enumeration, host requests HID descriptor
- **Purpose:** Provide HID Report Descriptor
- **Return:** Pointer to descriptor array
- **Critical:** This MUST return your descriptor!
- **Use Case:** Tell host what kind of HID device you are

## Why This Design?

### Advantages of Weak Linking

1. **Optional Callbacks**: Only implement what you need
2. **No Registration Code**: Cleaner, simpler
3. **Compile-Time Binding**: Faster, no function pointer overhead
4. **Type Safety**: Compiler checks function signature
5. **Easy to Override**: Just define the function

### Comparison

| Aspect | Weak Linking | Function Pointers |
|--------|--------------|-------------------|
| Registration | Automatic | Manual `register_callback()` |
| Performance | Direct call | Indirect call (slower) |
| Code Size | Smaller | Larger (registration code) |
| Flexibility | Compile-time | Runtime |
| Safety | Compile-time checks | Runtime checks needed |

## Testing Your Callbacks

### Test 1: Mount Callback
```c
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "✓ MOUNT CALLBACK WORKS!");
    led_send_command(LED_CMD_BLINK, 5);  // Blink 5 times
}
```

**Expected:** After connecting USB, LED blinks 5 times

### Test 2: Descriptor Callback
```c
uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    ESP_LOGI(TAG, "✓ DESCRIPTOR CALLBACK WORKS!");
    return hid_keyboard_report_descriptor;
}
```

**Expected:** During enumeration, you see the log message

### Test 3: SET_REPORT Callback
```c
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, 
                            hid_report_type_t report_type, 
                            uint8_t const* buffer, uint16_t bufsize) {
    ESP_LOGI(TAG, "✓ SET_REPORT CALLBACK WORKS! LED=0x%02X", buffer[0]);
}
```

**Test:** Press Caps Lock on your computer
**Expected:** See log with LED status byte

## Common Issues

### Issue: Callback Never Called

**Possible Causes:**
1. Function signature doesn't match exactly
2. Function declared `static` (should be global)
3. TinyUSB not initialized properly
4. USB cable issue (charge-only cable)

**Debug:**
```c
// Add logging at function entry
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "I WAS CALLED!");  // Add this!
    // ... rest of code
}
```

### Issue: Callbacks Called Multiple Times

**Expected Behavior:**
- Mount/Unmount: Once per connect/disconnect
- Suspend/Resume: Can happen multiple times
- GET_REPORT: Host-dependent
- SET_REPORT: When keyboard LEDs change
- Descriptor: During enumeration

### Issue: Wrong Function Signature

```c
// ❌ WRONG - Won't override weak symbol
void tud_mount_cb(int unused) { ... }

// ❌ WRONG - Wrong return type
int tud_mount_cb(void) { return 0; }

// ✅ CORRECT - Exact match
void tud_mount_cb(void) { ... }
```

## Confirming It Works

After flashing, you should see:

```
I (320) USB_HID_KEYBOARD: ESP32 USB HID Keyboard with FreeRTOS Starting...
I (321) USB_HID_KEYBOARD: GPIO initialized
I (322) USB_HID_KEYBOARD: Initializing USB...
I (323) USB_HID_KEYBOARD: USB HID initialized
I (324) USB_HID_KEYBOARD: Button task started
I (325) USB_HID_KEYBOARD: LED task started
I (326) USB_HID_KEYBOARD: System ready! Press button to start automation.

[Plug in USB cable to computer]

D (456) USB_HID_KEYBOARD: HID Descriptor requested for instance=0
I (457) USB_HID_KEYBOARD: ✓ USB Device Mounted - Keyboard ready!

[LED blinks once - visual confirmation!]

[Press Caps Lock on computer]

D (789) USB_HID_KEYBOARD: HID SET_REPORT: instance=0, report_id=0, type=2, size=1
D (790) USB_HID_KEYBOARD: Keyboard LEDs: CapsLock=1 NumLock=0 ScrollLock=0
```

## Summary

✅ **No explicit registration needed** - Callbacks work through weak linking
✅ **Automatically called** - TinyUSB library calls them at the right time
✅ **Just define functions** - Linker does the magic
✅ **Now with logging** - Updated code includes debug messages
✅ **Visual feedback** - LED blinks when mounted

The callbacks ARE being called - the weak linking mechanism ensures this happens automatically at compile/link time, not runtime!

## Further Reading

- [GCC Weak Attribute Documentation](https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-weak-function-attribute)
- [TinyUSB Device API](https://docs.tinyusb.org/en/latest/reference/device.html)
- [ESP-IDF TinyUSB Component](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_device.html)