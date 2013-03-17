/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "repocontrollermaster.H"
#include "stasher/nodeinfo.H"
#include "stasher/client.H"
#include "nslist.H"
#include "trancommit.H"
#include <x/destroycallbackflag.H>
#include <x/serialize.H>
#include <sstream>
#include <iostream>

const char tstnodes::clusterdir[]="conftestcluster.dir";

#define CLUSTERNAME	"test"

std::string tstnodes::getnodedir(size_t i)
{
	std::ostringstream o;

	o << "conftestnode" << (char)('a'+i) << ".dir";

	return o.str();
}

std::string tstnodes::getnodefullname(size_t i)
{
	return getnodename(i) + "." CLUSTERNAME;
}

std::string tstnodes::getnodename(size_t i)
{
	std::ostringstream o;

	o << "node" << (char)('a'+i);

	return o.str();
}

void tstnodes::clustkey_generate(time_t now)
{
	repomg::clustkey_generate(clusterdir, CLUSTERNAME,
				  now,
				  now + 365 * 24 * 60 * 60,
				  "rsa",
				  "weak",
				  "sha1");
}

tstnodes::tstnodes(size_t nArg) : n(nArg), useencryption(false)
{
	x::dir::base::rmrf(clusterdir);
	time_t now(time(NULL));

	clustkey_generate(now);

	for (size_t i=0; i<n; ++i)
	{
		x::dir::base::rmrf(getnodedir(i));

		update_node_key(now, i);
	}
}

void tstnodes::update_cluster_keys(size_t i)
{
	repomg::key_export(clusterdir, getnodedir(i));
}

void tstnodes::update_cluster_keys_local(size_t i)
{
	repomg::key_export_local(clusterdir, getnodedir(i));
}

void tstnodes::update_node_key(time_t now, size_t i)
{
	repomg::nodekey_generate(getnodedir(i), clusterdir, "",
				 getnodename(i),
				 now, now + 7 * 24 * 60 * 60, "weak", "sha1");
}

tstnodes::~tstnodes()
{
	x::dir::base::rmrf(clusterdir);

	for (size_t i=0; i<n; ++i)
		x::dir::base::rmrf(getnodedir(i));
}

tstnodes::nodeObj::nodeObj(const std::string &dir)
	: node(dir)
{
}

tstnodes::nodeObj::~nodeObj() noexcept
{
}

// Initial test nodes with a basic root namespace view
void tstnodes::init(std::vector<noderef> &nodes,
		    const char *root_ns)
{
	init(nodes, { {"", root_ns} }, { {"etc", "etc"}});
}

void tstnodes::init(std::vector<noderef> &nodes,
		    const std::map<std::string, std::string> &rwmap,
		    const std::map<std::string, std::string> &romap)
{
	nodes.clear();
	nodes.resize(n);

	for (size_t i=0; i<n; ++i)
	{
		nodes[i]=noderef::create(getnodedir(i));

		newtran tran=nodes[i]->repo->newtransaction();

		nslist dummy_ns;

		{
			nsmap dummy_map;
			dummy_map.map=rwmap;

			dummy_ns.rw.push_back(dummy_map);
		}

		{
			nsmap dummy_map;
			dummy_map.map=romap;

			dummy_ns.ro.push_back(dummy_map); // [WRITEPRIVILEGES]
		}

		x::fd serialized(tran->newobj(NSLISTOBJECT));

		x::fd::base::outputiter iter(serialized);

		x::serialize::iterator<x::fd::base::outputiter> ser_iter(iter);

		ser_iter(dummy_ns);

		ser_iter.getiter().flush();
		serialized->close();

		x::uuid uuid=tran->finalize();

		nodes[i]->repo->begin_commit(uuid, x::eventfd::create())
			->commit();
		nodes[i]->repo->cancel(uuid);
	}
}

void tstnodes::setmasteron0(std::vector<noderef> &nodes)
{
	STASHER_NAMESPACE::nodeinfomap clusterinfo;

	STASHER_NAMESPACE::nodeinfo info;

	info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
					   "localhost"));

	if (useencryption)
		info.useencryption(true);

	for (size_t i=0; i<nodes.size(); ++i)
	{
		clusterinfo[getnodefullname(i)]=info;
		info.nomaster(true); // 2nd and subsequent nodes
	}

	for (size_t i=0; i<nodes.size(); ++i)
	{
		repoclusterinfoObj::saveclusterinfo(nodes[i]->repo,
						    clusterinfo);
	}
}

repocontrollermasterptr
tstnodes::startmastercontrolleron0(std::vector<noderef> &nodes)
{
	setmasteron0(nodes);

	for (size_t i=0; i<nodes.size(); i++)
		nodes[i]->start(true);

	if (nodes.size() > 1)
	{
		for (size_t i=0; i<nodes.size(); i++)
			nodes[i]->debugWaitFullQuorumStatus(false);

		for (size_t i=0; i<nodes.size(); i++)
		{
			std::cerr << "Waiting for connection attempts from node "
				  << (char)('a'+i) << " to complete" << std::endl;

			nodes[i]->listener->connectpeers();
			nodes[i]->debugWait4AllConnections();
		}

		std::cerr << "Waiting for quorum" << std::endl;
	}

	for (size_t i=0; i<nodes.size(); i++)
		nodes[i]->debugWaitFullQuorumStatus(true);

	return nodes[0]->repocluster->getCurrentController();
}

void tstnodes::startmastercontrolleron0_int(std::vector<noderef> &nodes)

{
	startmastercontrolleron0(nodes);
}

void tstnodes::startreconnecter(const std::vector<noderef> &nodes)

{
	for (std::vector<noderef>::const_iterator b=nodes.begin(),
		     e=nodes.end(); b != e; ++b)
		(*b)->start_reconnecter();
}

void tstnodes::any1canbemaster(std::vector<noderef> &nodes)
{
	thesecantbemaster(nodes, std::set<size_t>());
}

void tstnodes::thesecantbemaster(std::vector<noderef> &nodes,
				 const std::set<size_t> &nomasters)

{
	std::set<std::string> s;
	tobjrepoObj::values_t valuesMap;
	std::set<std::string> notfound;

	s.insert(STASHER_NAMESPACE::client::base::clusterconfigobj);

	nodes[0]->repo->values(s, false, valuesMap, notfound);

	tobjrepoObj::values_t::iterator
		iter=valuesMap.find(STASHER_NAMESPACE::client::base
				    ::clusterconfigobj);

	if (iter == valuesMap.end())
		throw EXCEPTION("Could not obtain uuid for "
				+ std::string(STASHER_NAMESPACE::client::base
					      ::clusterconfigobj));

	newtran tr(nodes[0]->repo->newtransaction());

	{
		x::fd fd(tr->updobj(STASHER_NAMESPACE::client::base
				    ::clusterconfigobj,
				    iter->second.first));

		STASHER_NAMESPACE::nodeinfomap clusterinfo;

		STASHER_NAMESPACE::nodeinfo info;

		info.options.insert(std::make_pair
				    (STASHER_NAMESPACE::nodeinfo::host_option,
				     "localhost"));

		if (useencryption)
			info.useencryption(true);

		for (size_t i=0; i<nodes.size(); ++i)
		{
			STASHER_NAMESPACE::nodeinfo ninfo=info;

			if (nomasters.find(i) != nomasters.end())
				ninfo.nomaster(true);

			clusterinfo[getnodefullname(i)]=ninfo;
		}

		x::fd::base::outputiter fditer(fd);

		x::serialize
			::iterator<x::fd::base::outputiter> ser_iter(fditer);

		ser_iter(clusterinfo);

		fditer.flush();
		fd->close();
	}

	trandistributorObj::transtatus stat;
	x::ptr<x::obj> mcguffin;
	mcguffin=x::ptr<x::obj>::create();

	std::cout << "Updating cluster node list" << std::endl;

	stat=nodes[0]->distributor->newtransaction(tr, mcguffin);

	x::destroyCallbackFlag flag(x::destroyCallbackFlag::create());

	mcguffin->addOnDestroy(flag);

	mcguffin=x::ptr<x::obj>();
	flag->wait();
}

