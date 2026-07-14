# Range-Test Procedure

1. Flash the project to an ESP32-S3 board.
2. Use a Coded PHY capable central to scan for `ESP32S3-LR-BLE`.
3. Connect and subscribe to the telemetry characteristic.
4. Record RSSI, filtered RSSI, negotiated PHY, packet delivery, and reconnect count.
5. Test `1M`, `S2`, and `S8` commands independently.
6. Repeat at multiple distances and at 0, 90, 180, and 270 degree orientations.
7. Test inside the final enclosure and at minimum supply voltage.
8. Change one variable at a time so improvements remain attributable.

A useful acceptance metric is the maximum distance at which at least 99 percent of 10,000 application packets arrive within the required latency.
