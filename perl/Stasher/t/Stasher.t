# This test should be runnable after 'make install' as 'perl Stasher.t'.
# Stasher must be installed, running a single repository server on this
# machine, in the default repository location.

use strict;
use warnings;

use Test::More;
use File::Temp qw/tempfile/;

BEGIN { use_ok('Stasher'); };

#########################

sub testsuite {
    my ($ok) = @_;

    my $conn = Stasher::connect;

    $ok->($conn, "Succesfully connected to the server");

    my %ret=$conn->limits;

    $ok->($conn, "Succesfully called limits(): " . join(", ", map {
	"$_=" . $ret{$_};
	} sort keys %ret));

    my @defaultnodes = Stasher::defaultnodes;
    print join(":", @defaultnodes) . "\n";
    $ok->(1, "Called dirs");

    my $res=$conn->exists("east", "north");

    $ok->($res, "exists() call made");

    my $uuid=$conn->update(%$res);

    $ok->($uuid, "Removed existing objects [uuid $uuid]");

    my $fh=tempfile;

    print $fh "North Object";
    $fh->seek(0, 0);

    $uuid=$conn->update(
	east => { contents => "East Object" },
	north => { fd => $fh },
	);

    $ok->($uuid, "Created objects");

    my $uuid2=$conn->update(
	east => { uuid => $uuid, contents => "Second East Object" }
	);

    $ok->($uuid2, "Updated object");

    $ok->($conn->fetch(objects => ["east"])->{east}{contents} eq "Second East Object", "Verified contents of updated object");

    $fh=$conn->fetch(objects=>["east"], fd=>1)->{east}{fd};

    $ok->($fh && join("", <$fh>) eq "Second East Object", "Verified contents of updated object via file descriptor");

    $ok->(!$conn->update(
	       east => { uuid => $uuid, contents => "Second East Object" }
	), "Rejected update due to missing uuid");

    my @dir_hier;

    push @dir_hier, "";

    my $h;

    while (defined($h=shift @dir_hier))
    {
	foreach my $obj ($conn->dir($h))
	{
	    if ($obj =~ m@/$@)
	    {
		push @dir_hier, $obj;
	    }
	    else
	    {
		print "OBJECT: $obj\n";
	    }
	}
    }
    $ok->(1, "Called dir()");
    $ok->($conn->update( east => { uuid => $uuid2 }, north => {uuid => $uuid}), "Removed both objects");

    eval {
	$conn->exists('//invalid//object');
    };

    $ok->($@, "Caught exception");
}

if ($ENV{"LEAKCHECK"})
{
    my $t=time;

    while (time < $t+20)
    {
	testsuite(sub {
	    my ($v, $descr) = @_;
	    ok($v, $descr) if !$v;
	    });
    }
}
else
{
    testsuite(sub {
	my ($v, $descr) = @_;
	ok($v, $descr);
	});
}

done_testing();

