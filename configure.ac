AC_PREREQ(2.61)
AC_INIT([jsonperfmon], [0.1.0], [philippe.duveau@free.fr])
AC_CONFIG_SRCDIR([src/jsonperf.c])

AC_CANONICAL_HOST
case "${host}" in
  *-*-linux*)
    os_type="linux"
    AC_DEFINE([OS_LINUX], [1], [Indicator for a Linux OS])
  ;;
  *-*-*darwin*|*-*-dragonfly*|*-*-freebsd*|*-*-netbsd*|*-*-openbsd*|*-*-kfreebsd*)
    os_type="bsd"
    AC_DEFINE([OS_BSD], [1], [Indicator for a BSD OS])
  ;;
  *-*-solaris*)
    os_type="solaris"
    AC_DEFINE([OS_SOLARIS], [1], [Indicator for a Solaris OS])
  ;;
  *-apple-*)
    os_type="apple"
    AC_DEFINE([OS_APPLE], [1], [Indicator for APPLE OS])
  ;;
  *-*-aix*)
    os_type="aix"
    AC_DEFINE([OS_AIX], [1], [Indicator for a AIX OS])
    # Check if cc is the XLC legacy mode, if so add the right option to handle inline functions
    if [ cc -qversion 2>&1 1>/dev/null ]; 
    then
    	CFLAGS="${CFLAGS} -qkeyword=inline"
    fi
  ;;
esac

AM_CONDITIONAL(OS_LINUX, test x$os_type == xlinux)
AM_CONDITIONAL(OS_BSD, test x$os_type == xbsd)
AM_CONDITIONAL(OS_SOLARIS, test x$os_type == xsolaris)
AM_CONDITIONAL(OS_APPLE, test x$os_type == xapple)
AM_CONDITIONAL(OS_AIX, test x$os_type == xaix)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

if test "$GCC" = "yes"; then
	my_CFLAGS="-W -Wall -Wformat-security -Wshadow -Wcast-align -Wpointer-arith -Wmissing-format-attribute -g"
else
	my_CFLAGS=""
fi
AC_SUBST([my_CFLAGS])

AC_PROG_LIBTOOL

LT_INIT

AC_SUBST(rt_libs)

AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_FILES([Makefile \
		src/Makefile \
		])
AC_OUTPUT
AC_CONFIG_MACRO_DIR([m4])


echo "****************************************"
echo "jsonperfmon is configured to be compiled"
echo