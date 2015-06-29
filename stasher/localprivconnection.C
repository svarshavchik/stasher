/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "localprivconnection.H"
#include "repoclusterinfoimpl.H"
#include "threadreport.H"
#include "replymsgs.auto.H"
#include "deserobjname.H"
#include "repoclusterinfoimpl.H"
#include "repomg.H"
#include "clusterlistener.H"
#include <x/property_list.H>
#include <x/eventdestroynotify.H>
#include <algorithm>

MAINLOOP_IMPL(localprivconnectionObj)

#include "localprivconnection.msgs.def.H"

localprivconnectionObj
::localprivconnectionObj(const std::string &threadnameArg,
			 const tobjrepo &repo,
			 const STASHER_NAMESPACE
			 ::stoppableThreadTracker &tracker,
			 const x::ptr<trandistributorObj> &distributor,
			 const repoclusterinfoimpl &cluster,
			 const spacemonitor &spacemonitorArg,
			 const x::semaphore &getsemaphoreArg,
			 const clusterlistener &listenerArg)
: localconnectionObj(threadnameArg, nsview::create(), repo, tracker,
		     distributor, cluster, spacemonitorArg,
		     getsemaphoreArg),
	listener(listenerArg)
{
}

localprivconnectionObj::~localprivconnectionObj() noexcept
{
}

void localprivconnectionObj::deserialized(const STASHER_NAMESPACE::adminstop
					  &msg)

{
	LOG_INFO("Received stop command: " << getName());

	repoclusterinfoimpl(cluster)->stop(*socket);
}

class LIBCXX_INTERNAL localprivconnectionObj::getserverstatusdonecb
	: virtual public x::obj {

 public:
	x::weakptr<x::ptr<localprivconnectionObj> > conn;
	x::ptr<STASHER_NAMESPACE::threadreportObj> report;
	x::uuid requuid;

	getserverstatusdonecb(const x::ptr<localprivconnectionObj> &connArg,
			      const x::ptr<STASHER_NAMESPACE
					   ::threadreportObj> &reportArg,
			      const x::uuid &requuidArg)
		: conn(connArg), report(reportArg), requuid(requuidArg)
	{
	}

	~getserverstatusdonecb() noexcept
	{
	}

	void destroyed() noexcept
	{
		x::ptr<localprivconnectionObj> ptr=conn.getptr();

		if (!ptr.null())
			ptr->serverstatusreport_done(requuid, report);
	}
};

void localprivconnectionObj::deserialized(const getserverstatus_req_t
					  &msg)
{
	x::ptr<STASHER_NAMESPACE::threadreportObj> report=tracker->getReport();

	x::ptr<x::obj> mcguffin=report->mcguffin;

	report->mcguffin=x::ptr<x::obj>();

	auto cb=x::ptr<getserverstatusdonecb>
		::create(x::ptr<localprivconnectionObj>(this), report,
			 msg.requuid);

	mcguffin->ondestroy([cb]{cb->destroyed();});

	report->mcguffin=x::ptr<x::obj>();
}

void localprivconnectionObj::dispatch(const serverstatusreport_done_msg &msg)

{
	std::ostringstream o;

	o << "Cluster:" << std::endl;

	try {
		cluster->report(o);
	} catch (const x::exception &e)
	{
		o << "  " << e << std::endl;
	}

	msg.report->format(o);

	getserverstatus_resp_msg_t resp(msg.requuid);
	getserverstatus_resp_msg_t::resp_t &msgres=resp.getmsg();

	msgres.report=o.str();
	resp.write(writer);
}

void localprivconnectionObj::deserialized(const getprops_req_t &msg)

{
	getprops_resp_msg_t resp(msg.requuid);
	getprops_resp_msg_t::resp_t &msgres=resp.getmsg();

	x::property::enumerate_properties(msgres.properties);
	resp.write(writer);
}

std::string localprivconnectionObj::normalize(const std::string &name)
{
	std::list<std::string> hier_list;

	x::property::parsepropname(name.begin(), name.end(), hier_list,
				   x::locale::create(""));

	return x::property::combinepropname(hier_list);
}

class LIBCXX_INTERNAL localprivconnectionObj::propsetresetaction {

 public:
	propsetresetaction() {}
	~propsetresetaction() noexcept {}

	virtual void operator()(std::map<std::string,
					 std::string> &propmap)=0;

	class setpropvalue;
	class resetpropvalue;
};

class LIBCXX_INTERNAL localprivconnectionObj::propsetresetaction::setpropvalue :
	public propsetresetaction {

 public:
	std::string name, value;

	setpropvalue(const std::string &nameArg,
		     const std::string &valueArg)
		: name(nameArg), value(valueArg) {}

	~setpropvalue() noexcept {}

	void operator()(std::map<std::string,
				 std::string> &propmap)

	{
		x::property::load_property(name, value, true,
					   true,
					   x::property::errhandler::errthrow(),
					   x::locale::create(""));

		propmap[name]=value=x::property::value<std::string>
			(name, std::string()).getValue();
	}
};

class LIBCXX_INTERNAL localprivconnectionObj::propsetresetaction::resetpropvalue :
	public propsetresetaction {

 public:
	std::string name;

	resetpropvalue(const std::string &nameArg)
		: name(nameArg) {}

	~resetpropvalue() noexcept {}

	void operator()(std::map<std::string,
				 std::string> &propmap)

	{
		propmap.erase(name);
	}
};

void localprivconnectionObj::deserialized(const setprop_req_t &msg)
{
	setprop_resp_msg_t resp(msg.requuid);
	setprop_resp_msg_t::resp_t &msgres=resp.getmsg();

	try {
		std::string name=normalize(msg.propname);

		propsetresetaction::setpropvalue action(name, msg.propvalue);

		dopropsetreset(action, name);

		msgres.succeeded=true;
		msgres.newvalue=action.value;

	} catch (const x::exception &e)
	{
		std::ostringstream o;

		msgres.succeeded=false;

		o << e;
		msgres.errmsg=o.str();
	}
	resp.write(writer);
}

void localprivconnectionObj::deserialized(const resetprop_req_t &msg)

{
	resetprop_resp_msg_t resp(msg.requuid);
	resetprop_resp_msg_t::resp_t &msgres=resp.getmsg();

	try {
		std::string name=normalize(msg.propname);

		propsetresetaction::resetpropvalue action(name);

		dopropsetreset(action, name);

		msgres.resultmsg="Property will be reset effective server restart";
	} catch (const x::exception &e)
	{
		std::ostringstream o;

		o << e;
		msgres.resultmsg=o.str();
	}
	resp.write(writer);
}

void localprivconnectionObj::dopropsetreset(propsetresetaction &action,
					    const std::string &name)

{
	std::map<std::string, std::string> props;

	x::property::enumerate_properties(props);

	// Check that the property actually exists
	if (props.find(name) == props.end())
		throw EXCEPTION("Property does not exist");

	props.clear();

	x::locale glob=x::locale::create("");

	// Load properties from the property file

	x::property::list tmplist=x::property::list::create();

	std::string filename=repo->directory + "/properties";

	tmplist->load_file(filename, true, true,
			   x::property::errhandler::errthrow(), glob);

	tmplist->enumerate(props);

	action(props);

	// And save the list back where it came from
	x::fd savefd=x::fd::create(filename);

	x::ostream o=savefd->getostream();

	x::property::save_properties<char>(props,
					   std::ostreambuf_iterator<char>(*o),
					   glob);
	*o << std::flush;

	savefd->close();
}

bool localprivconnectionObj::allow_admin_get()

{
	return true;
}

void localprivconnectionObj::deserialized(const certreload_req_t &msg)

{
	certreload_resp_msg_t resp(msg.requuid);

	listener->reload();
	resp.write(writer);
}


class LIBCXX_INTERNAL localprivconnectionObj::resigndonecb
	: virtual public x::obj {

 public:
	x::weakptr<x::ptr<localprivconnectionObj> > conn;
	boolref status;
	x::uuid requuid;

	resigndonecb(const x::ptr<localprivconnectionObj> &connArg,
		     const boolref &statusArg,
		     const x::uuid &requuidArg)
		: conn(connArg), status(statusArg), requuid(requuidArg)
	{
	}

	~resigndonecb() noexcept
	{
	}

	void destroyed() noexcept
	{
		x::ptr<localprivconnectionObj> ptr=conn.getptr();

		if (!ptr.null())
			ptr->resign_done(requuid, status);
	}
};

void localprivconnectionObj::deserialized(const resign_req_t &msg)

{
	boolref status=boolref::create();

	auto resign_ret=cluster->resign(status);

	auto cb=x::ref<resigndonecb>::create
		(x::ptr<localprivconnectionObj>(this), status, msg.requuid);

	resign_ret->ondestroy([cb]{cb->destroyed();});
}

void localprivconnectionObj::dispatch(const resign_done_msg &msg)

{
	resign_resp_msg_t resp(msg.requuid);
	resign_resp_msg_t::resp_t &msgres=resp.getmsg();

	msgres.status=msg.status->flag;
	msgres.master=clusterinfoObj::status(cluster)->master;
	resp.write(writer);
}

std::string localprivconnectionObj::report(std::ostream &rep)

{
	localconnectionObj::report(rep);
	return "connection(admin, " + getName() + ")";
}

class LIBCXX_INTERNAL localprivconnectionObj::newcertdonecb
	: virtual public x::obj {

 public:
	x::weakptr<x::ptr<localprivconnectionObj> > conn;
	x::uuid requuid;
	x::ref<repopeerconnectionObj::setnewcertObj> req;

	newcertdonecb(const x::ptr<localprivconnectionObj> &connArg,
		      const x::ref<repopeerconnectionObj::setnewcertObj>
		      &reqArg,
		      const x::uuid &requuidArg)
		: conn(connArg), requuid(requuidArg), req(reqArg)
	{
	}

	void destroyed() noexcept
	{
		x::ptr<localprivconnectionObj> ptr=conn.getptr();

		if (!ptr.null())
			ptr->setnewcert_done(requuid, req);
	}
};

void localprivconnectionObj::deserialized(const setnewcert_req_t &msg)
{
	auto req=x::ref<repopeerconnectionObj::setnewcertObj>
		::create(msg.certificate);

	auto mcguffin=x::ref<x::obj>::create();

	auto callback=x::ref<newcertdonecb>
		::create(x::ptr<localprivconnectionObj>(this),
			 req, msg.requuid);

	mcguffin->ondestroy([callback]{callback->destroyed();});

	std::string peername=repomg::certificate_name(msg.certificate,
						      "certificate");

	auto peer=cluster->getnodepeer(peername);

	if (peer.null())
	{
		req->result="No connection with " + peername;
		return; // Let nature take its course
	}

	x::ref<repopeerconnectionObj>(peer)->setnewcert_request(req, mcguffin);
}

void localprivconnectionObj::dispatch(const setnewcert_done_msg &msg)
{
	setnewcert_resp_msg_t resp(msg.requuid);
	setnewcert_resp_msg_t::resp_t &msgres=resp.getmsg();

	msgres.success=msg.req->success;
	msgres.message=msg.req->result;
	resp.write(writer);

}

class localprivconnectionObj::halted_cbObj
	: virtual public x::obj {

public:

	x::weakptr<x::ptr<localprivconnectionObj> > conn;

	haltrequest_resp_msg_t resp;

	halted_cbObj(const x::ptr<localprivconnectionObj> &connArg,
		     const x::uuid &requuid) : conn(connArg), resp(requuid)
	{
	}

	~halted_cbObj() noexcept
	{
	}

	void destroyed() noexcept
	{
		auto c=conn.getptr();

		if (!c.null())
			c->halt_ack(resp);
	}
};

void localprivconnectionObj::deserialized(const haltrequest_req_t &msg)
{
	auto mcguffin=x::ref<x::obj>::create();

	auto cb=x::ref<halted_cbObj>
		::create(x::ptr<localprivconnectionObj>(this), msg.requuid);

	mcguffin->ondestroy([cb]{cb->destroyed();});

	STASHER_NAMESPACE::haltrequestresults res(&cb->resp.getmsg());

	cluster->getCurrentController()->halt(res, mcguffin);
}

void localprivconnectionObj::dispatch(const halt_ack_msg &msg)
{
	LOG_INFO("Acknowledging halt");
	msg.resp.write(writer);
}
