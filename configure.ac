dnl        Copyright (C) 2006, 2014, 2017, 2018 Free Software Foundation, Inc
dnl Process this file with autoconf to produce a configure script.

AC_INIT(GNU units,2.21,adrianm@gnu.org)
AC_PREREQ(2.59)
AC_ARG_PROGRAM

AC_SUBST(DEFIS)
AC_SUBST(STRFUNC)
AC_SUBST(UDAT)
AC_SUBST(CDAT)
AC_SUBST(UDATADIR)
AC_SUBST(RELOCATION)
AC_SUBST(HAVE_PYTHON)
AC_SUBST(PYTHON_VERSION)
AC_SUBST(MKS_POSIX)
AC_SUBST(MKS_RES)
AC_SUBST(MKS_DRIVE)
AC_SUBST(HAVE_MKS)

RELOCATION=no

dnl Hack to set correct CFLAGS for cl compiler while still
dnl allowing user to override the CFLAGS setting

saveCFLAGS=$CFLAGS

AC_PROG_CC(gcc cc egcs cl.exe)
AC_PROG_CC_C99

case $OSTYPE in
    darwin*)
        echo detected MacOS, setting flags
        CPPFLAGS="$CPPFLAGS -I/opt/local/include"
        LDFLAGS="$LDFLAGS -L/opt/local/lib"
        ;;
    *) echo detected non-mac os: $OSTYPE
	
esac


AC_DEFUN([AX_CHECK_COMPILE_FLAG],
[AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_IF
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler accepts $1], CACHEVAR, [
  ax_check_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
  _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $4 $1"
  AC_COMPILE_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_COMPILE_FLAGS

if test "$CC" = cl.exe; then
  if test -z "$saveCFLAGS" ; then
    AC_MSG_NOTICE([setting special CFLAGS value for cl])
    CFLAGS="-O2 -W3 -D_CRT_SECURE_NO_WARNINGS -nologo"
  fi
  if test `expr "$CFLAGS" : '.*D_CRT_SECURE_NO_WARNINGS'` -eq 0; then
    AC_MSG_NOTICE([disabling CRT_SECURE warnings for cl])
    CFLAGS="$CFLAGS -D_CRT_SECURE_NO_WARNINGS"
  fi 
else
   dnl The -fpic option is needed with gcc if you want to compile a shared lib
   dnl and causes no harm for regular compilation.  But cl.exe accepts it
   dnl and then prints a warning.  
   AX_CHECK_COMPILE_FLAG(-fpic,[CFLAGS="$CFLAGS -fpic"])
fi


AC_C_CONST
AC_PROG_INSTALL
AC_PROG_MKDIR_P

dnl Checks for libraries.

AC_SEARCH_LIBS(sin,m)

dnl Check for readline with various possible required support libs

AS_UNSET(ac_cv_lib_readline_append_history)
for termlib in "" -ltermcap -lncurses -lcurses; do
  if test "$ac_cv_lib_readline_append_history" != yes ; then
    AS_UNSET(ac_cv_lib_readline_append_history)
    AC_CHECK_LIB(readline,append_history,
                 [LIBS="-lreadline $termlib $LIBS";DEFIS="$DEFIS -DREADLINE"],
                 [],[$termlib])
  fi
done



if test "$ac_cv_lib_readline_append_history" = yes; then
  AC_CHECK_DECL(rl_completion_suppress_append, [],
                   [DEFIS="$DEFIS -DNO_SUPPRESS_APPEND"],
                   [[#include <readline/readline.h>]])
fi


dnl Checks for header files.
AC_CHECK_HEADER(string.h,[],[DEFIS="$DEFIS -DSTRINGS_H"])
AC_CHECK_HEADER(stdlib.h,[],[DEFIS="$DEFIS -DNO_STDLIB_H"])

dnl Checks for library functions.
AC_CHECK_FUNC(strchr,[],DEFIS="$DEFIS -DNO_STRCHR")
AC_CHECK_FUNC(strspn,[],DEFIS="$DEFIS -DNO_STRSPN";STRFUNC="strfunc.$OBJEXT")
AC_CHECK_FUNC(strtok,[],DEFIS="$DEFIS -DNO_STRTOK";STRFUNC="strfunc.$OBJEXT")

AC_CHECK_DECL(TIOCGWINSZ,[DEFIS="$DEFIS -DHAVE_IOCTL"],[],[#include<sys/ioctl.h>])

AC_CHECK_FUNC(setenv,[],DEFIS="$DEFIS -DNO_SETENV")
AC_CHECK_FUNC(setlocale,[],DEFIS="$DEFIS -DNO_SETLOCALE")

AC_CACHE_CHECK([for locale and UTF-8 support], am_cv_utf8,
  [AC_TRY_LINK([
#define _XOPEN_SOURCE 600
#include <wchar.h>
#include <locale.h>
#include <langinfo.h>
], [wchar_t *out;char *in;char *res;
     res=setlocale(LC_CTYPE,"");res=in; 
     mbsrtowcs(out, &res, 2, NULL);
     wcswidth(out,2);],
    am_cv_utf8=yes,
    am_cv_utf8=no)
])

if test $am_cv_utf8 = yes; then
    DEFIS="$DEFIS -DSUPPORT_UTF8"
fi

AC_CACHE_CHECK([for isfinite], am_cv_isfinite,
  [AC_TRY_LINK([
#define _XOPEN_SOURCE 600
#include <math.h>
], [float a;a=1;isfinite(a);],
    am_cv_isfinite=yes,
    am_cv_isfinite=no)
])

if test $am_cv_isfinite = no; then
    DEFIS="$DEFIS -DNO_ISFINITE"
fi

AC_CHECK_PROG(mkstoolkit,mksinfo,found,no)


# assume that if we have the MKS Toolkit, we're running the Korn shell
HAVE_MKS=no
if test $mkstoolkit = found; then
    if mksinfo > /dev/null; then
      if test -n "$ROOTDIR"; then
        DEFIS="$DEFIS -DHAVE_MKS_TOOLKIT"
        echo 'using MKS Toolkit'
        MKS_POSIX=".POSIX:"
        MKS_RES='$(RES)'
        HAVE_MKS=yes
        # if the user has specified a drive, leave things alone.
        # otherwise, prepend the drive on which the Toolkit is installed
        case "$prefix" in
        ?:*)
          MKS_DRIVE= ;;
        *)
          MKS_DRIVE="${ROOTDIR%%:*}:" ;;
        esac
      else
        echo 'no ROOTDIR... cannot use MKS Toolkit'
      fi
    else
      echo 'mksinfo failed... not using MKS Toolkit'
    fi
fi


AC_PATH_PROG(PYTHON, python, no)

if test $PYTHON = no; then 
   HAVE_PYTHON=no
   echo '    Units will work without python but the currency update'
   echo '    script, units_cur,  will not be installed'
   PYTHON=''
else
   HAVE_PYTHON=yes
fi

dnl Check for path search option
AC_ARG_ENABLE([relocation],
    AC_HELP_STRING([--enable-relocation],
       [look in program directory for units data files (default is NO)]),
    [if test $enable_relocation = yes ;then
          UDATADIR=""
	  RELOCATION="yes"
          echo relocation enabled
     else
          UDATADIR="$datadir/units/"	
	  echo relocation disabled
     fi
    ], [UDATADIR="$datadir/units/"])

UDAT="$datadir/units/"
CDAT="$sharedstatedir/units/"


AC_CONFIG_FILES(Makefile)
AC_OUTPUT
