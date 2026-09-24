// lspdiag

/**
 * @file logger.h
 * @brief Lightweight logging facility for todo.
 *
 * This module provides a minimal, centralized logging API with support for
 * multiple log levels and selectable backends (stdio or syslog).
 *
 * The logger is implemented as a private singleton with static storage
 * duration. It is initialized once via ::logger_init() and then accessed
 * through convenience macros or ::logger_log().
 *
 * Design goals:
 *  - No dynamic allocation
 *  - No context passing
 *  - Safe default behavior before explicit initialization
 *  - Suitable for daemon-style applications
 */


#ifndef __LOGGER_H
#define __LOGGER_H

#include <stdbool.h>

/**
 * @enum log_level_t
 * @brief Logging severity levels.
 *
 * Log levels are ordered by increasing verbosity.
 * Messages with a level greater than the currently configured level
 * are suppressed.
 */
typedef enum {
    LOG_LVL_ERROR = 0,
    LOG_LVL_WARN,
    LOG_LVL_INFO,
    LOG_LVL_DEBUG,
} log_level_t;

/**
 * @enum log_backend_t
 * @brief Logging output backends.
 *
 * Determines where log messages are emitted.
 */
typedef enum {
    LOG_BACKEND_STDIO = 0,
    LOG_BACKEND_SYSLOG,
    LOG_BACKEND_FILE,
} log_backend_t;

/**
 * @brief Initialize the logging subsystem.
 *
 * Configures the logger with the desired log level, backend, and identity
 * string. This function must be called once during program startup before
 * logging is used in production mode.
 *
 * If not called explicitly, the logger operates with default zero-initialized
 * settings.
 *
 * @param level   Maximum log level to emit.
 * @param backend Logging backend to use.
 * @param id      Optional identifier string (e.g. daemon name). If the backend
 *                is ::LOG_BACKEND_FILE, this is treated as the log file path.
 *                If NULL, a default identifier or filename may be used.
 *                The string is copied, so the caller need not keep it alive.
 *                An out-of-range @p level is clamped to ::LOG_LVL_DEBUG.
 */
void logger_init(log_level_t level, log_backend_t backend, const char *id);

/**
 * @brief Shut down the logging subsystem.
 *
 * Releases backend-specific resources (e.g. closes syslog).
 * This function is typically called during program shutdown.
 */
void logger_close(void);

/**
 * @brief Emit a log message.
 *
 * Logs a formatted message at the specified log level.
 * Messages with a level greater than the configured maximum
 * are silently discarded. The logger ends each message with a newline,
 * so @p fmt should not.
 *
 * @param level Log severity level.
 * @param fmt   printf-style format string.
 * @param ...   Format arguments.
 */
void logger_log(log_level_t level, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

/**
 * @brief Check whether DEBUG-level messages are currently enabled.
 *
 * Mirrors the exact check ::logger_log() performs internally. Intended for
 * call sites that need to build up expensive debug-only diagnostics (e.g.
 * joining an array into a string) - guard that work behind this check so it
 * is skipped entirely outside of ::LOG_LVL_DEBUG, instead of paying the cost
 * on every call and letting a suppressed ::LDBG discard the result.
 *
 * @return true if the logger is initialized and its level is
 *         ::LOG_LVL_DEBUG, false otherwise.
 */
bool logger_debug_enabled(void);

/**
 * @name Convenience logging macros
 * @{
 *
 * Shorthand macros for logging at fixed severity levels.
 * These macros forward directly to ::logger_log().
 */

/** Log an error-level message. */
#define LERR(fmt, ...)  logger_log(LOG_LVL_ERROR, fmt, ##__VA_ARGS__)

/** Log an warning-level message. */
#define LWARN(fmt, ...) logger_log(LOG_LVL_WARN, fmt, ##__VA_ARGS__)

/** Log an inforomational message. */
#define LINFO(fmt, ...) logger_log(LOG_LVL_INFO, fmt, ##__VA_ARGS__)

/** Log an debug-level message. */
#define LDBG(fmt, ...)  logger_log(LOG_LVL_DEBUG, fmt, ##__VA_ARGS__)

#endif //__LOGGER_H
