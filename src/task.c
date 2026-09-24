// lspdiag

#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <storage.h>
#include <task.h>

int task_create(int64_t project_id, int64_t parent_id, const char *title,
	priority_t priority, task_t *out)
{
	RETURN_ERR_IF(title == NULL || title[0] == '\0', "task_create: title is empty");
	RETURN_ERR_IF(strlen(title) >= TASK_TITLE_MAX, "task_create: title too long");
	RETURN_ERR_IF(task_model_validate_priority((int)priority) != RT_SUCCESS,
		"task_create: invalid priority %d", (int)priority);

	RETURN_ERR_IF(storage_task_insert(project_id, parent_id, title, priority, out) != RT_SUCCESS,
		"task_create: storage insert failed (nesting invariant or db error)");
	return RT_SUCCESS;
}

int task_update_fields(int64_t id, const char *title, const char *notes)
{
	RETURN_ERR_IF(title != NULL && (title[0] == '\0' || strlen(title) >= TASK_TITLE_MAX),
		"task_update_fields: invalid title");
	return storage_task_update_fields(id, title, notes);
}

int task_set_priority(int64_t id, priority_t new_priority)
{
	RETURN_ERR_IF(task_model_validate_priority((int)new_priority) != RT_SUCCESS,
		"task_set_priority: invalid priority %d", (int)new_priority);
	return storage_task_set_priority(id, new_priority);
}

int task_set_completed(int64_t id, bool completed, bool confirmed_cascade,
	int *out_subtask_count)
{
	int subtasks = storage_task_has_subtasks(id);
	RETURN_ERR_IF(subtasks < 0, "task_set_completed: has_subtasks query failed");
	if (out_subtask_count != NULL)
		*out_subtask_count = subtasks;

	RETURN_ERR_IF(subtasks > 0 && !confirmed_cascade,
		"task_set_completed: %d subtasks need confirmation first", subtasks);
	return storage_task_set_completed(id, completed, subtasks > 0);
}

int task_delete(int64_t id)
{
	return storage_task_delete_cascade(id);
}

int task_move_to_project(int64_t id, int64_t dest_project_id)
{
	task_t t;
	RETURN_ERR_IF(storage_task_get(id, &t) != RT_SUCCESS, "task_move_to_project: task not found");
	bool movable = (t.parent_id == 0 && !t.archived && t.project_id != dest_project_id);
	task_model_free(&t);
	RETURN_ERR_IF(!movable,
		"task_move_to_project: only non-archived top-level tasks move, to another project");

	project_t dest;
	RETURN_ERR_IF(storage_project_get(dest_project_id, &dest) != RT_SUCCESS,
		"task_move_to_project: destination project not found");
	bool dest_archived = dest.archived;
	project_model_free(&dest);
	RETURN_ERR_IF(dest_archived, "task_move_to_project: destination project is archived");

	return storage_task_move_project(id, dest_project_id);
}

int task_clear_notes(int64_t id)
{
	return storage_task_clear_notes(id);
}

int task_reorder_step(int64_t id, int direction)
{
	return storage_task_reorder_move(id, direction);
}

int task_archive_completed(int64_t project_id, int *out_count, bool confirmed)
{
	return storage_task_archive_completed(project_id, out_count, confirmed);
}

int task_restore(int64_t id)
{
	return storage_task_restore(id);
}

int task_get_visible_row(int64_t project_id, bool include_archived,
	int index, task_t *out)
{
	RETURN_ERR_IF(out == NULL || index < 0, "task_get_visible_row: invalid arguments");

	task_t *arr = NULL;
	size_t n = 0;
	RETURN_ERR_IF(task_list_visible_rows(project_id, include_archived, &arr, &n) != RT_SUCCESS,
		"task_get_visible_row: list failed");

	int rc = RT_ERROR;
	if ((size_t)index < n) {
		*out = arr[index];
		arr[index].notes = NULL; /* ownership of notes transferred to *out */
		rc = RT_SUCCESS;
	}
	storage_task_array_free(arr, n);
	return rc;
}

int task_list_visible_rows(int64_t project_id, bool include_archived,
	task_t **out_arr, size_t *out_n)
{
	RETURN_ERR_IF(out_arr == NULL || out_n == NULL,
		"task_list_visible_rows: invalid arguments");

	task_t *top = NULL;
	size_t top_n = 0;
	RETURN_ERR_IF(storage_task_list_top_level(project_id, include_archived, &top, &top_n)
			!= RT_SUCCESS,
		"task_list_visible_rows: top-level query failed");

	size_t cap = (top_n > 0 ? top_n * 2 : 4);
	task_t *result = malloc(cap * sizeof(*result));
	if (result == NULL) {
		LERR("task_list_visible_rows: out of memory");
		storage_task_array_free(top, top_n);
		return RT_ERROR;
	}
	size_t n = 0;

	for (size_t i = 0; i < top_n; i++) {
		if (n == cap) {
			cap *= 2;
			task_t *tmp = realloc(result, cap * sizeof(*result));
			if (tmp == NULL) {
				LERR("task_list_visible_rows: out of memory");
				free(result);
				storage_task_array_free(top, top_n);
				return RT_ERROR;
			}
			result = tmp;
		}
		/* Shallow copy: ownership of top[i].notes transfers into result[]. */
		result[n++] = top[i];

		task_t *subs = NULL;
		size_t subs_n = 0;
		if (storage_task_list_subtasks(top[i].id, include_archived, &subs, &subs_n)
				== RT_SUCCESS) {
			for (size_t j = 0; j < subs_n; j++) {
				if (n == cap) {
					cap *= 2;
					task_t *tmp = realloc(result, cap * sizeof(*result));
					if (tmp == NULL) {
						LERR("task_list_visible_rows: out of memory");
						free(result);
						free(subs);
						storage_task_array_free(top, top_n);
						return RT_ERROR;
					}
					result = tmp;
				}
				result[n++] = subs[j];
			}
			free(subs); /* shallow: notes ownership already transferred above */
		}
	}
	free(top); /* shallow: notes ownership already transferred into result[] */

	*out_arr = result;
	*out_n = n;
	return RT_SUCCESS;
}

int task_find_visible_index(int64_t project_id, bool include_archived, int64_t task_id)
{
	task_t *arr = NULL;
	size_t n = 0;
	if (task_list_visible_rows(project_id, include_archived, &arr, &n) != RT_SUCCESS)
		return -1;

	int idx = -1;
	for (size_t i = 0; i < n; i++) {
		if (arr[i].id == task_id) {
			idx = (int)i;
			break;
		}
	}
	storage_task_array_free(arr, n);
	return idx;
}
