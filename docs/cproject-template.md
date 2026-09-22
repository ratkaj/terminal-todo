# AGENTS.md

C project template (autotools + Unity tests + gcov/lcov coverage). The
placeholder name is `todo`; `scripts/rename-project.sh <name>` replaces it
everywhere.

## Layout

- `configure.ac`, `Makefile.am` - autotools; `--enable-tests` and
  `--enable-coverage` switches. Root `Makefile.am` owns the `tests` and
  `coverage` targets.
- `src/Makefile.am` - the program plus one `test<module>` binary per module.
- `src/*.c`, `src/include/*.h` - implementation and public headers (Doxygen).
  `example.[ch]` is a throwaway sample module; `logger` and `common.h` are
  shared helpers.
- `src/tests/` - Unity framework (`unity.c`, `include/unity/`) and
  `<module>_tests.c` files.
- `docs/developer/` - `DEVELOPER_DOC_TEMPLATE.md`; per-module docs go in
  `modules/<module>.md`.

## Conventions

- C11, tabs, `RT_SUCCESS`/`RT_ERROR` return codes, `RETURN_ERR_IF` guards.
- Tests: `test_<module>_<behavior>()`, one behavior each; `setUp()` inits the
  logger to `/tmp/todo_<module>.log`, `tearDown()` closes it; `main()`
  uses `UNITY_BEGIN()`/`RUN_TEST()`/`UNITY_END()`. Cover happy path plus
  invalid args, boundaries, and failure paths.

## Adding a module `foo`

1. `src/include/foo.h` (Doxygen), `src/foo.c`.
2. `src/tests/foo_tests.c`.
3. `src/Makefile.am`: add `testfoo` to `bin_PROGRAMS` (inside
   `ENABLE_TESTS`), its `_SOURCES` and `_CFLAGS`; add `foo.c` to the program
   sources.
4. Root `Makefile.am`: add a block for `./src/testfoo` in `tests`.
5. `.gitignore`: add `src/testfoo`.
6. `docs/developer/modules/foo.md` from the template.

## Commands

    autoreconf -i && ./configure --enable-tests && make
    make -C src testfoo && ./src/testfoo     # single module
    make tests                               # all tests
    ./configure --enable-tests --enable-coverage && make coverage
                                             # report: coverage/index.html
