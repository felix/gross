dnl $Id$

AC_INIT([gross],[svn])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

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
AC_PROG_SED
AC_PATH_PROG(POD2MAN, pod2man)
AM_CONDITIONAL(HAVE_POD2MAN, test x$POD2MAN != x)

SED="$lt_cv_path_SED"
AC_SUBST([SED])
AM_CONDITIONAL(HAVE_SED, test x$SED != x)

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

AC_CHECK_TYPE([bool])

AC_CHECK_DECLS([CLOCK_MONOTONIC, CLOCK_HIGHRES],,, [#include <time.h>])

AC_MSG_CHECKING([whether to disable dnsbl checking])
AC_ARG_ENABLE(dnsbl,
    AC_HELP_STRING([--disable-dnsbl], [Disable dnsbl checking]),
    [AC_MSG_RESULT([yes]) ; dnsbl="no"],
    [AC_MSG_RESULT([no]) ; dnsbl="yes"]
)

if test "$dnsbl" == "yes"
then
    AC_CHECK_HEADERS(ares.h, 
	DNSBLSOURCES='check_dnsbl.o',
	AC_MSG_ERROR([ares.h needed for dnsbl checking.])
    )
    AC_SUBST([DNSBLSOURCES])
    AC_CHECK_LIB(cares, ares_init, ,
	AC_MSG_ERROR([c-ares library needed for dnsbl checking.])
    )
fi

AC_MSG_CHECKING([whether to enable milter])
AC_ARG_ENABLE(milter,
    AC_HELP_STRING([--enable-milter], [Enable milter]),
    [AC_MSG_RESULT([yes]) ; milter="yes"],
    [AC_MSG_RESULT([no]) ; milter="no"]
)

if test "$milter" == "yes"
then
    AC_CHECK_HEADERS(libmilter/mfapi.h,
	MILTERSOURCES='worker_milter.o',
	AC_MSG_ERROR([libmilter/mfapi.h needed for dnsbl checking.])
    )
    AC_SUBST([MILTERSOURCES])
    AC_CHECK_LIB(milter, smfi_main, ,
	AC_MSG_ERROR([libmilter library needed for milter support])
    )
fi

AC_MSG_CHECKING([whether to build check_spf])
AC_ARG_ENABLE(spf,
    AC_HELP_STRING([--enable-spf], [Enable spf check]),
    [AC_MSG_RESULT([yes]) ; spf="yes"],
    [AC_MSG_RESULT([no]) ; spf="no"]
)

if test "$spf" == "yes"
then
    AC_CHECK_HEADERS(arpa/nameser_compat.h)
    AC_CHECK_HEADERS(arpa/inet.h)
    AC_CHECK_HEADERS(spf2/spf.h,
	SPFSOURCES='check_spf.o',
	AC_MSG_ERROR(spf2/spf.h needed for spf checks.),
	[#include <arpa/nameser_compat.h>
	 #include <arpa/inet.h>
	 #include <time.h>
	])

    AC_SUBST([SPFSOURCES])
    AC_CHECK_LIB(spf2, SPF_request_new, ,
	AC_MSG_ERROR([libspf2 library needed for spf checks.])
    )
fi

AC_CHECK_TYPES(useconds_t)

dnl AC_HEADER_STDC
AC_C_CONST
AC_TYPE_SIGNAL

AC_SUBST(REENTRANT_FLAG)

if test "x$GCC" == "xyes"
then
	STATIC_GLIBC_FLAG="-XCClinker -static-libgcc"
else
	STATIC_GLIBC_FLAG=""
fi
AC_SUBST(STATIC_GLIBC_FLAG)

dnl various defines used by gross
dnl
dnl default config file location
if test "x$prefix" == xNONE; then
    prefix=$ac_default_prefix
fi
AC_DEFINE_UNQUOTED([CONFIGFILE], "`eval echo $sysconfdir/grossd.conf`", [Default config file])
AC_DEFINE_UNQUOTED([CONFIGURE_LINE], "${CONFIGURE_LINE}", [./configure command and args])

AC_CONFIG_FILES([Makefile src/Makefile man/Makefile])
AC_OUTPUT
