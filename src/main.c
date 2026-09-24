// lspdiag

/**
 * @file main.c
 * @brief Program entry point for todo.
 */

#include <stdio.h>

#include <app_main.h>
#include <common.h>
#include <logger.h>

int main(void)
{
	static const char log_path[] = "/tmp/todo.log";

	logger_init(LOG_LVL_INFO, LOG_BACKEND_FILE, log_path);
	int rc = app_main_run();
	logger_close();
	if (rc != RT_SUCCESS)
		fprintf(stderr, "todo: details are in %s\n", log_path);
	return rc == RT_SUCCESS ? 0 : 1;
}
