# Production Security Checklist

## Before irreversible provisioning

- Confirm the exact ESP32-S3 module and silicon revision.
- Confirm flash capacity, mode, and frequency.
- Verify the production partition table.
- Test signed OTA updates and rollback.
- Test recovery after interrupted filesystem writes.
- Confirm all required eFuse keys and key purposes.
- Confirm the order used to burn read-protection and write-protection bits.
- Verify that the production signing key is backed up and access controlled.
- Verify manufacturing tools against disposable boards.

## Device-unique material

- Use a unique Flash Encryption key for each device.
- Use a device-unique application root secret when application-level encryption is required.
- Do not derive secrets only from a MAC address or another public identifier.
- Do not compile fleet-wide production secrets into firmware.

## Boot chain

- Enable Secure Boot v2.
- Sign the bootloader and every application image.
- Verify that unsigned and modified images do not boot.
- Define secure OTA signing and release procedures.
- Configure anti-rollback when required.

## Storage

- Verify that the LittleFS partition has the `encrypted` flag.
- Confirm Flash Encryption is enabled on the physical device.
- Search a raw flash dump for known plaintext test strings.
- Confirm cloned encrypted flash does not operate on another device.
- Avoid storing plaintext copies in logs, core dumps, or temporary files.
- Add authenticated encryption for data that requires tamper detection.

## Interfaces

- Review UART download mode restrictions.
- Review JTAG and USB-JTAG restrictions.
- Remove or protect manufacturing test commands.
- Review exposed test pads and connectors.
- Disable production debug logging that contains sensitive data.

## Reliability

- Run thousands of randomized power-cut cycles.
- Test power removal during create, write, flush, close, rename, and delete operations.
- Verify primary and backup record recovery.
- Test a full partition and fragmented partition.
- Verify wear under the expected write workload.
- Confirm the watchdog margin during worst-case filesystem operations.

## End-of-line verification

- Read and archive the eFuse summary.
- Confirm Flash Encryption is active.
- Confirm Secure Boot is active.
- Confirm the signed production image version.
- Confirm the encrypted LittleFS partition mounts and passes a read/write test.
- Confirm production debug restrictions.
- Record the device serial number and provisioning result.
