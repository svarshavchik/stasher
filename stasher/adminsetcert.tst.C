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

#include <sstream>

static size_t count_certs(const std::string &report)
{
	std::istringstream i(report);

	size_t cnt=0;

	std::string line;

	do
	{
		std::getline(i, line);

		std::string::iterator b=line.begin(), e=line.end();

		while (b != e)
		{
			if (*b != ' ')
				break;
			++b;
		}

		if (std::string(b, e).substr(0, 20)
		    == "Cluster certificate:")
			++cnt;
	} while (!i.eof());

	return cnt;
}

static std::string node_cert(const std::string &report)
{
	std::istringstream i(report);

	std::string line;

	std::string cert;

	do
	{
		std::getline(i, line);

		std::string::iterator b=line.begin(), e=line.end();

		while (b != e)
		{
			if (*b != ' ')
				break;
			++b;
		}

		if (std::string(b, e).substr(0, 17)
		    == "Node certificate:")
		{
			cert=std::string(b, e);
			break;
		}
	} while (!i.eof());

	return cert;
}

static void test1(tstnodes &t)
{
	time_t now;

	std::cerr << "test1" << std::endl;

	{
		std::vector<tstnodes::noderef> tnodes;

		t.init(tnodes);
		t.startmastercontrolleron0_int(tnodes);

		tnodes[0]->debugWaitQuorumStatus(true);
		tnodes[1]->debugWaitQuorumStatus(true);

		STASHER_NAMESPACE::client cl0=
			STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0));

		STASHER_NAMESPACE::client cl1=
			STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(1));

		std::cerr << cl0->getserverstatus()->report << std::endl;

		now=time(NULL);
		tstnodes::clustkey_generate(time(NULL));
		tstnodes::update_cluster_keys(1);

		std::string report=cl0->getserverstatus()->report;

		std::cerr << report << std::endl;

		if (count_certs(report) != 2) // [KEYEXPORTDIST]
			throw EXCEPTION("[KEYEXPORTDIST] failed (1)");
	}

	while (time(NULL) == now)
		sleep(1);

	tstnodes::clustkey_generate(now=time(NULL));
	tstnodes::update_cluster_keys_local(0); // [CREDENTIALSUPDATE]
	tstnodes::update_cluster_keys_local(1); // [CREDENTIALSUPDATE]

	{
		std::vector<tstnodes::noderef> tnodes;

		t.init(tnodes);
		t.startmastercontrolleron0_int(tnodes);

		tnodes[0]->debugWaitQuorumStatus(true);
		tnodes[0]->debugWaitQuorumStatus(true);

		STASHER_NAMESPACE::client cl0=
			STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0));

		STASHER_NAMESPACE::client cl1=
			STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(1));

		std::string report=cl0->getserverstatus()->report;

		std::cerr << report << std::endl;

		if (count_certs(report) != 3)
			throw EXCEPTION("[CREDENTIALSUPDATE] failed");
		while (time(NULL) == now)
			sleep(1);

		tstnodes::clustkey_generate(time(NULL));
		tstnodes::update_cluster_keys(1);

		report=cl0->getserverstatus()->report;

		std::cerr << report << std::endl;

		if (count_certs(report) != 4) // [KEYEXPORTDIST]
			throw EXCEPTION("[KEYEXPORTDIST] failed (2)");

		std::string before_cert=node_cert(report);

		tstnodes::update_node_key(time(NULL), 0);

		report=cl0->getserverstatus()->report;

		std::cerr << report << std::endl;

		std::string after_cert=node_cert(report);

		if (before_cert == after_cert)
			throw EXCEPTION("[NODEKEYUPDATE] failed");
	}
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		time_t now=time(NULL);

		tstnodes nodes(2);

		while (time(NULL) == now)
			sleep(1);

		test1(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}
