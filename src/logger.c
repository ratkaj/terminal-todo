#include <stdio.h>
#include <stdarg.h>
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

static struct {
	log_level_t level;
	log_backend_t backend;
	const char *id;
	FILE *file;
	int initialized;
} logger;


void logger_init(log_level_t level, log_backend_t backend, const char *id)
{
	logger.level = level;
	logger.backend = backend;
	logger.id = id;
	logger.file = NULL;
	logger.initialized = 1;

	if (logger.backend == LOG_BACKEND_SYSLOG)
		openlog(logger.id, LOG_CONS | LOG_PID, LOG_DAEMON);
	else if (logger.backend == LOG_BACKEND_FILE) {
		const char *path = (logger.id != NULL) ? logger.id : "todo.log";
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
			level_str[level]);
		vfprintf(logger.file, fmt, ap);
		fflush(logger.file);
	} else {
		FILE *out = (level <= LOG_LVL_WARN) ? stderr : stdout;
		fprintf(out, "%s [%s]: ", logger.id, level_str[level]);
		vfprintf(out, fmt, ap);
		// fprintf(out, "\n");
		fflush(out);
	}

	va_end(ap);
}
