#include <iostream>
#include <type_traits>
#include <stasher/client.H>
#include <stasher/process_request.H>
#include <x/dequemsgdispatcher.H>
#include <x/destroycallbackflag.H>
#include <x/threads/run.H>
#include <x/fditer.H>

/*
  This is an example of a simple thread that supports unsophisticated object
  updates based solely on the existing object's contents (if any).

  First, use x::run() to start the thread, then install the client connection
  by invoking the installclient() message.

  The thread object defines an update() method that takes the name of an object,
  a functor, and a mcguffin.

  update() returns a reference to updateRequestObj(). The thread holds a
  reference on the mcguffin. When no other references remain, and the mcguffin
  goes out of scope and gets destroyed, updateRequestObj's status indicates
  the update result status.

  The functor takes two parameters, and must return a
  stasher::client::base::transaction. If the object already exists in the
  repository, the first parameter is a non-null x::fdptr(), and the second
  parameter is the object's x::uuid. If the object does not exist, the first
  parameter is a null x::fdptr(), and the x::uuid parameter can be ignored.
  The functor must construct the transaction object, using its parameters,
  and return it.

  update() returns the reference to updateRequestObj() immediately. The
  thread retrieves the object's contents from the repository, invokes
  the functor, then puts the transaction into the object repository. The
  mcguffin parameter to update() indicates the completion status.

  Note that it's possible that the functor gets invoked more than once. If
  the update failed with a stasher::req_rejected_stat, the update thread
  goes back to the starting point, gets the updated objects contents, and
  invokes the functor again, to build a new transaction object.
*/

class updateRequestObj : virtual public x::obj {

public:
	// The stored object name
	std::string objectname;

	// The status of the request, when it's done.
	stasher::req_stat_t status;

	updateRequestObj(const std::string &objectnameArg)
		: objectname(objectnameArg),

		  // By default, if everything comes apart, we're disconnected.
		  status(stasher::req_disconnected_stat)
	{
	}

	~updateRequestObj() noexcept
	{
	}

	virtual stasher::client::base::transaction
	update(const x::fdptr &obj, const x::uuid &uuid)=0;
};

// This is the thread that implements the object updates. It uses
// x::dequemsgdispatcherObj to execute a message queue.

class updateThreadObj : public x::dequemsgdispatcherObj,

			// Only one thread can use this object.
			public x::runthreadsingleton {

	// The client connection object, installed by installclient(), is
	// stored on the executing thread's stack, so that it automatically
	// goes out of scope and gets destroyed.
	//
	// This is a native pointer to the underlying client object.
	// It is only accessed by the executing thread. run() sets it to NULL,
	// and installclient() initializes it, when it gets the real connection
	// handle.
	decltype(&*stasher::clientptr()) client;

	// This is the actual client connection handle on the executing thread's
	// stack. It is set by installclient().
	stasher::clientptr *clientptr;

	// This is the template subclass of updateRequestObj that's constructed
	// by update(). It implements the virtual update() method that gets
	// called when the existing object's contents are retrieved. It
	// invokes the functor that was passed by the original update(),
	// and returns the resulting transaction.

	template<typename F>
	class updateRequestImplObj : public updateRequestObj {

		F functor;

	public:
		updateRequestImplObj(const std::string &objectnameArg,
				     F && functorArg)
			: updateRequestObj(objectnameArg),
			  functor(std::forward<F>(functorArg))
		{
		}

		~updateRequestImplObj() noexcept
		{
		}

		stasher::client::base::transaction
		update(const x::fdptr &obj, const x::uuid &uuid) override
		{
			return functor(obj, uuid);
		}
	};

public:
	updateThreadObj() {}
	~updateThreadObj() noexcept {}

	// A very basic thread run() method. Message-dispatching based threads
	// don't need much else.

	void run()
	{
		// Initialize the client connection handle pointers and
		// references, awaiting the installclient() message.

		stasher::clientptr clientptrRef;
		client=NULL;
		clientptr=&clientptrRef;

		while (1)
		{
			try {
				nextmsg()->dispatch();
			} catch (const x::stopexception &e)
			{
				return;
			} catch (const x::exception &e)
			{
				std::cerr << e << std::endl;
			}
		}
	}

	// update() constructs the request object, then sends an "updateobject"
	// message to the thread.

	template<typename F>
	x::ref<updateRequestObj> update(const std::string &objectname,
					F && functor,
					const x::ptr<x::obj> &mcguffin)
	{
		x::ref<updateRequestObj> req=
			x::ref<updateRequestImplObj<typename
						    std::decay<F>::type> >
			::create(objectname, std::forward<F>(functor));

		updateobject(req, mcguffin);
		return req;
	}
private:

	// Invoked by run() to retrieve the next message from the message queue.

	x::dispatchablebase nextmsg()
	{
		msgqueue_t::lock lock(msgqueue);

		lock.wait([&lock]
			  {
				  return !lock->empty();
			  });

		x::dispatchablebase msg=lock->front();

		lock->pop_front();
		return msg;
	}

	void do_update(const x::ref<updateRequestObj> &msg_request,
		       const x::ptr<x::obj> &msg_mcguffin);

#include "updatethread.msgs.all.H"
};

// The installclient message.

void updateThreadObj::dispatch(const installclient_msg &msg)
{
	*clientptr=msg.client;
	client= &*msg.client;
}


// The updateobject() method sends a get_request for the object's existing
// contents.

void updateThreadObj::dispatch(const updateobject_msg &msg)
{
	if (!client)
		return; // Not connected

	std::cout << "Received update request for " << msg.request->objectname
		  << ", checking its existing contents" << std::endl;

	do_update(msg.request, msg.mcguffin);
}

void updateThreadObj::do_update(const x::ref<updateRequestObj> &msg_request,
				const x::ptr<x::obj> &msg_mcguffin)
{
	stasher::client::base::getreq req=
		stasher::client::base::getreq::create();

	req->openobjects=true;
	req->objects.insert(msg_request->objectname);

	// Send a get_request, when done, send a processcontents message
	// to this thread, with the results.
	//
	// Need to capture pretty much everything by value. The functor gets
	// invoked from the client connection thread, of course.

	x::ref<updateThreadObj> me(this);

	stasher::process_request(client->get_request(req),
				 [msg_request, msg_mcguffin, me]
				 (const stasher::getresults &res)
				 {
					 me->processcontents(res->objects,
							     msg_request,
							     msg_mcguffin);
				 });
}

void updateThreadObj::dispatch(const processcontents_msg &msg)
{
	if (!client)
		return; // Not connected

	std::cout << "Received existing contents of "
		  << msg.request->objectname
		  << ", updating it" << std::endl;

	auto iter=msg.contents->find(msg.request->objectname);

	// Send a transaction put to the server. When it completes,
	// send the processupdate message to this thread.

	x::ref<updateThreadObj> me(this);
	auto req=msg.request;
	auto mcguffin=msg.mcguffin;

	stasher::process_request
		(client->put_request(iter == msg.contents->end()
				     ? req->update(x::fdptr(), x::uuid())
				     : req->update(iter->second.fd,
						   iter->second.uuid)),
		 [me, req, mcguffin]
		 (const stasher::putresults &res)
		 {
			 me->processupdate(res, req, mcguffin);
		 });
}

// Check the status of the completed update.

void updateThreadObj::dispatch(const processupdate_msg &msg)
{
	if (!client)
		return; // Not connected

	if ((msg.request->status=msg.putresults->status)
	    == stasher::req_rejected_stat)
	{
		std::cout << "Update of "
			  << msg.request->objectname
			  << " rejected because someone else updated it, trying again"
			  << std::endl;
		do_update(msg.request, msg.mcguffin);
	}

	// We're done. All we needed to do is set msg.request->status, and upon
	// exit, the request mcguffin goes out of scope.
}

// Process command line parameters.

x::ref<updateRequestObj> make_request(const x::ref<updateThreadObj> &thread,
				      const std::string &objectname,
				      size_t counter,
				      const x::ref<x::obj> &mcguffin);

void update(const std::list<std::string> &args)
{
	x::destroyCallbackFlag::base::guard guard;

	x::ref<updateThreadObj> thread=x::ref<updateThreadObj>::create();

	x::stoppable::base::group stoppable_group=
		x::stoppable::base::group::create();

	x::ref<x::obj> mcguffin=x::ref<x::obj>::create();

	// When the mcguffin goes out of scope and gets destroyed, invoke
	// thread->stop().

	stoppable_group->add(thread);
	stoppable_group->mcguffin(mcguffin);

	// And make sure that the thread object gets completely destroyed,
	// before we're done.
	guard(thread);

	// Start the thread, connnect it to the object repository.

	x::run(thread);
	thread->installclient(stasher::client::base::connect());

	size_t counter=0;

	// Keep track of the created requests.

	std::list<std::pair<x::ref<x::obj>, x::ref<updateRequestObj> > >
		requests;

	// Dump all the transactions on the thread
	for (auto &object:args)
	{
		x::ref<x::obj> mcguffin=x::ref<x::obj>::create();

		x::ref<updateRequestObj> req=
			make_request(thread, object, ++counter, mcguffin);
		requests.push_back(std::make_pair(mcguffin, req));
	}

	// And wait for each one to finish

	while (!requests.empty())
	{
		x::ref<updateRequestObj> req=requests.front().second;

		// Use the guard to wait for the mcguffin to go out of scope
		// and get destroyed, indicating that the transaction finished.
		{
			x::destroyCallbackFlag::base::guard tran_guard;

			tran_guard(requests.front().first);

			requests.pop_front();
		}

		std::cout << x::tostring(req->status) << std::endl;
	}
}

// Invoke the thread object's update() with a simple functor.
// The functor appends "Object #n" to the object in the repository,
// creating it if it does not exist.

x::ref<updateRequestObj> make_request(const x::ref<updateThreadObj> &thread,
				      const std::string &objectname,
				      size_t counter,
				      const x::ref<x::obj> &mcguffin)
{
	return thread
		->update(objectname,
			 [counter, objectname]
			 (const x::fdptr &fd, const x::uuid &uuid)
			 {
				 stasher::client::base::transaction tr=
					 stasher::client::base::transaction
					 ::create();

				 std::ostringstream o;

				 // Note that if fd is null(),
				 // x::fdinputiter(fd) is the ending iterator,
				 // same as x::fdinputiter(). How convenient.
				 o << std::string(x::fdinputiter(fd),
						  x::fdinputiter())
				   << "Object #" << counter << std::endl;

				 std::string s=o.str();

				 if (fd.null())
					 tr->newobj(objectname, s);
				 else
					 tr->updobj(objectname, uuid, s);
				 return tr;
			 }, mcguffin);
}

// This example takes a list of object specified on the command line, and
// updates them.
//
// "Object #1" gets written into the first object, the second one has
// "Object #2" written to it, and so on.
//
// If the object already exists, this text gets appended to the existing
// object's contents.
//
// The same object can be listed more than once. This results in the object
// getting each corresponding "Object #n" appended to it. Although the
// expectation is that they'll be in order, sometimes they won't be, for
// for the following reasons.
//
// Each listed object on the command line gets processed individually, as
// described by previous contents, each update gets executed by, first, getting
// the object's existing contents, if it exists already. The update is based
// on the object's existing contents.
//
// Consider the following sequence of events, where the same object name
// gets specified three times:
//
// - Initial contents of the object: empty.
//
// - The main execution thread sends the first two instances to the thread,
// then loses the CPU.
//
// - Instance 1 sends get request #1.
//
// - Instance 2 sends get request #2.
//
// - Get request #1 received, empty contents, "Object #1" update gets sent.
//
// - The main execution thread regains CPU, sends the third instance of the
// same object name to the update thread.
//
// - Instance 3 sends get request #3.
//
// - "Object #1" appended to the object.
//
// - Get request #2 received, empty contents, because it crossed the
// "Object #1" update response, at the same time, between the client and the
// repository server. "Object #2" update gets sent.
//
// - Get request #3 received, existing contents "Object #1" (the first update
// was processed on the server), so in response to that, "Object #1/Object #3"
// update gets sent to the server.
//
// - The sole "Object #2" update gets rejected, because it carried the
// (nonexistent) uuid of the empty object that it was based on. The update
// thread tries again.
//
// - "Object #3" appended the object.
//
// - The retried update for Object #2 proceeds along.
//
// The end result is the object containing "Object #1/Object #3/Object #2",
// without breaking any laws.
//
// This can happen if the same object name gets listed at least three times.
// This can happen if the same object name appears twice if some other
// application also modifies the object, at the same time.

int main(int argc, char **argv)
{
	try {
		update(std::list<std::string>(argv+1, argv+argc));
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}

	return 0;
}
