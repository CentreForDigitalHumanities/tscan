#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
AC_INIT([tscan], [0.2], [to-be-decided@ergens.nl])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([src/tscan.cxx])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX

# libtool stuff
AC_PROG_LIBTOOL
LT_INIT

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([mkdir])

OPENMP_CHECK( [CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"
	       AC_DEFINE(HAVE_OPENMP, 1 , Define to 1 if you have OpenMP )],
	      [AC_MSG_NOTICE(We don't have OpenMP. Multithreaded testing is disabled)] )

AC_SUBST([OPENMPFLAGS],[$OPENMP_CXXFLAGS])

# ugly hack when PKG_CONFIG_PATH isn't defined.
# couldn't get it to work otherwise
if test "x$PKG_CONFIG_PATH" = x; then
     export PKG_CONFIG_PATH=""
fi
PKG_PROG_PKG_CONFIG

#AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )

AX_ICU_CHECK( [3.6],
	      [CXXFLAGS="$CXXFLAGS $ICU_CPPSEARCHPATH"
	       LIBS="$ICU_LIBPATH $ICU_LIBS $ICU_IOLIBS $LIBS"],
	      [AC_MSG_FAILURE( "No ICU development environment found. Please check if libicu-dev or the like is installed" )] )

AC_ARG_WITH(timblserver,
       [  --with-timblserver=DIR       use timblserver installed in <DIR>;
               note that you can install timbl in a non-default directory with
               ./configure --prefix=<DIR> in the timbl installation directory],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([timblserver], [timblserver >= 1.7] )
CXXFLAGS="$CXXFLAGS $timblserver_CFLAGS"
LIBS="$timblserver_LIBS $LIBS"


AC_ARG_WITH(ticcutils,
       [  --with-ticcutils=DIR       use ticcutils installed in <DIR>;
               note that you can install ticcutils in a non-default directory w
ith
               ./configure --prefix=<DIR> in the ticcutils installation directo
ry],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
#  AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([ticcutils], [ticcutils >= 0.4] )
CXXFLAGS="$CXXFLAGS $ticcutils_CFLAGS"
LIBS="$ticcutils_LIBS $LIBS"

PKG_CHECK_MODULES([XML2], [libxml-2.0 >= 2.6.16] )
CXXFLAGS="$CXXFLAGS $XML2_CFLAGS"
LIBS="$LIBS $XML2_LIBS"

AC_ARG_WITH(folia,
       [  --with-folia=DIR       use libfolia installed in <DIR>;
               note that you can install folia in a non-default directory with
               ./configure --prefix=<DIR> in the folia installation directory],
       [PKG_CONFIG_PATH="$withval/lib/pkgconfig:$PKG_CONFIG_PATH"],
       [PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$PKG_CONFIG_PATH"])
AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([folia], [folia >= 0.10] )
CXXFLAGS="$folia_CFLAGS $CXXFLAGS"
LIBS="$folia_LIBS $LIBS"



AC_CONFIG_FILES([Makefile
                 include/Makefile
                 include/tscan/Makefile
                 src/Makefile
		 data/Makefile
		 config/Makefile
                 docs/Makefile])
AC_OUTPUT
