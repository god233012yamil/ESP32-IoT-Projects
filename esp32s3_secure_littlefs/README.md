# ESP32-S3 Secure LittleFS Example

This ESP-IDF project demonstrates a production-oriented LittleFS storage design for the ESP32-S3.

The project uses:

- A LittleFS data partition marked with the `encrypted` partition flag
- Runtime verification of Flash Encryption and Secure Boot status
- Atomic file replacement using a temporary file and rename operation
- An application-level record header with format version, payload length, sequence number, and checksum
- Recovery from an interrupted write by validating the primary and backup records
- A repeatable write/read verification task
- No automatic filesystem formatting after an ordinary mount failure

## Important security behavior

The `encrypted` partition flag only encrypts the LittleFS partition when ESP32-S3 Flash Encryption is enabled.

The project builds and runs on an unsecured development board, but it prints a warning when Flash Encryption or Secure Boot is not active. A raw flash dump is protected only after Flash Encryption has been correctly provisioned.

Secure Boot and Flash Encryption use irreversible eFuse operations. Validate the complete manufacturing, OTA, and recovery process on disposable development boards before enabling production settings.

## Requirements

- ESP-IDF 5.4 or newer
- ESP32-S3 target
- 8 MB flash by default
- Python and the standard ESP-IDF toolchain
- Internet access during the first build so the ESP Component Manager can download LittleFS

## Build and flash

```bash
idf.py set-target esp32s3
idf.py fullclean
idf.py build
idf.py -p COM_PORT flash monitor
```

Replace `COM_PORT` with the serial port used by the board.

## Expected behavior

At startup, the application:

1. Reports whether Flash Encryption is active.
2. Reports whether Secure Boot is active.
3. Confirms that the `storage` partition carries the encrypted flag.
4. Mounts LittleFS at `/littlefs`.
5. Loads the last valid record when one exists.
6. Writes a new record with an incremented sequence number.
7. Reads the record back and verifies its header and checksum.
8. Prints LittleFS capacity and usage information.

The application writes these files:

```text
/littlefs/device_record.bin
/littlefs/device_record.bak
/littlefs/device_record.tmp
```

The temporary and backup files support recovery when power is removed during a storage update.

## Enabling Flash Encryption for development testing

Use `idf.py menuconfig` and review:

```text
Security features
    Enable flash encryption on boot
    Enable usage mode
```

For initial experiments, use the development workflow documented for the exact ESP-IDF release. After the first encrypted boot, normal flashing and erase behavior changes.

Confirm the runtime log reports:

```text
Flash Encryption: ENABLED
```

The `storage` partition is marked `encrypted` in `partitions.csv`. When Flash Encryption is active, reads and writes through the ESP-IDF partition layer are transparently decrypted and encrypted by the ESP32-S3 hardware.

## Enabling Secure Boot v2

Use `idf.py menuconfig` and review:

```text
Security features
    Enable hardware Secure Boot in bootloader
    Secure Boot version
    Sign binaries during build
```

ESP32-S3 uses Secure Boot v2. Generate and protect the RSA-3072 signing key according to the documentation for the installed ESP-IDF release.

Confirm the runtime log reports:

```text
Secure Boot: ENABLED
```

Do not keep a production signing key in the project directory or source repository.

## Production recommendations

- Use a unique Flash Encryption key per device.
- Protect signing keys with restricted access or an HSM-backed signing service.
- Enable Secure Boot before deployment.
- Restrict JTAG, USB-JTAG, UART download mode, and other debug paths according to the product threat model.
- Use signed OTA updates.
- Define anti-rollback behavior.
- Never log credentials or decrypted secrets.
- Do not automatically format a filesystem after an unexplained mount failure.
- Execute repeated power interruption tests during file writes.
- Archive the final eFuse summary for each manufactured device.
- Add authenticated application-level encryption for records that require tamper detection, because XTS flash encryption provides confidentiality but not record authentication.

## Flash size

The default configuration assumes an ESP32-S3 module with 8 MB of flash. Change the flash-size setting and partition layout when using a different module.

## Project structure

```text
esp32s3_secure_littlefs/
|-- CMakeLists.txt
|-- partitions.csv
|-- sdkconfig.defaults
|-- README.md
|-- docs/
|   `-- PRODUCTION_SECURITY_CHECKLIST.md
`-- main/
    |-- CMakeLists.txt
    |-- idf_component.yml
    |-- main.c
    |-- secure_storage.c
    `-- secure_storage.h
```
