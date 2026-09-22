// lspdiag

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <project.h>
#include <project_resolve.h>
#include <storage.h>
#include <task.h>

#define PROVISIONAL_PATH_BUF 4096

static void path_basename(const char *path, char *out, size_t out_cap)
{
	const char *slash = strrchr(path, '/');
	const char *base = (slash != NULL && slash[1] != '\0') ? slash + 1 : path;
	/* Bounded copy (not snprintf) so a basename longer than out_cap is
	   silently truncated for display without tripping -Wformat-truncation. */
	size_t len = strlen(base);
	if (len >= out_cap)
		len = out_cap - 1;
	memcpy(out, base, len);
	out[len] = '\0';
}

int project_create_explicit(const char *display_name, const char *canonical_path,
	project_t *out)
{
	RETURN_ERR_IF(display_name == NULL || display_name[0] == '\0',
		"project_create_explicit: empty display name");
	RETURN_ERR_IF(strlen(display_name) >= PROJECT_NAME_MAX,
		"project_create_explicit: display name too long");

	project_t p = {0};
	snprintf(p.display_name, sizeof(p.display_name), "%s", display_name);
	p.canonical_path = canonical_path ? strdup(canonical_path) : NULL;

	int64_t id = 0;
	int rc = storage_project_insert(&p, &id);
	free(p.canonical_path);
	RETURN_ERR_IF(rc != RT_SUCCESS, "project_create_explicit: storage insert failed");

	if (out != NULL)
		return storage_project_get(id, out);
	return RT_SUCCESS;
}

int project_resolve_or_provisional(project_t *out, bool *out_is_provisional)
{
	RETURN_ERR_IF(out == NULL, "project_resolve_or_provisional: out is NULL");

	resolve_kind_t kind;
	char provisional_path[PROVISIONAL_PATH_BUF];
	project_t resolved = {0};
	RETURN_ERR_IF(project_resolve_current(&kind, &resolved, provisional_path,
			sizeof(provisional_path)) != RT_SUCCESS,
		"project_resolve_or_provisional: resolution failed");

	if (kind == RESOLVE_PROVISIONAL) {
		memset(out, 0, sizeof(*out));
		out->id = 0;
		path_basename(provisional_path, out->display_name, sizeof(out->display_name));
		out->canonical_path = strdup(provisional_path);
		out->archived = false;
		out->builtin = false;
		if (out_is_provisional != NULL)
			*out_is_provisional = true;
		return RT_SUCCESS;
	}

	*out = resolved;
	if (out_is_provisional != NULL)
		*out_is_provisional = false;
	return RT_SUCCESS;
}

int project_commit_provisional_with_task(project_t *provisional, const char *title,
	priority_t prio, task_t *out_task)
{
	RETURN_ERR_IF(provisional == NULL || title == NULL,
		"project_commit_provisional_with_task: invalid arguments");
	RETURN_ERR_IF(provisional->id != 0,
		"project_commit_provisional_with_task: project is already persisted");

	RETURN_ERR_IF(storage_begin() != RT_SUCCESS,
		"project_commit_provisional_with_task: begin failed");

	int64_t project_id = 0;
	if (storage_project_insert(provisional, &project_id) != RT_SUCCESS) {
		storage_rollback();
		return RT_ERROR;
	}

	if (task_create(project_id, 0, title, prio, out_task) != RT_SUCCESS) {
		storage_rollback();
		return RT_ERROR;
	}

	RETURN_ERR_IF(storage_commit() != RT_SUCCESS,
		"project_commit_provisional_with_task: commit failed");
	provisional->id = project_id;
	return RT_SUCCESS;
}

int project_rename(int64_t id, const char *new_display_name)
{
	RETURN_ERR_IF(new_display_name == NULL || new_display_name[0] == '\0',
		"project_rename: empty name");
	RETURN_ERR_IF(strlen(new_display_name) >= PROJECT_NAME_MAX,
		"project_rename: name too long");

	project_t p;
	RETURN_ERR_IF(storage_project_get(id, &p) != RT_SUCCESS, "project_rename: project not found");
	snprintf(p.display_name, sizeof(p.display_name), "%s", new_display_name);
	int rc = storage_project_update(&p);
	project_model_free(&p);
	return rc;
}

int project_archive(int64_t id)
{
	project_t p;
	RETURN_ERR_IF(storage_project_get(id, &p) != RT_SUCCESS, "project_archive: project not found");
	if (p.builtin) {
		project_model_free(&p);
		LERR("project_archive: cannot archive built-in project %lld", (long long)id);
		return RT_ERROR;
	}
	p.archived = true;
	int rc = storage_project_update(&p);
	project_model_free(&p);
	return rc;
}

int project_restore(int64_t id)
{
	project_t p;
	RETURN_ERR_IF(storage_project_get(id, &p) != RT_SUCCESS, "project_restore: project not found");
	p.archived = false;
	int rc = storage_project_update(&p);
	project_model_free(&p);
	return rc;
}

int project_delete_or_clear(int64_t id)
{
	project_t p;
	RETURN_ERR_IF(storage_project_get(id, &p) != RT_SUCCESS,
		"project_delete_or_clear: project not found");
	int rc = p.builtin ? storage_project_clear_tasks(id) : storage_project_delete_cascade(id);
	project_model_free(&p);
	return rc;
}

int project_find_index(bool include_archived, int64_t project_id)
{
	project_t *arr = NULL;
	size_t n = 0;
	if (storage_project_list(include_archived, &arr, &n) != RT_SUCCESS)
		return -1;

	int idx = -1;
	for (size_t i = 0; i < n; i++) {
		if (arr[i].id == project_id) {
			idx = (int)i;
			break;
		}
	}
	storage_project_array_free(arr, n);
	return idx;
}
