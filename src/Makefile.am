bin_PROGRAMS = todo

DOMAIN_SOURCES = \
	task_model.c \
	confirm.c \
	project_resolve.c \
	task.c \
	project.c \
	storage.c \
	strbuf.c \
	export.c \
	report.c

UI_LOGIC_SOURCES = \
	ui_layout.c \
	ui_state.c \
	notes_editor.c \
	input_dispatch.c

UI_NCURSES_SOURCES = \
	ui_draw.c \
	app_main.c

todo_SOURCES = \
	main.c logger.c \
	${DOMAIN_SOURCES} \
	${UI_LOGIC_SOURCES} \
	${UI_NCURSES_SOURCES}

CLEANFILES = \
	*.gcda \
	*.gcno \
	*.gcov \
	tests/*.gcda \
	tests/*.gcno \
	tests/*.gcov

COMMON_INCLUDES = -Iinclude

todo_CFLAGS = ${COMMON_INCLUDES} ${SQLITE3_CFLAGS} ${NCURSESW_CFLAGS} \
	${WARN_CFLAGS} -std=gnu11 -D_DEFAULT_SOURCE
todo_LDADD = ${SQLITE3_LIBS} ${NCURSESW_LIBS}

if ENABLE_TESTS
# noinst_PROGRAMS: built by `make all` (and thus by `make tests`, below) but
# never installed by `make install` - these are Unity test binaries, not
# part of the application.
noinst_PROGRAMS = testlogger \
	testtask_model testconfirm testproject_resolve testtask testproject teststorage \
	testui_layout testui_state testnotes_editor testinput_dispatch testexport \
	testreport

UNITY_SOURCES = \
	tests/unity.c \
	tests/include/unity/unity.h \
	tests/include/unity/unity_internals.h

TEST_CFLAGS = ${COMMON_INCLUDES} -Itests/include -Itests/include/unity \
	${WARN_CFLAGS} -std=gnu11 -D_DEFAULT_SOURCE

# Test binaries compile only the module under test plus what it depends on
# (not main.c), so each stays small and coverage is attributed per module.
testlogger_SOURCES = \
	${UNITY_SOURCES} \
	tests/logger_tests.c \
	logger.c
testlogger_CFLAGS = ${TEST_CFLAGS}

testtask_model_SOURCES = \
	${UNITY_SOURCES} \
	tests/task_model_tests.c \
	logger.c task_model.c
testtask_model_CFLAGS = ${TEST_CFLAGS}

testconfirm_SOURCES = \
	${UNITY_SOURCES} \
	tests/confirm_tests.c \
	logger.c confirm.c
testconfirm_CFLAGS = ${TEST_CFLAGS}

testproject_resolve_SOURCES = \
	${UNITY_SOURCES} \
	tests/project_resolve_tests.c \
	logger.c task_model.c storage.c project_resolve.c
testproject_resolve_CFLAGS = ${TEST_CFLAGS} ${SQLITE3_CFLAGS}
testproject_resolve_LDADD = ${SQLITE3_LIBS}

testtask_SOURCES = \
	${UNITY_SOURCES} \
	tests/task_tests.c \
	logger.c task_model.c storage.c task.c
testtask_CFLAGS = ${TEST_CFLAGS} ${SQLITE3_CFLAGS}
testtask_LDADD = ${SQLITE3_LIBS}

testproject_SOURCES = \
	${UNITY_SOURCES} \
	tests/project_tests.c \
	logger.c task_model.c storage.c project_resolve.c project.c task.c
testproject_CFLAGS = ${TEST_CFLAGS} ${SQLITE3_CFLAGS}
testproject_LDADD = ${SQLITE3_LIBS}

teststorage_SOURCES = \
	${UNITY_SOURCES} \
	tests/storage_tests.c \
	logger.c task_model.c storage.c
teststorage_CFLAGS = ${TEST_CFLAGS} ${SQLITE3_CFLAGS}
teststorage_LDADD = ${SQLITE3_LIBS}

testexport_SOURCES = \
	${UNITY_SOURCES} \
	tests/export_tests.c \
	logger.c task_model.c storage.c task.c strbuf.c export.c
testexport_CFLAGS = ${TEST_CFLAGS} ${SQLITE3_CFLAGS}
testexport_LDADD = ${SQLITE3_LIBS}

testreport_SOURCES = \
	${UNITY_SOURCES} \
	tests/report_tests.c \
	logger.c task_model.c storage.c strbuf.c report.c
testreport_CFLAGS = ${TEST_CFLAGS} ${SQLITE3_CFLAGS}
testreport_LDADD = ${SQLITE3_LIBS}

testui_layout_SOURCES = \
	${UNITY_SOURCES} \
	tests/ui_layout_tests.c \
	logger.c ui_layout.c
testui_layout_CFLAGS = ${TEST_CFLAGS}

testui_state_SOURCES = \
	${UNITY_SOURCES} \
	tests/ui_state_tests.c \
	logger.c task_model.c confirm.c ui_state.c
testui_state_CFLAGS = ${TEST_CFLAGS}

# Only the pure tmpfile read/write helpers are exercised; initscr() is never
# called, so no real terminal is required, but the ncurses symbols
# notes_editor.c references still need to resolve at link time.
testnotes_editor_SOURCES = \
	${UNITY_SOURCES} \
	tests/notes_editor_tests.c \
	logger.c notes_editor.c
testnotes_editor_CFLAGS = ${TEST_CFLAGS} ${NCURSESW_CFLAGS}
testnotes_editor_LDADD = ${NCURSESW_LIBS}

testinput_dispatch_SOURCES = \
	${UNITY_SOURCES} \
	tests/input_dispatch_tests.c \
	logger.c task_model.c confirm.c storage.c task.c project.c project_resolve.c \
	ui_layout.c ui_state.c input_dispatch.c
testinput_dispatch_CFLAGS = ${TEST_CFLAGS} ${SQLITE3_CFLAGS} ${NCURSESW_CFLAGS}
testinput_dispatch_LDADD = ${SQLITE3_LIBS} ${NCURSESW_LIBS}
endif
