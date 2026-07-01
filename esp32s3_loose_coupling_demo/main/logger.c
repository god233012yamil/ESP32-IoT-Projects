#include "logger.h"

#include <stdarg.h>
#include <stdio.h>

#include "esp_log.h"

/**
 * @brief Initializes the logger.
 */
void logger_init(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
}

/**
 * @brief Logs an info message.
 *
 * Args:
 *     tag: The tag for the log message.
 *     message: The log message.
 */
void logger_info(const char *tag, const char *message)
{
    ESP_LOGI(tag, "%s", message);
}

/**
 * @brief Logs an info message with formatting.
 *
 * Args:
 *     tag: The tag for the log message.
 *     format: The format string for the log message.
 *     ...: Variable arguments for the format string.
 */
void logger_infof(const char *tag, const char *format, ...)
{
    char buffer[128];
    va_list args;

    va_start(args, format);
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    ESP_LOGI(tag, "%s", buffer);
}
