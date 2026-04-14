/**
 * @file scale_config.h
 * @brief Compile-time configuration for the ESP32-S3 Digital Scale.
 *
 * Centralises every tunable constant: GPIO pins, Wi-Fi credentials,
 * web-server settings, calibration defaults, FreeRTOS task parameters,
 * and NVS namespace keys.  Edit this file to adapt the project to a
 * different board layout or network environment.
 *
 * @author  ESP32-S3 Scale Project
 * @version 1.0.0
 * @date    2025
 */

#ifndef SCALE_CONFIG_H
#define SCALE_CONFIG_H

/* ── HX711 GPIO ───────────────────────────────────────────────────── */

/** GPIO connected to HX711 DOUT (data output). */
#define CONFIG_HX711_DOUT_GPIO        4

/** GPIO connected to HX711 SCK (serial clock / PD_SCK). */
#define CONFIG_HX711_SCK_GPIO         5

/* ── Measurement ──────────────────────────────────────────────────── */

/** ADC samples averaged per weight reading (higher = less noise, more latency). */
#define CONFIG_SCALE_SAMPLES          10

/** Interval in ms between consecutive weight readings. */
#define CONFIG_MEASURE_INTERVAL_MS    500

/** Samples averaged during a tare operation. */
#define CONFIG_TARE_SAMPLES           20

/** Default scale factor (raw counts per gram). Replace after calibration. */
#define CONFIG_SCALE_FACTOR           430.0f

/** Full-scale capacity in grams (4 × 50 000 g). */
#define CONFIG_MAX_WEIGHT_G           200000.0f

/** Readings below this threshold are displayed as 0.00 g. */
#define CONFIG_ZERO_THRESHOLD_G       2.0f

/* ── Wi-Fi ────────────────────────────────────────────────────────── */

/** Wi-Fi SSID for the network the ESP32-S3 will join. */
#define CONFIG_WIFI_SSID              "YOUR_WIFI_SSID"

/** Wi-Fi password. */
#define CONFIG_WIFI_PASSWORD          "YOUR_WIFI_PASSWORD"

/** Maximum Wi-Fi connection attempts before giving up. */
#define CONFIG_WIFI_MAX_RETRIES       5

/* ── Web Server ───────────────────────────────────────────────────── */

/** TCP port the HTTP server listens on. */
#define CONFIG_WEBSERVER_PORT         80

/** Maximum simultaneous HTTP connections. */
#define CONFIG_WEBSERVER_MAX_SOCKETS  4

/** Server-Sent Events endpoint path (used by the dashboard). */
#define CONFIG_SSE_URI                "/events"

/** Weight data JSON endpoint path. */
#define CONFIG_API_WEIGHT_URI         "/api/weight"

/** Tare command endpoint path. */
#define CONFIG_API_TARE_URI           "/api/tare"

/** Calibration command endpoint path. */
#define CONFIG_API_CALIBRATE_URI      "/api/calibrate"

/** Status / info endpoint path. */
#define CONFIG_API_STATUS_URI         "/api/status"

/** Interval in ms between SSE weight-push events. */
#define CONFIG_SSE_PUSH_INTERVAL_MS   500

/* ── FreeRTOS Tasks ───────────────────────────────────────────────── */

/** Stack in bytes for the weight-measurement task. */
#define CONFIG_MEASURE_TASK_STACK     4096

/** Priority for the measurement task. */
#define CONFIG_MEASURE_TASK_PRIORITY  5

/** Stack in bytes for the SSE push task. */
#define CONFIG_SSE_TASK_STACK         4096

/** Priority for the SSE push task. */
#define CONFIG_SSE_TASK_PRIORITY      4

/* ── NVS ──────────────────────────────────────────────────────────── */

/** NVS namespace for persisted calibration data. */
#define CONFIG_NVS_NAMESPACE          "scale_cfg"

/** NVS key for the scale factor. */
#define CONFIG_NVS_KEY_SCALE          "scale_factor"

/** NVS key for the tare offset. */
#define CONFIG_NVS_KEY_TARE           "tare_offset"

/* ── Console ──────────────────────────────────────────────────────── */

/** UART baud rate for debug output. */
#define CONFIG_CONSOLE_BAUD           115200

#endif /* SCALE_CONFIG_H */
