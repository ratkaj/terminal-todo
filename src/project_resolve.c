// lspdiag

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common.h>
#include <project_resolve.h>
#include <storage.h>

#define PATH_BUF_MAX 4096

static int find_builtin_project(const char *name, project_t *out)
{
	project_t *arr = NULL;
	size_t n = 0;
	RETURN_ERR_IF(storage_project_list(true, &arr, &n) != RT_SUCCESS,
		"find_builtin_project: list failed");

	int rc = RT_ERROR;
	for (size_t i = 0; i < n; i++) {
		if (arr[i].builtin && strcmp(arr[i].display_name, name) == 0) {
			*out = arr[i];
			out->canonical_path = NULL; /* built-ins never have a path */
			rc = RT_SUCCESS;
			break;
		}
	}
	storage_project_array_free(arr, n);
	RETURN_ERR_IF(rc != RT_SUCCESS, "find_builtin_project: '%s' not found", name);
	return RT_SUCCESS;
}

int project_resolve_cwd(const char *cwd, const char *home,
	resolve_kind_t *out_kind, project_t *out_project,
	char *out_provisional_path, size_t path_cap)
{
	RETURN_ERR_IF(cwd == NULL || home == NULL || out_kind == NULL,
		"project_resolve_cwd: invalid arguments");

	if (strcmp(cwd, home) == 0) {
		*out_kind = RESOLVE_HOME_TODAY;
		if (out_project != NULL)
			RETURN_ERR_IF(find_builtin_project("Today", out_project) != RT_SUCCESS,
				"project_resolve_cwd: Today built-in not found");
		return RT_SUCCESS;
	}

	char path[PATH_BUF_MAX];
	int n = snprintf(path, sizeof(path), "%s", cwd);
	RETURN_ERR_IF(n < 0 || (size_t)n >= sizeof(path), "project_resolve_cwd: cwd too long");

	for (;;) {
		project_t found;
		if (storage_project_find_by_path(path, &found) == RT_SUCCESS) {
			*out_kind = RESOLVE_REGISTERED;
			if (out_project != NULL)
				*out_project = found;
			else
				project_model_free(&found);
			return RT_SUCCESS;
		}

		if (strcmp(path, "/") == 0)
			break;

		char *slash = strrchr(path, '/');
		if (slash == NULL || slash == path) {
			path[0] = '/';
			path[1] = '\0';
		} else {
			*slash = '\0';
		}
	}

	*out_kind = RESOLVE_PROVISIONAL;
	if (out_provisional_path != NULL) {
		int m = snprintf(out_provisional_path, path_cap, "%s", cwd);
		RETURN_ERR_IF(m < 0 || (size_t)m >= path_cap,
			"project_resolve_cwd: provisional path too long for buffer");
	}
	return RT_SUCCESS;
}

int project_resolve_current(resolve_kind_t *out_kind, project_t *out_project,
	char *out_provisional_path, size_t path_cap)
{
	char cwd_buf[PATH_BUF_MAX];
	RETURN_ERR_IF(getcwd(cwd_buf, sizeof(cwd_buf)) == NULL,
		"project_resolve_current: getcwd failed: %s", strerror(errno));

	char cwd_real[PATH_BUF_MAX];
	RETURN_ERR_IF(realpath(cwd_buf, cwd_real) == NULL,
		"project_resolve_current: realpath(%s) failed: %s", cwd_buf, strerror(errno));

	const char *home_env = getenv("HOME");
	RETURN_ERR_IF(home_env == NULL, "project_resolve_current: HOME is not set");

	char home_real[PATH_BUF_MAX];
	RETURN_ERR_IF(realpath(home_env, home_real) == NULL,
		"project_resolve_current: realpath(HOME=%s) failed: %s", home_env, strerror(errno));

	return project_resolve_cwd(cwd_real, home_real, out_kind, out_project,
		out_provisional_path, path_cap);
}
