AC_PREREQ([2.69])
AC_INIT([netloadgraph], [0.1], [fatwildcat@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
GLIB_GSETTINGS

# Checks for libraries.
AC_CHECK_LIB([m], [log])
PKG_CHECK_MODULES(LIBGTK, [gtk+-3.0])

# Checks for header files.
AC_CHECK_HEADERS([limits.h stdlib.h string.h sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([atexit gettimeofday strchr strcspn strspn strstr strtoull])

AC_CONFIG_FILES([Makefile src/Makefile icons/Makefile debian/Makefile])
AC_OUTPUT
