// lspdiag

#include <stdlib.h>

#include <common.h>
#include <task_model.h>

int task_model_validate_priority(int v)
{
	RETURN_ERR_IF(v != PRIORITY_P1 && v != PRIORITY_P2 && v != PRIORITY_P3,
		"task_model_validate_priority: invalid priority %d", v);
	return RT_SUCCESS;
}

void task_model_free(task_t *t)
{
	if (t == NULL)
		return;
	free(t->notes);
	t->notes = NULL;
}

void project_model_free(project_t *p)
{
	if (p == NULL)
		return;
	free(p->canonical_path);
	p->canonical_path = NULL;
}
