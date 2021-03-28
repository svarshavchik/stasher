#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.66])
AC_INIT(stasher, 0.11.0, [mrsam@courier-mta.com])
AM_INIT_AUTOMAKE(dist-bzip2 foreign)
AC_CONFIG_SRCDIR([objrepo.H])
AC_CONFIG_HEADERS([objrepo_config.h])
LT_INIT
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.20.2])
AM_XGETTEXT_OPTION([--directory=\${top_builddir}])

AC_ARG_WITH(extratests,[Additional testing (may fail on busy build servers)],
				   extratests="$withval",
				   extratests=no)
AM_CONDITIONAL(extratests, test "$extratests" = "yes")

# Checks for programs.
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_PATH_PROGS(PERL,perl5 perl,/usr/bin/perl)

# Checks for libraries.

# Checks for header files.

AC_CHECK_HEADER([readline/readline.h],
		[AC_DEFINE([HAVE_READLINE_READLINE_H], [1],
                        [Define to 1 if you have <readline/readline.h>.])
		LIBREADLINE='-lreadline'])

AC_SUBST(LIBREADLINE)

AC_ARG_WITH(testprograms,
	[AS_HELP_STRING([--with-testprograms],[build test programs])], [
  TESTPROGS='$(ALLTESTPROGS)'
  ], [
   TESTPROGS=""
  ])

AC_SUBST(TESTPROGS)

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

LIBCXX_INIT
LDFLAGS="$LDFLAGS -z relro -z now"

AM_CONDITIONAL(HAVE_DOCBOOK,[test -d ${srcdir}/docbook])
AC_CONFIG_FILES(Doxyfile Makefile po/Makefile.in
		stasher.sysinit sysinit.rc
		perl/Stasher/lib/Stasher.pm
		packaging/fedora/Makefile
		packaging/fedora/stasher.spec
		packaging/fedora/stasher.service)
AC_OUTPUT
