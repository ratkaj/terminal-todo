AC_INIT([todo], 1.00)

# This is a single-user personal tool, not something distros package, so
# default to a per-user, no-sudo install location instead of autoconf's
# usual /usr/local. `./configure --prefix=...` still overrides this.
AC_PREFIX_DEFAULT([$HOME/.local])

AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_FILES([
                 Makefile
                 src/Makefile
                 ])
AC_PROG_CC

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([NCURSESW], [ncursesw])
PKG_CHECK_MODULES([SQLITE3], [sqlite3])
NCURSESW_CFLAGS="$NCURSESW_CFLAGS -D_XOPEN_SOURCE=600"

# -Werror is opt-in: a newer compiler than the one this was developed on
# can add warnings, and that should not break a user's build from source.
AC_ARG_ENABLE([werror],
	AS_HELP_STRING([--enable-werror], [treat compiler warnings as errors]),
	[enable_werror=yes],
	[enable_werror=no])
WARN_CFLAGS="-Wall -Wextra -Wpedantic"
if test "x$enable_werror" = "xyes"; then
  WARN_CFLAGS="$WARN_CFLAGS -Werror"
fi
AC_SUBST([WARN_CFLAGS])

AC_ARG_ENABLE([tests],
	AS_HELP_STRING([--enable-tests], [build the unit test binaries]),
	[enable_tests=yes],
	[enable_tests=no])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" = "xyes"])

AC_ARG_ENABLE([coverage],
	AS_HELP_STRING([--enable-coverage], [enable gcov code coverage]),
	[enable_coverage=yes],
	[enable_coverage=no])
AM_CONDITIONAL([ENABLE_COVERAGE], [test "x$enable_coverage" = "xyes"])

if test "x$enable_coverage" = "xyes"; then
  CFLAGS="-O0 -g --coverage"
  LDFLAGS="$LDFLAGS --coverage"
fi

AC_ARG_ENABLE([sanitize],
	AS_HELP_STRING([--enable-sanitize], [build with ASan/UBSan instrumentation]),
	[enable_sanitize=yes],
	[enable_sanitize=no])
AM_CONDITIONAL([ENABLE_SANITIZE], [test "x$enable_sanitize" = "xyes"])

if test "x$enable_sanitize" = "xyes"; then
  CFLAGS="$CFLAGS -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
  LDFLAGS="$LDFLAGS -fsanitize=address,undefined"
fi

AC_OUTPUT
