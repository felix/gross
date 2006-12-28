AC_INIT([gross],[0.6.3-svn])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([config.h])

CONFIGURE_LINE="$0 $*"

dnl AC_PROG_MAKE_SET
AC_PROG_CC([cc gcc])
AC_PROG_INSTALL

dnl this will disable C++ and F77 checking 
dnl there is a chance that it'll break something in
dnl the future, but this will do for now
AC_DEFUN([GROSS_CLEAR_LT_TAGCONFIG],
    [m4_define([_LT_AC_TAGCONFIG], [])]
)
GROSS_CLEAR_LT_TAGCONFIG
AC_PROG_LIBTOOL

CPPFLAGS="$CPPFLAGS -I/usr/local/include"
LDFLAGS="$LDFLAGS -L/usr/local/lib"

case ${target_os} in
    linux*)
	SYS=linux
	REENTRANT_FLAG="-D_REENTRANT"
	;;
    darwin*)
	SYS=darwin
	REENTRANT_FLAG="-D_REENTRANT"
	;;
    solaris*)
	SYS=solaris
	REENTRANT_FLAG="-D_POSIX_PTHREAD_SEMANTICS"
	;;
    *bsd*)
	SYS=${target_os}
	REENTRANT_FLAG="-D_REENTRANT"
	;;
    *)
	AC_MSG_ERROR([Unsupported target OS ${target_os}])
	;;
esac

if test "$SYS" = "darwin"
then
    AC_DEFINE([USE_SEM_OPEN], [], [Use sem_open() instead of sem_init()])
else
    AC_SEARCH_LIBS(sem_init, [rt], ,
	AC_MSG_ERROR([Can't compile without semaphores.])
    )
fi

AC_SEARCH_LIBS(bind, [socket], , 
    AC_MSG_ERROR([Can't compile without BSD sockets.])
)

AC_CHECK_LIB(pthread, pthread_create, [LIBS="$LIBS -lpthread"],
    AC_MSG_ERROR([Can't compile without pthreads.])
)

AC_CHECK_LIB(m, pow)
AC_CHECK_LIB(nsl, inet_pton)
AC_CHECK_HEADERS(netinet/in.h)

AC_CHECK_FUNCS([clock_gettime])

AC_CHECK_DECLS([CLOCK_MONOTONIC, CLOCK_HIGHRES],,, [#include <time.h>])

AC_MSG_CHECKING([whether to disable dnsbl checking])
AC_ARG_ENABLE(dnsbl,
    AS_HELP_STRING([--disable-dnsbl], [Disable dnsbl checking]),
    [AC_MSG_RESULT([yes]) ; dnsbl="no"],
    [AC_MSG_RESULT([no]) ; dnsbl="yes"]
)

if test "$dnsbl" == "yes"
then
    AC_CHECK_HEADERS(ares.h,
	DNSBLSOURCES='dnsblc.o'
    )
    AC_SUBST([DNSBLSOURCES])
    AC_CHECK_LIB(cares, ares_init, ,
	AC_MSG_ERROR([c-ares library needed for dnsbl checking.])
    )
fi

dnl Determine which protocol should the server (and client) use

AC_DEFINE([POSTFIX], [0], [Postfix])
AC_DEFINE([SJSMS], [1], [Sun Java System Messaging Server])
AM_CONDITIONAL(SJSMS, false)

AC_MSG_CHECKING([which protocol should the server use])
AC_ARG_WITH(worker,
    AS_HELP_STRING([--with-worker=<protocol>], [Select protocol for worker (default=postfix)]),
    [AC_MSG_RESULT([$withval]) ; workerproto="$withval"],
    [AC_MSG_RESULT([postfix]) ; workerproto="postfix"]
)

case "$workerproto" in
"postfix")
    PROTOCOL='postfix'
    AC_SUBST([PROTOCOL])
    AC_DEFINE([PROTOCOL], [POSTFIX], [Postfix])
    ;;
"sjsms")
    PROTOCOL='sjsms'
    AC_SUBST([PROTOCOL])
    AC_DEFINE([PROTOCOL], [SJSMS], [Sun Java System Messaging Server])
    AM_CONDITIONAL(SJSMS, true)
    ;;
*)
    AC_MSG_ERROR([unknown worker protocol: "$workerproto"])
    ;;
esac

AC_CHECK_TYPES(useconds_t)

AC_HEADER_STDC
AC_C_CONST
AC_TYPE_SIGNAL

AC_SUBST(REENTRANT_FLAG)

dnl various defines used by gross
dnl
dnl default config file location
if test "x$prefix" == xNONE; then
    prefix=$ac_default_prefix
fi
AC_DEFINE_UNQUOTED([CONFIGFILE], "`eval echo $sysconfdir/grossd.conf`", [Default config file])
AC_DEFINE_UNQUOTED([CONFIGURE_LINE], "${CONFIGURE_LINE}", [./configure command and args])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
