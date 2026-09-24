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
		missing \
		test-driver

# `make tests` is kept as a shorthand for the standard `make check`.
tests: check

if ENABLE_COVERAGE
# `clean` forces every object to be rebuilt under the current (coverage-
# instrumented) CFLAGS. Without it, objects left over from a prior
# non-coverage build stay stale and uninstrumented, which starves lcov's
# --remove step of any '*/unity.c' .gcda data to match - and lcov >=2 treats
# an unmatched --remove pattern as a hard error rather than a no-op.
coverage: coverage-clean clean all
	$(MAKE) check || true
	lcov --directory ./src/ --capture --output-file coverage.info
	lcov --remove coverage.info '*/unity.c' --output-file coverage.info --ignore-errors unused
	genhtml coverage.info --output-directory coverage
	@echo "Coverage report: coverage/index.html"

coverage-clean:
	rm -rf coverage coverage.info
endif
