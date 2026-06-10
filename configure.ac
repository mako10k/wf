AC_INIT([wf], [0.2.0])
AM_INIT_AUTOMAKE([foreign -Wall])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC

AC_CHECK_HEADERS([openssl/rand.h openssl/sha.h], [],
    [AC_MSG_ERROR([OpenSSL headers are required])])
AC_SEARCH_LIBS([SHA256], [crypto], [],
    [AC_MSG_ERROR([libcrypto is required])])
AC_SEARCH_LIBS([RAND_bytes], [crypto], [],
    [AC_MSG_ERROR([libcrypto with RAND_bytes is required])])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
