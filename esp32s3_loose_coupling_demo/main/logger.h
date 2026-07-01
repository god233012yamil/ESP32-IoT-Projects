#ifndef LOGGER_H
#define LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the application logger.
 *
 * The logger is intentionally exposed as a service instead of coupling modules
 * directly to a specific output transport.
 */
void logger_init(void);

/**
 * @brief Writes an informational log message.
 *
 * Args:
 *     tag: Module name used as the log tag.
 *     message: Null-terminated message string.
 */
void logger_info(const char *tag, const char *message);

/**
 * @brief Writes a formatted informational log message.
 *
 * Args:
 *     tag: Module name used as the log tag.
 *     format: printf-style format string.
 *     ...: Format arguments.
 */
void logger_infof(const char *tag, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
