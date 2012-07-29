/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include "repomg.H"
#include <x/options.H>
#include <algorithm>

static void test1(tstnodes &t)
{
	time_t now=time(NULL);

	std::cerr << "test1" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	tnodes[0]->debugWaitQuorumStatus(true);
	tnodes[1]->debugWaitQuorumStatus(true);

	std::list<x::gnutls::x509::crt> dummy;

	x::gnutls::datum_t before=
		repomg::loadcerts(repomg::nodecert(tstnodes::getnodedir(1)),
				  dummy);

	repomg::nodekey_generate(t.getnodedir(0), t.clusterdir, "",
				 t.getnodename(1),
				 now, now + 7 * 24 * 60 * 60, "weak", "sha1");
	x::gnutls::datum_t after=
		repomg::loadcerts(repomg::nodecert(tstnodes::getnodedir(1)),
				  dummy);

	if (before->size() == after->size() &&
	    std::equal(before->begin(), before->end(),
		       after->begin()))
		throw EXCEPTION("Certificate push failed");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(2);

		test1(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}
