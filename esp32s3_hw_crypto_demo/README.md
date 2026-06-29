# ESP32-S3 Hardware-Accelerated Cryptography Demo

This ESP-IDF project accompanies the article "Hardware-Accelerated Cryptography on the ESP32-S3: Building Faster and More Secure Embedded Systems."

The application demonstrates:

- AES-256-GCM authenticated encryption and decryption
- ESP32-S3 hardware AES acceleration through Mbed TLS
- SHA-256 hashing through Mbed TLS
- AES-GCM and SHA-256 known-answer tests
- Authentication failure after ciphertext tampering
- AES-GCM and SHA-256 throughput benchmarks
- Hardware-random key and IV generation for the runtime demonstration

## Target

- ESP32-S3
- ESP-IDF 5.4 or later

The code uses the legacy Mbed TLS APIs available in ESP-IDF 5.x. ESP-IDF 6.x keeps compatibility options, but new projects targeting its PSA-first security stack may require API migration in a future release.

## Project Structure

```text
esp32s3_hw_crypto_demo/
|-- CMakeLists.txt
|-- README.md
|-- sdkconfig.defaults
`-- main/
    |-- CMakeLists.txt
    `-- main.c
```

## Build and Run

Open an ESP-IDF terminal and run:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COM_PORT flash monitor
```

Replace `COM_PORT` with the board serial port, such as `COM7` on Windows or `/dev/ttyUSB0` on Linux.

Exit the monitor with `Ctrl+]`.

## Expected Output

The monitor should report:

```text
AES-256-GCM known-answer test: PASS
SHA-256 known-answer test: PASS
Recovered plaintext: Temperature=24.7,Humidity=48.2
Tamper detection: PASS
AES-GCM benchmark: ...
SHA-256 benchmark: ...
All cryptographic tests completed successfully
```

Throughput depends on the ESP-IDF version, CPU frequency, compiler options, memory placement, and concurrent radio activity.

## Confirm Hardware Acceleration

The supplied `sdkconfig.defaults` requests:

```text
CONFIG_MBEDTLS_HARDWARE_AES=y
CONFIG_MBEDTLS_HARDWARE_SHA=y
```

After configuration, verify the generated `sdkconfig`:

Windows PowerShell:

```powershell
Select-String -Path sdkconfig -Pattern "CONFIG_MBEDTLS_HARDWARE"
```

Linux or macOS:

```bash
grep CONFIG_MBEDTLS_HARDWARE sdkconfig
```

## Security Notes

This project is an educational demonstration, not a complete key-management system.

- The runtime AES key is generated at boot and kept temporarily in RAM.
- The key is not persisted and is not suitable for device provisioning.
- Production systems should use per-device keys and protected storage.
- AES-GCM IVs must never repeat under the same key.
- A production nonce counter must remain unique across resets and power loss.
- Do not log keys, plaintext secrets, or protected device credentials.
- Evaluate Secure Boot v2, flash encryption, encrypted NVS, HMAC eFuse keys, or the Digital Signature peripheral for production hardware.

## Comparing Hardware and Software Implementations

To compare configurations, build two separate copies of the project:

1. Keep hardware AES and SHA enabled and record the benchmark results.
2. Disable the corresponding options through `idf.py menuconfig`, rebuild from a clean build directory, and repeat the test.
3. Keep CPU frequency, optimization settings, packet sizes, and radio state identical.

Do not reuse an existing benchmark IV/key sequence in a real protocol. The benchmark counter is intentionally local to this demonstration.
