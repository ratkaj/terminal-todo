// lspdiag

/**
 * @file main.c
 * @brief Program entry point for todo.
 */

#include <app_main.h>
#include <common.h>
#include <logger.h>

int main(void)
{
	logger_init(LOG_LVL_INFO, LOG_BACKEND_FILE, "/tmp/todo.log");
	int rc = app_main_run();
	logger_close();
	return rc == RT_SUCCESS ? 0 : 1;
}
