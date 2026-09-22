// lspdiag

#include <string.h>

#include <confirm.h>

void confirm_state_init(confirm_state_t *cs)
{
	if (cs == NULL)
		return;
	memset(cs, 0, sizeof(*cs));
}

bool confirm_state_should_prompt(const confirm_state_t *cs, confirm_category_t cat)
{
	if (cs == NULL || cat < 0 || cat >= CONFIRM_CAT_COUNT)
		return true;
	return !cs->suppressed[cat];
}

void confirm_state_apply_answer(confirm_state_t *cs, confirm_category_t cat,
	char answer, bool *out_proceed)
{
	bool proceed = (answer == 'y' || answer == 'Y');
	if (cs != NULL && cat >= 0 && cat < CONFIRM_CAT_COUNT && answer == 'Y')
		cs->suppressed[cat] = true;
	if (out_proceed != NULL)
		*out_proceed = proceed;
}
