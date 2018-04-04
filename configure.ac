#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
AC_INIT([tscan], [0.9.1], [m.h.vanderklis@uu.nl]) #also change in webservice/setup.py
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/tscan.cxx])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX( [g++ c++] )

#force to C++ when running tests
AC_LANG([C++])

# libtool stuff
AC_PROG_LIBTOOL
LT_INIT

# Check for OpenMP support
AC_OPENMP
if test "x$ac_cv_prog_cxx_openmp" != "xunsupported"; then
   CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"
   AC_DEFINE(HAVE_OPENMP, 1 , Define to 1 if you have OpenMP )
else
   AC_MSG_NOTICE(We don't have OpenMP. Multithreaded operation is disabled)
fi

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([mkdir])

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

AC_ARG_WITH(ticcutils,
       [  --with-ticcutils=DIR       use ticcutils installed in <DIR>;
               note that you can install ticcutils in a non-default directory w
ith
               ./configure --prefix=<DIR> in the ticcutils installation directo
ry],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
#  AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([ticcutils], [ticcutils >= 0.18] )
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
PKG_CHECK_MODULES([folia], [folia >= 1.6] )
CXXFLAGS="$folia_CFLAGS $CXXFLAGS"
LIBS="$folia_LIBS $LIBS"


AC_ARG_WITH(frog,
       [  --with-frog=DIR       use libfrog installed in <DIR>;
               note that you can install frog in a non-default directory with
               ./configure --prefix=<DIR> in the frog installation directory],
       [PKG_CONFIG_PATH="$withval/lib/pkgconfig:$PKG_CONFIG_PATH"],
       [PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$PKG_CONFIG_PATH"])
AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([frog], [frog >= 0.13.6] )
CXXFLAGS="$frog_CFLAGS $CXXFLAGS"
LIBS="$frog_LIBS $LIBS"

AC_ARG_WITH(timbl,
       [  --with-timbl=DIR       use timbl installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([timbl], [timbl >= 6.4.4] )
CXXFLAGS="$CXXFLAGS $timbl_CFLAGS"
LIBS="$timbl_LIBS $LIBS"


AC_CONFIG_FILES([Makefile
                 include/Makefile
                 include/tscan/Makefile
                 src/Makefile
		 data/Makefile
		 webservice/Makefile
		 config/Makefile
                 docs/Makefile])
AC_OUTPUT
