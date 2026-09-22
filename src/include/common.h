// lspdiag

/**
 * @file common.h
 * @brief Common definitions and convenience macros used across todo.
 *
 * This header centralizes small, widely-used utilities:
 *  - standard return codes
 *  - early-return guard macros with integrated logging
 *
 * The guard macros are intended to reduce repetitive boilerplate in functions by
 * combining validation, error logging, and consistent return values.
 *
 * @note This header depends on the logger subsystem (see logger.h) for error reporting.
 */

#ifndef __TODO_COMMON_H
#define __TODO_COMMON_H

#include <stdbool.h>
#include <logger.h>

/** Standard success return code */
#define RT_SUCCESS   0

/** Standard error return code */
#define RT_ERROR    -1


/**
 * @def RETURN_ERR_IF
 * @brief Validate a condition and return ::RT_ERROR if it is true.
 *
 * If @p cond evaluates to true, the macro:
 *  - logs an error using ::LERR()
 *  - returns ::RT_ERROR from the calling function
 *
 * Intended for functions that return an `int` status.
 *
 * @param cond Condition that triggers an error return when true.
 * @param fmt  printf-style format string for the error log.
 * @param ...  Format arguments.
 *
 * @warning This macro returns from the calling function. Use only where returning
 *          ::RT_ERROR is valid and expected.
 *
 * @code
 * int foo(int x)
 * {
 *     RETURN_ERR_IF(x < 0, "invalid x=%d", x);
 *     return RT_SUCCESS;
 * }
 * @endcode
 */
#define RETURN_ERR_IF(cond, fmt, ...)            \
    do {                                         \
        if (cond) {                              \
            LERR(fmt, ##__VA_ARGS__);            \
            return RT_ERROR;                     \
        }                                        \
    } while (0)


/**
 * @def RETURN_NULLERR_IF
 * @brief Validate a condition and return NULL if it is true.
 *
 * If @p cond evaluates to true, the macro:
 *  - logs an error using ::LERR()
 *  - returns NULL from the calling function
 *
 * Intended for functions that return a pointer type.
 *
 * @param cond Condition that triggers an error return when true.
 * @param fmt  printf-style format string for the error log.
 * @param ...  Format arguments.
 *
 * @warning This macro returns from the calling function. Use only where returning
 *          NULL is valid and expected.
 *
 * @code
 * void *alloc_object(size_t sz)
 * {
 *     RETURN_NULLERR_IF(sz == 0, "size must be non-zero");
 *     return malloc(sz);
 * }
 * @endcode
 */
#define RETURN_NULLERR_IF(cond, fmt, ...)        \
    do {                                         \
        if (cond) {                              \
            LERR(fmt, ##__VA_ARGS__);            \
			return NULL;                         \
        }                                        \
    } while (0)


/**
 * @def LERR_IF
 * @brief Validate a condition and log error if it is true.
 *
 * If @p cond evaluates to true, the macro:
 *  - logs an error using ::LERR()
 *
 * Intended for logging error on condition.
 *
 * @param cond Condition that triggers an error when true.
 * @param fmt  printf-style format string for the error log.
 * @param ...  Format arguments.
 *
 * @code
 * int foo(int x)
 * {
 *     LERR_IF(x < 0, "invalid x=%d", x);
 * }
 * @endcode
 */
#define LERR_IF(cond, fmt, ...)            \
    do {                                         \
        if (cond) {                              \
            LERR(fmt, ##__VA_ARGS__);            \
        }                                        \
    } while (0)


/**
 * @def LWARN_IF
 * @brief Validate a condition and log warning if it is true.
 *
 * If @p cond evaluates to true, the macro:
 *  - logs an warning using ::LWARN()
 *
 * Intended for logging warning on condition.
 *
 * @param cond Condition that triggers a warning when true
 * @param fmt  printf-style format string for the warning log.
 * @param ...  Format arguments.
 *
 * @code
 * int foo(int x)
 * {
 *     LWARN_IF(x < 0, "invalid x=%d", x);
 * }
 * @endcode
 */
#define LWARN_IF(cond, fmt, ...)            \
    do {                                         \
        if (cond) {                              \
            LWARN(fmt, ##__VA_ARGS__);            \
        }                                        \
    } while (0)

#endif //__TODO_COMMON_H
