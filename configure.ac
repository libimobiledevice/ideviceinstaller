#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(ideviceinstaller, 1.1.1, nospam@nowhere.com)
AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip check-news])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

# Checks for libraries.
PKG_CHECK_MODULES(libimobiledevice, libimobiledevice-1.0 >= 0.9.7)
PKG_CHECK_MODULES(libimobiledevice100, libimobiledevice-1.0 >= 1.0.0, libimobiledevice_1_0=yes, libimobiledevice_1_0=no)
if test x"$libimobiledevice_1_0" = xyes; then
  AC_DEFINE([HAVE_LIBIMOBILEDEVICE_1_0], 1, [Define if libimobiledevice is using 1.0.0 API])
fi
PKG_CHECK_MODULES(libimobiledevice110, libimobiledevice-1.0 >= 1.1.0, libimobiledevice_1_1=yes, libimobiledevice_1_1=no)
if test x"$libimobiledevice_1_1" = xyes; then
  AC_DEFINE([HAVE_LIBIMOBILEDEVICE_1_1], 1, [Define if libimobiledevice is using 1.1.0 API])
fi
PKG_CHECK_MODULES(libimobiledevice112, libimobiledevice-1.0 >= 1.1.2, libimobiledevice_1_2=yes, libimobiledevice_1_2=no)
if test x"$libimobiledevice_1_2" = xno; then
  PKG_CHECK_MODULES(libglib2, glib-2.0 >= 2.14.1)
fi
PKG_CHECK_MODULES(libimobiledevice115, libimobiledevice-1.0 >= 1.1.5, libimobiledevice_1_1_5=yes, libimobiledevice_1_1_5=no)
if test x"$libimobiledevice_1_1_5" = xyes; then
  AC_DEFINE([HAVE_LIBIMOBILEDEVICE_1_1_5], 1, [Define if libimobiledevice is using 1.1.5 API])
fi
PKG_CHECK_MODULES(libplist, libplist >= 0.15)
PKG_CHECK_MODULES(libzip, libzip >= 0.8)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([strcasecmp strdup strerror strndup])

# Check for lstat

AC_MSG_CHECKING([whether lstat is available])
AC_TRY_LINK([
#include <sys/types.h>
#include <sys/stat.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
],[
struct stat st;
lstat("/tmp", &st);
], [have_lstat="yes"], [have_lstat="no"])
AC_MSG_RESULT([${have_lstat}])

if test "x${have_lstat}" = "xyes" ; then
  AC_DEFINE([HAVE_LSTAT], 1, [Define if lstat syscall is supported])
fi

AS_COMPILER_FLAGS(GLOBAL_CFLAGS, "-Wall -Wextra -Wmissing-declarations -Wredundant-decls -Wshadow -Wpointer-arith  -Wwrite-strings -Wswitch-default -Wno-unused-parameter -Werror -g")
AC_SUBST(GLOBAL_CFLAGS)

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_OUTPUT([
Makefile
src/Makefile
man/Makefile
])

