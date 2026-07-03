# ESP32-S3 Context Switch Demo

This ESP-IDF project accompanies the article "Context Switching Explained: How RTOSes Work Internally".

The firmware demonstrates how an RTOS-based embedded application uses task priorities, task notifications, queues, mutexes, timer interrupts, and stack monitoring. It is designed for ESP32-S3 targets using ESP-IDF 5.x.

## What the Demo Shows

- A GPTimer ISR wakes a high-priority control task every 10 ms.
- The high-priority task preempts lower-priority work when it becomes ready.
- A communication task simulates asynchronous application events.
- A logger task receives events through a FreeRTOS queue.
- A monitor task reports stack high-water marks.
- GPIO pins can be observed with a logic analyzer or oscilloscope to visualize task activity.

## Default Trace Pins

| Signal | Default GPIO |
|---|---:|
| Control task trace | GPIO2 |
| Communication task trace | GPIO4 |
| Background task trace | GPIO5 |

The pins can be changed with `idf.py menuconfig` under `Context Switch Demo`.

## Build and Flash

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the serial port used by your board.

## Expected Monitor Output

The monitor shows events produced by different tasks and periodic stack watermark reports. The control task is woken by a hardware timer interrupt and should run at a regular interval.

## Hardware Notes

Connect a logic analyzer or oscilloscope to the configured GPIO pins. The control trace pin goes high only while the control task is running. The other pins toggle from lower-priority tasks, allowing you to observe scheduling behavior indirectly.
