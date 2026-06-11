AC_INIT([wf], [0.2.0])
AM_INIT_AUTOMAKE([foreign -Wall])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC

AC_CHECK_HEADERS([libintl.h locale.h])
AC_SEARCH_LIBS([bindtextdomain], [intl], [], [])
AC_CHECK_PROGS([MSGFMT], [msgfmt], [false])
AC_CHECK_PROGS([XGETTEXT], [xgettext], [false])
AC_CHECK_PROGS([MSGMERGE], [msgmerge], [false])

AS_IF([test "x$MSGFMT" = xfalse],
    [AC_MSG_WARN([msgfmt not found: make locale, make test, and translation install will fail until gettext is installed])])
AS_IF([test "x$XGETTEXT" = xfalse],
    [AC_MSG_WARN([xgettext not found: make pot will fail until gettext is installed])])
AS_IF([test "x$MSGMERGE" = xfalse],
    [AC_MSG_WARN([msgmerge not found: make update-po will fail until gettext is installed])])

AC_SUBST([MSGFMT])
AC_SUBST([XGETTEXT])
AC_SUBST([MSGMERGE])

AC_CHECK_HEADERS([openssl/rand.h openssl/sha.h], [],
    [AC_MSG_ERROR([OpenSSL headers are required])])
AC_SEARCH_LIBS([SHA256], [crypto], [],
    [AC_MSG_ERROR([libcrypto is required])])
AC_SEARCH_LIBS([RAND_bytes], [crypto], [],
    [AC_MSG_ERROR([libcrypto with RAND_bytes is required])])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
