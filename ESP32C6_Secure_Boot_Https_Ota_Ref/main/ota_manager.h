#pragma once
#include "esp_err.h"

/*
 * ota_manager_start
 *
 * Starts the OTA decision loop task. The task triggers OTA only when:
 * - user action is detected (button) OR a cloud trigger is detected
 * - current time is within maintenance window (or allowed without time)
 * - battery is above threshold
 * - basic network readiness checks pass
 *
 * Returns:
 *   ESP_OK if the task was created, otherwise error.
 */
esp_err_t ota_manager_start(void);