#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <time.h>

#include <logger.h>

/** Human-readable log-level names, indexed by log_level_t; internal formatting only. */
static const char *level_str[] = {
	"ERROR",
	"WARN",
	"INFO",
	"DEBUG"
};

#define LEVEL_COUNT (sizeof(level_str) / sizeof(level_str[0]))

/** Name for @p level, or "?" if it has no entry in level_str. */
static const char *level_name(log_level_t level)
{
	return (unsigned)level < LEVEL_COUNT ? level_str[level] : "?";
}

static struct {
	log_level_t level;
	log_backend_t backend;
	char id[PATH_MAX];
	FILE *file;
	int initialized;
} logger;


void logger_init(log_level_t level, log_backend_t backend, const char *id)
{
	logger.level = ((unsigned)level < LEVEL_COUNT) ? level : LOG_LVL_DEBUG;
	logger.backend = backend;
	/* Copied: openlog() keeps the pointer it is given, and callers may
	 * pass a temporary buffer. */
	snprintf(logger.id, sizeof(logger.id), "%s", (id != NULL) ? id : "");
	logger.file = NULL;
	logger.initialized = 1;

	if (logger.backend == LOG_BACKEND_SYSLOG)
		openlog(logger.id[0] ? logger.id : NULL, LOG_CONS | LOG_PID, LOG_DAEMON);
	else if (logger.backend == LOG_BACKEND_FILE) {
		const char *path = logger.id[0] ? logger.id : "todo.log";
		logger.file = fopen(path, "a");
		if (logger.file == NULL) {
			logger.initialized = 0;
		}
	}
}

void logger_close(void)
{
	if (!logger.initialized)
		return;
	if (logger.backend == LOG_BACKEND_SYSLOG)
		closelog();
	else if (logger.backend == LOG_BACKEND_FILE && logger.file != NULL) {
		fclose(logger.file);
		logger.file = NULL;
	}
	logger.initialized = 0;
}

bool logger_debug_enabled(void)
{
	return logger.initialized && logger.level >= LOG_LVL_DEBUG;
}

void logger_log(log_level_t level, const char *fmt, ...)
{
	if (!logger.initialized)
		return;
	if (level > logger.level)
		return;

	va_list ap;
	va_start(ap, fmt);

	if (logger.backend == LOG_BACKEND_SYSLOG) {

		int pri;
		switch (level) {
			case LOG_LVL_ERROR: pri = LOG_ERR;     break;
			case LOG_LVL_WARN:  pri = LOG_WARNING; break;
			case LOG_LVL_INFO:  pri = LOG_INFO;    break;
			default:            pri = LOG_DEBUG;   break;
		}

		vsyslog(pri, fmt, ap);

	} else if (logger.backend == LOG_BACKEND_FILE) {
		if (logger.file == NULL) {
			va_end(ap);
			return;
		}
		char ts[32] = {0};
		time_t now = time(NULL);
		struct tm tm_now;
		if (localtime_r(&now, &tm_now) != NULL) {
			strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);
		}
		fprintf(logger.file, "%s [%s]: ",
			ts[0] ? ts : "0000-00-00 00:00:00",
			level_name(level));
		vfprintf(logger.file, fmt, ap);
		fputc('\n', logger.file);
		fflush(logger.file);
	} else {
		FILE *out = (level <= LOG_LVL_WARN) ? stderr : stdout;
		fprintf(out, "%s [%s]: ", logger.id, level_name(level));
		vfprintf(out, fmt, ap);
		fputc('\n', out);
		fflush(out);
	}

	va_end(ap);
}
