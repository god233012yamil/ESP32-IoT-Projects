# ESP32-S3 USB Mass Storage Device Demo

This ESP-IDF project accompanies "Mastering USB on the ESP32-S3 with ESP-IDF: Part 13 - Understanding USB Storage Devices."

The firmware turns an ESP32-S3 into a USB Mass Storage Class device backed by a dedicated FAT partition in internal SPI flash. It demonstrates:

- USB Mass Storage Class using the native USB-OTG peripheral
- TinyUSB device descriptors and bulk endpoints
- SCSI-based block storage as seen by the host
- 512-byte logical block geometry
- FAT formatting and file creation
- Wear levelling for internal SPI flash
- Exclusive ownership transfer between the ESP32-S3 application and USB host

## Requirements

- ESP32-S3 development board with its native USB D+ and D- port available
- ESP-IDF 5.4.x
- At least 4 MB of flash
- A data-capable USB cable

The native USB port must be connected to the ESP32-S3 USB-OTG peripheral. A USB-to-UART connector cannot provide MSC functionality.

## Build and Flash

```text
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port used by the board.

After boot, connect the board's native USB port to the computer. The operating system should detect a removable disk named by the product string `ESP32-S3 USB Storage Lab`.

The volume contains:

- `README.TXT`
- `LAYERS.TXT`
- `SCSI.TXT`

## Storage Layout

The custom partition table creates a dedicated FAT partition named `usb_storage`. The firmware mounts this partition through the ESP-IDF wear-levelling layer and exposes it through the Espressif TinyUSB MSC component.

The project intentionally uses exclusive ownership:

1. The application mounts the FAT volume.
2. The application creates the example files.
3. The application unmounts the local FAT volume.
4. The USB host receives exclusive block-level access.

The ESP32-S3 application must not read or write the FAT volume while the USB host owns it.

## Important Notes

- Always eject the volume before unplugging the USB cable.
- Host operating systems cache file data and FAT metadata.
- Internal flash has finite write endurance. This project uses wear levelling, but an SD card or external storage may be better for write-heavy products.
- The example VID and PID are suitable for development. Production hardware must use identifiers appropriate for the product.
- Reformatting the USB volume from a computer erases the demonstration files. Reset the board to recreate them.

## Project Structure

```text
esp32s3_usb_msc_storage_demo/
|-- CMakeLists.txt
|-- partitions.csv
|-- sdkconfig.defaults
|-- README.md
`-- main/
    |-- CMakeLists.txt
    |-- idf_component.yml
    `-- usb_msc_storage_demo.c
```
