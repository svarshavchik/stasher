/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include <x/exception.H>
#include <x/options.H>

#include "boolref.H"
#include "boolref.C"
#include "objrepocopysrcinterface.C"
#include "nodeclusterstatus.H"
#include "clusterinfofwd.H"

#define clusterinfo_H

class peerstatusObj;

class clusterinfoObj : virtual public x::obj {

public:
	bool install_called;

	bool recalculate_called;

	bool newmaster_called;

	clusterinfoObj() : install_called(false), recalculate_called(false),
			   newmaster_called(false) {}

	~clusterinfoObj() noexcept {}

	std::string nodename;

	const std::string &getthisnodename() noexcept
	{
		return nodename;
	}

	std::pair<bool, x::ptr<peerstatusObj> >
	installpeer(const std::string &nodename,
		    const x::ptr<peerstatusObj> &node)

	{
		install_called=true;
		return std::make_pair(true, x::ptr<peerstatusObj>());
	}

	void peerstatusupdated(const x::ptr<peerstatusObj> &peerRef,
			       const nodeclusterstatus &peerStatus)

	{
		recalculate_called=true;
	}

	void peernewmaster(const x::ptr<peerstatusObj> &peerRef,
			   const nodeclusterstatus &newmaster)

	{
		newmaster_called=true;
	}

	void recalculate()
	{
		recalculate_called=true;
	}
};

#include "peerstatus.C"
#include "clusterstatusnotifier.C"
#include <iostream>
#include <cstdlib>

class teststatusObj : public peerstatusObj::adapterObj {

public:
	teststatusObj(const std::string &peername)
		: peerstatusObj::adapterObj(peername) {}

	~teststatusObj() noexcept {}

	void stop()
	{
	}

	using peerstatusObj::install;
	using peerstatusObj::peerstatusupdate;
};

static void test1()
{
	clusterinfo c(clusterinfo::create());

	auto stat=x::ptr<teststatusObj>::create("foo");

	{
		nodeclusterstatus s("a", x::uuid(), 1, false);

		stat->peerstatusupdate(s);

		if (*teststatusObj::status(stat) != s ||
		    teststatusObj::status(stat)->uuid != s.uuid)
			throw EXCEPTION("What?");
	}

	if (c->install_called || c->recalculate_called)
		throw EXCEPTION("What?");

	stat->install(c);

	if (!c->install_called || c->recalculate_called)
		throw EXCEPTION("What?");

	{
		nodeclusterstatus s("b", x::uuid(), 1, false);

		stat->peerstatusupdate(s);

		if (*teststatusObj::status(stat) != s ||
		    teststatusObj::status(stat)->uuid != s.uuid ||
		    !c->recalculate_called)
			throw EXCEPTION("What?");

		c->newmaster_called=false;

		stat->peerstatusupdate(s);

		if (c->newmaster_called)
			throw EXCEPTION("What?");

		stat->peerstatusupdate(nodeclusterstatus("c", x::uuid(), 1,
							 false));
		if (!c->newmaster_called)
			throw EXCEPTION("What?");

	}

	c->recalculate_called=false;

	stat=x::ptr<teststatusObj>(); // [ONDESTROY]

	if (!c->recalculate_called)
		throw EXCEPTION("[ONDESROY] failed");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);
	try {
	} catch (const x::exception &e)
	{
		test1();

		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}
