#
# Copyright 2013 Double Precision, Inc.
# See COPYING for distribution information.
#
# Don't execute this directly. Run 'make perl-configure' in top level build
# directory.

use 5.016003;
use ExtUtils::MakeMaker;
use IO::File;

my $CC = $ENV{'CC'} // 'g++';
my $shell=$ENV{'SHELL'} // '/bin/sh';
my $top_builddir="../..";

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'Stasher',
    VERSION_FROM      => 'lib/Stasher.pm', # finds $VERSION
    PREREQ_PM         => {}, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?     ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM  => 'lib/Stasher.pm', # retrieve abstract from module
       AUTHOR         => 'Sam Varshavchik <mrsam@courier-mta.com>') : ()),
    OBJECT            => '$(BASEEXT)$(OBJ_EXT) Stasherapi$(OBJ_EXT)',
    CC		      => $CC, # . ' -std=c++11 $(call GETFLAGS,$@)',
    LD                => '$(CC) -Wl,-R/usr/local/lib -Wl,../../.libs/libstasher-client.$(DLEXT)',
    FULLPERL          => 'LD_LIBRARY_PATH=`pwd`/../.. $(PERL)',
    LIBS              => [''], # e.g., '-lm'
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I. -I' . $top_builddir, # e.g., '-I. -I/usr/include/other'
    PM                => {'lib/Stasher.pm' => '$(INST_LIBDIR)/Stasher.pm'},
    LICENSE           => 'gpl',
	# Un-comment this if you add C files to link with later:
);

sub MY::postamble {

    return <<END

lib/Stasher.pm: lib/Stasher.pm.in
	rm -f lib/Stasher.pm
	cd $top_builddir; $shell ./config.status perl/Stasher/lib/Stasher.pm
	chmod -w lib/Stasher.pm

# GETFLAGS=\$(\$(@).flags)
# Stasherapi\$(OBJ_EXT).flags=-std=c++0x
Stasherapi\$(OBJ_EXT): Stasherapi.C


# End of results

END
;
}
