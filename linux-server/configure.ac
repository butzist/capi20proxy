#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([capi20proxy], [0.6.3], [friedrich@butzisten.de adam@butzisten.de])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/appindex.c])
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([capi20], [capi20_waitformessage],, [AC_MSG_ERROR([libcapi20 not installed or wrong version. \
			please update your isdn4k-utils(-devel) package!])])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h capi20.h errno.h fcntl.h linux/capi.h linux/kernelcapi.h netinet/in.h signal.h stdlib.h string.h sys/socket.h sys/time.h sys/stat.h syslog.h time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_DECLS([sys_siglist])

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([inet_ntoa memset socket])
AC_CONFIG_FILES([Makefile doc/Makefile src/Makefile contrib/Makefile])
AC_OUTPUT
