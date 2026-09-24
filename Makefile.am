SUBDIRS = src

.PHONY: coverage coverage-clean tests

clean-local:
	rm -rf coverage coverage.info

distclean-local:
	rm -rf src/config.status config.status

maintainer-clean-local:
	rm -rf \
		aclocal.m4 \
		compile \
		config.status \
		configure~ \
		depcomp \
		Makefile Makefile.in \
		src/Makefile src/Makefile.in \
		autom4te.cache \
		config.log \
		configure \
		install-sh \
		missing

if ENABLE_TESTS
# One block per test binary; keep in sync with noinst_PROGRAMS in src/Makefile.am.
tests: all
	@rm -fv /tmp/todo_*.log
	@echo
	@echo "===== TESTING LOGGER ====="
	@./src/testlogger
	@echo
	@echo "===== TESTING TASK_MODEL ====="
	@./src/testtask_model
	@echo
	@echo "===== TESTING CONFIRM ====="
	@./src/testconfirm
	@echo
	@echo "===== TESTING PROJECT_RESOLVE ====="
	@./src/testproject_resolve
	@echo
	@echo "===== TESTING TASK ====="
	@./src/testtask
	@echo
	@echo "===== TESTING PROJECT ====="
	@./src/testproject
	@echo
	@echo "===== TESTING STORAGE ====="
	@./src/teststorage
	@echo
	@echo "===== TESTING UI_LAYOUT ====="
	@./src/testui_layout
	@echo
	@echo "===== TESTING UI_STATE ====="
	@./src/testui_state
	@echo
	@echo "===== TESTING NOTES_EDITOR ====="
	@./src/testnotes_editor
	@echo
	@echo "===== TESTING EXPORT ====="
	@./src/testexport
	@echo
	@echo "===== TESTING REPORT ====="
	@./src/testreport
	@echo
	@echo "===== TESTING INPUT_DISPATCH ====="
	@./src/testinput_dispatch
endif

if ENABLE_COVERAGE
# `clean` forces every object to be rebuilt under the current (coverage-
# instrumented) CFLAGS. Without it, objects left over from a prior
# non-coverage build stay stale and uninstrumented, which starves lcov's
# --remove step of any '*/unity.c' .gcda data to match - and lcov >=2 treats
# an unmatched --remove pattern as a hard error rather than a no-op.
coverage: coverage-clean clean all
	$(MAKE) tests || true
	lcov --directory ./src/ --capture --output-file coverage.info
	lcov --remove coverage.info '*/unity.c' --output-file coverage.info --ignore-errors unused
	genhtml coverage.info --output-directory coverage
	@echo "Coverage report: coverage/index.html"

coverage-clean:
	rm -rf coverage coverage.info
endif
