/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/managedhierarchymonitor.H"
#include <stasher/client.H>
#include <stasher/userinit.H>
#include <stasher/manager.H>
#include <stasher/managedsubscriber.H>
#include <x/logger.H>

LOG_CLASS_INIT(STASHER_NAMESPACE::managedhierarchymonitorObj);

STASHER_NAMESPACE_START

// The implementation object is a hierarchy subscription, with extra jazz.

class LIBCXX_HIDDEN managedhierarchymonitorObj::implObj
	: public managedsubscriberObj {

	// Current client connection
	x::weakptr<clientptr> client_connection;

	// Hierarchy's name
	const std::string hierarchy;

	// The callback
	managedhierarchymonitor callback;

	// Current connection's limits.

	userinit limits;

	// List of discovered directories in the hierarchy
	std::list<std::string> hierarchies;

	// Inherited from managedsubscriberObj
	// Requests the contents of the updated object.

	void updated(const std::string &objname,
		     const std::string &suffix) override;

	// Inherited from managedsubscriberObj.
	// Invokes callback->connection_update(). Also
	// invokes callback->begin() for a req_processed_stat, puts the
	// top level hierarchy in hierarchies, then calls
	// get_next_hierarchy().

	void connection_update(req_stat_t status) override;

	// Send request for the contents of the next hierarchy to scan, for
	// the initial contents of the hierarchy.

	void req_next_hierarchy();

	void req_next_hierarchy(const client &c);

public:

	//! Constructor
	implObj(const clientptr &clientArg,
		const std::string &hierarchyArg,
		const managedhierarchymonitor &callbackArg);

	//! Destructor
	~implObj();

	// Mcguffin callback that handles the response to the
	// connection_update()'s request for the hierarchy's current contents.

	class getdirCompleteMcguffinObj : virtual public x::obj {
	public:

		x::weakptr<x::ptr<implObj> > mon;
		getdirrequest req; // Original request

		getdirCompleteMcguffinObj(const x::ptr<implObj> &monArg,
					  const getdirrequest &reqArg)
			: mon(monArg), req(reqArg)
		{
		}

		~getdirCompleteMcguffinObj() {}

		// Directory request complete. Instantiate the first
		// processDirResultsObj, and kick it off to process the first
		// batch of results from the directory request.
		void destroyed();
	};

	// A mcguffin that sends a request for the contents of the next chunk
	// of the hierarchy's current objects.
	//
	// After the request for the current contents of the hierarchy
	// gets processed, the list of entries in the hierarchy gets iterated
	// over, sending chunks of requests for the individual entries in the
	// hierarchy. Take the limit of the maximum number of objects in a
	// contents() request. Take that many of the next set of entries from
	// the list returned by the directory request, and send it.
	//
	// Then, attach another mcguffin to that request, so when it's done,
	// the next request gets sent.

	class processDirResultsObj : virtual public x::obj {

	public:

		x::weakptr<x::ptr<implObj> > mon;

		// Results of the original request for the current contents
		// of the hierarchy.
		getdirresults results;

		processDirResultsObj(const x::ptr<implObj> &monArg,
				     const getdirresults &resultsArg)
			: mon(monArg), results(resultsArg)
		{
		}

		~processDirResultsObj()
		{
		}

		// Do the next chunk.
		void destroyed();
	};

	// Send a request to return the uuid of the given entry(es) in the
	// hierarchy.
	//
	// This gets invoked by the managed subscriber callback, when it's
	// notified that some entry has changed (added/modified/deleted).
	// Also gets invoked when the request for the initial contents of some
	// directory in the monitored hierarchy gets returned.
	// Send a request for the uuids of the objects, attach an
	// updateCallbackObj mcguffin that processes the results of the
	// contents request.

	static x::ptr<x::obj>
		update(// Monitored object
		       const x::ref<implObj> &mon,

		       // Objects to process.

		       // Will contain one name, when this is called by
		       // the managed subscribed callback when one entry
		       // gets returned. The name will always be an object
		       // name, and not end with a trailing /.
		       //
		       // When processing the initial subscription results, this
		       // will be a list of objects in a hierarchy directory.
		       // Sends a request for uuids for as many as we can,
		       // which get removed from unprocessed, accordingly.
		       std::set<std::string> &unprocessed);

	class updateCallbackObj : virtual public x::obj {

	public:
		x::weakptr<x::ptr<implObj> > mon;
		getrequest req;
		client::base::getreq origreq;

		updateCallbackObj(const x::ptr<implObj> &monArg,
				  const getrequest &reqArg,
				  const client::base::getreq &origreqArg)
			: mon(monArg), req(reqArg), origreq(origreqArg)
		{
		}

		~updateCallbackObj()
		{
		}

		void destroyed();
	};

};

x::ref<x::obj>
managerObj::manage_hierarchymonitor(const client &clientArg,
				    const std::string &hiername,
				    const managedhierarchymonitor &mon)
{
	return manage_subscription(clientArg,
				   hiername + (hiername.size() ? "/":""),
				   x::ref<managedhierarchymonitorObj::implObj>
				   ::create(clientArg, hiername, mon));
}

managedhierarchymonitorObj::managedhierarchymonitorObj()
{
}

managedhierarchymonitorObj::~managedhierarchymonitorObj()
{
}

managedhierarchymonitorObj::implObj::implObj(const clientptr &clientArg,
					     const std::string &hierarchyArg,
					     const managedhierarchymonitor
					     &callbackArg)
	: client_connection(clientArg), hierarchy(hierarchyArg),
	  callback(callbackArg)
{
}

managedhierarchymonitorObj::implObj::~implObj()
{
}

void managedhierarchymonitorObj::implObj::updated(const std::string &objname,
						  const std::string &suffix)
{
	std::set<std::string> justone;

	justone.insert(objname+suffix);

	implObj::update(x::ref<implObj>(this), justone);
}

void managedhierarchymonitorObj::implObj::connection_update(req_stat_t status)
{
	LOG_DEBUG("connection_update: " << x::to_string(status));

	callback->connection_update(status);
	if (status != req_processed_stat)
		return;

	auto c=client_connection.getptr();

	if (c.null())
	{
		LOG_TRACE("Client no longer available");
		return;
	}

	limits=c->getlimits();

	callback->begin();
	hierarchies.clear();
	hierarchies.push_back(hierarchy);
	req_next_hierarchy(c);
}

void managedhierarchymonitorObj::implObj::req_next_hierarchy()
{
	auto c=client_connection.getptr();

	if (c.null())
	{
		LOG_TRACE("Client no longer available");
		return;
	}

	req_next_hierarchy(c);
}

void managedhierarchymonitorObj::implObj::req_next_hierarchy(const client &c)
{
	if (hierarchies.empty())
	{
		LOG_DEBUG("End of directory listing");
		callback->enumerated();
		return;
	}

	std::string next_hierarchy=hierarchies.front();

	LOG_DEBUG("Requesting directory contents of " << next_hierarchy);

	hierarchies.pop_front();

	auto req=c->getdir_request(next_hierarchy);

	auto mcguffin=x::ref<getdirCompleteMcguffinObj>
		::create(x::ref<implObj>(this), req.first);

	req.second->mcguffin()->ondestroy([mcguffin]{mcguffin->destroyed();});
}

void managedhierarchymonitorObj::implObj::getdirCompleteMcguffinObj::destroyed()
{
	auto results=req->getmsg();
	auto m=mon.getptr();

	LOG_DEBUG("directory retrieve: " << x::to_string(results->status));

	if (m.null())
	{
		LOG_TRACE("Manager object no longer available");
		return;
	}

	if (results->status != req_processed_stat)
	{
		LOG_ERROR(m->hierarchy << ": "
			  << x::to_string(results->status));
		return;
	}

	// Kick it off
	x::ref<processDirResultsObj>::create(m, results)->destroyed();
}

void managedhierarchymonitorObj::implObj::processDirResultsObj::destroyed()
{
	auto m=mon.getptr();

	if (m.null())
	{
		LOG_TRACE("Manager object no longer available");
		return;
	}

	// Remove the subhierarchies from the results.

	for (auto e=results->objects.end(),
		     b=results->objects.begin(); b != e;)
	{
		auto p=b;
		++b;

		if (p->size() == 0 || *--p->end() == '/')
		{
			// Sub-hierarchy, add them to the list to do.

			std::string s=*p;

			results->objects.erase(p);
			if (s.size())
				s=s.substr(0, s.size()-1);

			LOG_DEBUG("Queueing " << s << " for retrieval");
			m->hierarchies.push_back(s);
		}
	}

	auto mcguffin=implObj::update(m, results->objects);

	if (mcguffin.null())
		return;

	auto cb=x::ref<processDirResultsObj>(this);

	mcguffin->ondestroy([cb]{cb->destroyed();});
}


x::ptr<x::obj> managedhierarchymonitorObj::implObj
::update(const x::ref<implObj> &mon,
	 std::set<std::string> &unprocessed)
{
	// Send in the next chunk of uuid updates.

	auto c=mon->client_connection.getptr();

	if (c.null())
	{
		LOG_TRACE("Client no longer available");
		return x::ptr<x::obj>();
	}

	if (mon->limits.maxobjects == 0)
	{
		LOG_TRACE("Client not fully initialized");
		return x::ptr<x::obj>();
	}

	LOG_DEBUG("Sending a request for uids of directory entries");

	auto req=client::base::getreq::create();

	for (size_t i=0; i<mon->limits.maxobjects; ++i)
	{
		if (unprocessed.empty())
			break;

		auto iter=unprocessed.begin();
		std::string s=*iter;
		unprocessed.erase(iter);

		LOG_TRACE("entry: " << s);
		req->objects.insert(s);
	}

	if (req->objects.empty())
	{
		LOG_TRACE("No entries left");
		// Processing the initial contents of a hierarchy, reached the
		// end of the current hierarchy's object, start the next
		// sub-hierarchy.
		mon->req_next_hierarchy();
		return x::ptr<x::obj>();
	}

	auto client_req=c->get_request(req);

	auto callback=x::ref<updateCallbackObj>::create(mon, client_req.first,
							req);

	client_req.second->mcguffin()->ondestroy([callback]
						 {callback->destroyed();});
	return callback;
}

void managedhierarchymonitorObj::implObj::updateCallbackObj::destroyed()
{
	contents resp=req->getmsg()->objects;

	if (!resp->succeeded)
	{
		LOG_ERROR(resp->errmsg);
		return;
	}

	LOG_DEBUG("entry contents retrieved");

	x::ref<implObj> c=({

			auto ptr=mon.getptr();

			if (ptr.null())
			{
				LOG_TRACE("Request cancelled");
				return;
			}

			ptr;
		});

	for (auto &object:origreq->objects)
	{
		auto info=resp->find(object);

		if (info == resp->end())
		{
			LOG_TRACE("Removed: " << object);
			c->callback->removed(object);
		}
		else
		{
			LOG_TRACE("Entry: " << object);
			c->callback->updated(object, info->second.uuid);
		}
	}
}

STASHER_NAMESPACE_END
