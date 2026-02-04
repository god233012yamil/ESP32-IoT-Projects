# ESP32 FreeRTOS SMP Inter-Core IPC Demo (ESP-IDF)

This project demonstrates two practical inter-core communication patterns on ESP32 using FreeRTOS SMP:

1) Queue-based Producer/Consumer (tasks pinned to different cores)
2) Task Notification-based signaling (tasks pinned to different cores)

Both variants pin the producer to Core 0 and the consumer to Core 1 to make cross-core behavior obvious in logs.

## Requirements

- ESP-IDF v5.x
- Any ESP32 dual-core target (ESP32 / ESP32-S3). (ESP32-C3/C6 are single-core; pinning Core 1 will not apply.)

## Configure

Select the demo mode via menuconfig:

- `Component config -> SMP IPC Demo -> Demo mode`

## Build and Flash

```bash
idf.py set-target esp32
idf.py menuconfig
idf.py build flash monitor
```

## Expected Output

### Queue mode
- Producer prints "sent N (core 0)"
- Consumer prints "got N (core 1)"

### Notification mode
- Producer prints "notify seq=N (core 0)"
- Consumer prints "got X notify(ies) (core 1)"

## Notes

- Queues are ideal for moving payloads safely across cores.
- Task notifications are ideal for ultra-low-overhead signaling (often ISR-to-task), and can act like a lightweight counting semaphore.

