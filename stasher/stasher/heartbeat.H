/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_heartbeat_H
#define stasher_heartbeat_H

#include <x/serialize.H>
#include <x/deserialize.H>
#include <x/logger.H>
#include <x/ref.H>
#include <x/ptr.H>
#include <x/fditer.H>
#include <x/destroy_callback.H>
#include <x/property_properties.H>
#include <x/threads/timertask.H>

#include <stasher/versionedcurrent.H>
#include <stasher/versionedput.H>
#include <stasher/manager.H>
#include <stasher/client.H>
#include <stasher/process_request.H>

#include <map>
#include <string>
#include <utility>
#include <functional>
#include <sstream>
#include <time.h>

STASHER_NAMESPACE_START

//! Each node's posted heartbeat metadata

//! The template parameter is a serializable class that represents each
//! node's heartbeat metadata. This is, basically, just a tuple consisting of
//! a timestamp and an instance of the metadata class.
//!
//! The template class must support x::tostring().

template<typename T> class heartbeat_meta_t {

public:
	//! When this metadata was posted.

	time_t timestamp;

	//! Node's metadata
	T meta;

	//! Constructor
	heartbeat_meta_t(time_t timestampArg, const T &metaArg)
		: timestamp(timestampArg), meta(metaArg)
	{
	}

	//! Default constructor, used when deserializing the heartbeat object.

	heartbeat_meta_t()
	{
	}

	//! Destructor
	~heartbeat_meta_t()
	{
	}

	//! Metadata update.

	void update(//! New timestamp
		    time_t timestampArg,

		    //! New metadata
		    const T &metaArg)
	{
		timestamp=timestampArg;
		meta=metaArg;
	}

	//! Use a functor to update the metadata

	template<typename Functor>
	void update_with(//! Timestamp
			 time_t timestampArg,

			 //! Functor that gets invoked, and returns the new metadata.
			 Functor &functor)
	{
		timestamp=timestampArg;
		meta=functor();
	}

	//! Serialize the metadata.

	template<typename iter_type> void serialize(iter_type &iter)
	{
		iter(timestamp);
		iter(meta);
	}

	//! Convert the metadata to a string, then write it to an output stream.

	//! Four spaces get prepended to each line, for indentation. If the
	//! metadata's string is not empty, an extra line gets appended.

	void report(std::ostream &o)
	{
		std::istringstream i(x::tostring(meta));
		bool flag=false;
		std::string s;

		while (!std::getline(i, s).eof() || s.size() > 0)
		{
			o << "    " << s << std::endl;
			flag=true;
			s.clear();
		}
		if (flag)
			o << std::endl;
	}
};

//! Specialization for a void metadata.

//! This means that the heartbeat consists of just the timestamp, with no
//! futher data needed, other than the fact that it's alive.

template<>
class heartbeat_meta_t<void> {

public:
	//! When this metadata was posted.

	time_t timestamp;

	//! Constructor

	heartbeat_meta_t(time_t timestampArg)
		: timestamp(timestampArg)
	{
	}

	//! Default constructor, used when deserializing the heartbeat object.

	heartbeat_meta_t()
	{
	}

	//! Destructor

	~heartbeat_meta_t()
	{
	}

	//! Metadata update.

	void update(time_t timestampArg)
	{
		timestamp=timestampArg;
	}

	//! Use a functor to update the metadata

	//! For the void specialization, the functor gets invoked, and that's
	//! pretty much it.

	template<typename Functor>
	void update_with(time_t timestampArg, Functor &functor)
	{
		timestamp=timestampArg;
		functor(); // Ignored
	}

	//! Serialize the metadata.

	template<typename iter_type> void serialize(iter_type &iter)
	{
		iter(timestamp);
	}

	//! There is no metadata, so nothing gets reported by this specialization
	void report(std::ostream &o)
	{
	}
};

//! Collected heartbeat metadata of all nodes

//! The metadata is stored in an ordinary map, and this is just a subclass
//! of a std::map, that adds a convenient update() method.

template<typename I, typename T> class heartbeat_map_t
	: public std::map<I, heartbeat_meta_t<T> > {
public:

	//! Update the metadata of a particular node.

	//! If the map contains this node already, the existing metadata
	//! gets replaced, otherwise the node gets added.

	template<typename ...Args>
	void update(//! Node name
		    const I &s,

		    //! Metadata timestamp
		    time_t timestamp,

		    //! The metadata. No parameters should be provided for the
		    //! void specialization, otherwise this must be one
		    //! parameter, exactly.
		    Args && ...args)
	{
		auto iter=this->find(s);

		if (iter == this->end())
			this->insert(std::make_pair(s, heartbeat_meta_t<T>
						    (timestamp,
						     std::forward<Args>(args)...
						     )));
		else
			iter->second.update(timestamp,
					    std::forward<Args>(args)...);
	}

	//! Serialize the entire heartbeat object.

	template<typename iter_type> void serialize(iter_type &iter)
	{
		iter(static_cast<std::map<I, heartbeat_meta_t<T> > &>
		     (*this));
	}
};

class heartbeatUpdateRequestBaseObj;

//! The base class for a reference-counted current heartbeat object

//! This is a superclass of
//! \c INSERT_STASHER_NAMESPACE::currentHeartbeat<T>::base, a.k.a
//! \ref heartbeatBase "INSERT_LIBX_NAMESPACE::heartbeatBase"
//! that defines members that do not depend on the heartbeat metadata type.

class heartbeatCommonBase : public x::ptrrefBase {

	//! The logger class
	LOG_CLASS_SCOPE;
public:
	//! An internal logging function

	//! \internal
	static void log_corrupted();

	//! An internal logging function

	//! \internal
	static void log_initial();

	//! An internal logging function

	//! \internal
	static void log_new_heartbeat();

	//! An internal logging function

	//! \internal
	static void log_updating_heartbeat();

	//! An internal logging function

	//! \internal
	static void log_stale_heartbeat(const std::string &nodename);

	//! An internal logging function

	//! \internal

	static void log_next_expiration(const std::string &nodename,
					time_t my_expiration);
	//! An internal logging function

	//! \internal

	static void log_update_processed(stasher::req_stat_t status);

	//! Heartbeat update type

	typedef enum {
		initial_update,
		subsequent_update,
		periodic_update,
		collision_update,
		app_update,
	} update_type_t;

	//! Helper function for creating the timer task.

	//! \internal
	static x::timertask
	create_timer_task(const x::ref<heartbeatUpdateRequestBaseObj> &update);
};

//! Base class for references to a heartbeat objcet.

//! Refer to this class as
//! \c INSERT_STASHER_NAMESPACE::current_heartbeat<T>::base
//! or \c INSERT_STASHER_NAMESPACE::current_heartbeat<T>::base.

template<typename I, typename T>
class current_heartbeatBaseObj : public x::ptrrefBase {

public:

	//! Definion of the heartbeat std::map.

	typedef heartbeat_map_t<I, T> timestamps_t;
};

//! The current value of the heartbeat object in the repository.

template<typename I, typename T>
class current_heartbeatObj : virtual public x::obj {

public:
	//! This object's uuid in the repository

	x::uuid uuid;

	//! Each node's timestamp

        typename current_heartbeatBaseObj<I, T>::timestamps_t timestamps;

	//! Default constructor
	current_heartbeatObj()
	{
	}

	//! Construct from retrieved object file descriptor
	current_heartbeatObj(const x::uuid &uuidArg, const x::fd &fdArg)
		: uuid(uuidArg)
	{
		x::fdinputiter iter(fdArg), iter_end;

		x::deserialize::iterator<x::fdinputiter>
			deser_iter(iter, iter_end);

		deser_iter(timestamps);
	}

	//! Corrupted object, construct an empty heartbeat instance
	current_heartbeatObj(const x::uuid &uuidArg) : uuid(uuidArg)
	{
		heartbeatCommonBase::log_corrupted();
	}

	//! Create a string to go into the repository
	std::string toString() const
	{
		std::string s;
		std::back_insert_iterator<std::string> iter(s);

		x::serialize::iterator<std::back_insert_iterator<std::string> >
			ser_iter(iter);

		ser_iter(timestamps);
		return s;
	}

	//! Destructor
	~current_heartbeatObj() {}

	//! Update a node's heartbeat

	template<typename ...Args>
	void update(//! For this node
		    const I &nodename,

		    //! Update timestamp
		    time_t timestamp,

		    //! For non-void metadata, one argument must be supplied.
		    Args && ...args)
	{
		auto iter=timestamps.find(nodename);

		if (iter == timestamps.end())
		{
			heartbeat_meta_t<T> meta;

			meta.update(timestamp,
				    std::forward<Args>(args)...);

			timestamps.insert(std::make_pair(nodename, meta));
		}
		else
		{
			iter->second.update(timestamp,
					    std::forward<Args>(args)...);
		}
	}
};

//! The deserialized versioned heartbeat object in the repository

template<typename I, typename T>
#ifdef DOXYGEN
class current_heartbeat {}; // TODO
#else
using current_heartbeat=x::ref<current_heartbeatObj<I, T>,
			       current_heartbeatBaseObj<I, T> >;
#endif

//! A nullable reference pointer to the deserialized versioned heartbeat object.
template<typename I, typename T>
#ifdef DOXYGEN
class current_heartbeatptr {}; // TODO
#else
using current_heartbeatptr=
	x::ptr<current_heartbeatObj<I, T>, current_heartbeatBaseObj<I, T> >;
#endif

//! Base class for the heartbeat update request functor

//! heartbeatObj's constructor instantiates a subclass of this
//! reference-counted object that holds the heartbeat request functor.

class heartbeatUpdateRequestBaseObj : virtual public x::obj {

public:
	//! Constructor
	heartbeatUpdateRequestBaseObj()
	{
	}

	//! Destructor
	~heartbeatUpdateRequestBaseObj()
	{
	}

	//! Request the application to invoke heartbeatObj::update().

	//! The subclass invokes the application-provided functor.
	virtual void update(heartbeatCommonBase::update_type_t)=0;
};

//! An application-provided functor that receives a heartbeat request update.

template<typename UpdateFunctor>
class heartbeatUpdateRequestObj : public heartbeatUpdateRequestBaseObj {

public:
	//! The saved application functor

	typename std::enable_if<
		!std::is_lvalue_reference<UpdateFunctor>::value &&
	!std::is_rvalue_reference<UpdateFunctor>::value, UpdateFunctor >::type
		updatefunctor;

	//! Constructor
	heartbeatUpdateRequestObj(UpdateFunctor && UpdateFunctorArg)
		: updatefunctor(std::move(UpdateFunctorArg))
	{
	}

	//! Destructor
	~heartbeatUpdateRequestObj()
	{
	}

	//! Invoke the functor.

	void update(heartbeatCommonBase::update_type_t update_type) override
	{
		updatefunctor(update_type);
	}
};

//! Base class for INSERT_STASHER_NAMESPACE::heartbeatObj<T>

//! Refer to this class as \c INSERT_STASHER_NAMESPACE::heartbeatObj<T>::base
//!

template<typename I, typename T>
class heartbeatBase : public heartbeatCommonBase {

public:

	//! The versioned current object that talks to the object repository.

	typedef x::ref<stasher::currentObj<current_heartbeatptr<I, T>,
					   stasher::versionedptr<current_heartbeatptr<I, T> >
					   > > current_t;

	//! Pull in the current versioned lock type.

	typedef typename current_t::obj_type::current_value_t::lock lock;

	//! Pull in the timestamps type

	typedef typename current_heartbeatBaseObj<I, T>::timestamps_t
	timestamps_t;
};

//! Maintain heartbeats of the application running on different nodes

//! When an application runs on multiple nodes in the cluster, use this
//! object to periodically post an update of the that particular application's
//! instance to a shared heartbeat object, exchanging its heartbeat status
//! with other instances of the application on other nodes.
//!
//! Each application's instance carries a unique name, which would typically
//! be the name of the object repository node the instance runs on.
//!
//! The template parameter is a class that represent each instance's metadata.
//! The class must be serializable, and support x::tostring(). A template
//! parameter of \c void indicates that no per-instance metadata is needed,
//! and the heartbeat consists of only the heartbeat timestamps.

template<typename I, typename T>
class heartbeatObj : virtual public x::obj {

	//! Convenience typedef

	typedef typename heartbeatBase<I, T>::current_t current_t;

	//! Wait until the versioned current object is completely destroyed

	//! \internal
	//! The destructor makes sure that all pending updates have been fully
	//! flushed out.
	x::destroy_callback::base::guard guard;

	//! The application update request handler callback.

	//! \internal
	x::ref<heartbeatUpdateRequestBaseObj> updatereq;

	//! The versioned current object for the current heartbeat

	//! \internal
	current_t current;

	//! Heartbeat refresh interval

	//! \internal
	x::timer::base::duration_property_t timeout_duration;

	//! Stale heartbeat purge interval

	//! \internal
	x::timer::base::duration_property_t stale_duration;

	//! Managed version object subscription mcguffin

	//! \internal
	x::ref<x::obj> manage_mcguffin;

	//! The heartbeat refresh periodic timer task

	//! \internal
	x::timertask timer_task;

	//! The heartbeat refresh periodic timer task's cancellation mcguffin

	//! \internal
	x::ref<x::obj> timer_mcguffin;

	//! The client connection

	//! \internal
	stasher::client client;

	//! The object name in the repository for the heartbeat

	//! \internal
	std::string objname;

	//! This instance's name.

	//! \internal
	I nodename;

	//! A mcguffin for the current periodic heartbeat update in progress

	//! \internal
	x::weakptr<x::ptr<x::obj> > periodic_update_mcguffin;

	//! A mcguffin for the current application heartbeat update in progress

	//! \internal
	x::weakptr<x::ptr<x::obj> > app_update_mcguffin;

	//! Helper function for creating the versioned current object.

	//! \internal
	static inline current_t
	create_current(const x::ref<heartbeatUpdateRequestBaseObj> &update)
	{
		return stasher::make_versioned_current<current_heartbeatptr<I,
									    T> >
			( [update]
			  (const current_heartbeatptr<I, T> &val,
			   bool isinitial)
			  {
				  if (isinitial)
					  heartbeatCommonBase::log_initial();
				  update->update(isinitial ? heartbeatCommonBase
						 ::initial_update
						 : heartbeatCommonBase
						 ::subsequent_update);
			  } );
	}
public:

	//! Allow the lock class instance to take this object as a parameter.

	operator decltype(current->current_value) &()
	{
		return current->current_value;
	}

	//! Construct a heartbeat object instance
	template<typename RequestFunctor,
		 typename defaultTimeoutType,
		 typename defaultStaleType>
	heartbeatObj(//! This object's manager
		     const stasher::manager &managerArg,

		     //! The connection to the object repository server
		     const stasher::client &clientArg,

		     //! The heartbeat object's name
		     const std::string &objnameArg,

		     //! This instance's name
		     const I &nodenameArg,

		     //! Application property that specifies the refresh interval
		     const std::string &defaultTimeoutProp,

		     //! Default refresh interval
		     const defaultTimeoutType &defaultTimeout,

		     //! Application property that specifies the stale purge timeout
		     const std::string &defaultStaleProp,

		     //! Default refresh interval
		     const defaultStaleType &defaultStale,

		     //! Application callback to request a heartbeat update
		     RequestFunctor &&req)
		: updatereq( x::ref<heartbeatUpdateRequestObj
			     <RequestFunctor> >
			     ::create(std::move(req))),
		current(create_current(this->updatereq)),
		timeout_duration(defaultTimeoutProp, defaultTimeout),
		stale_duration(defaultStaleProp, defaultStale),

	// Start a managed subscription

		manage_mcguffin((guard(current), // First order of business!
				 this->current->manage(managerArg,
						       clientArg,
						       objnameArg))),

	// Make arrangements to call the update functor periodically

		timer_task(heartbeatCommonBase::create_timer_task(this->updatereq)
			   ),

		timer_mcguffin(this->timer_task->autocancel()),
		client(clientArg),
		objname(objnameArg),
		nodename(nodenameArg)
	{
		x::timer::base::global()
			->scheduleAtFixedRate(timer_task,
					      defaultTimeoutProp,
					      defaultTimeout);
	}

	//! Destructor
	~heartbeatObj()
	{
	}

	//! Administrative request to drop another instance from the heartbeat object
	template<typename Functor>
	void admin_drop(//! Remove this node
			const I &node,

			//! Status callback, gets invoked with two parameters, a bool and a std::string

			//! bool: true if the object was removed, false if
			//! failed, for some reason. std::string: the status
			//! message or an error message
			Functor &&callback)
	{
		typename heartbeatBase<I, T>::lock
			lock(current->current_value);

		if (lock->value.null())
		{
			callback(false, "Not yet initialized");
			return;
		}

		auto &timestamps=lock->value->timestamps;

		auto p=timestamps.find(node);

		if (p == timestamps.end())
		{
			callback(false, node + ": node unknown");
			return;
		}

		if (p->second.timestamp > time(NULL))
		{
			callback(false, node + ": node heartbeat not yet stale");
			return;
		}

		timestamps.erase(p);

		auto tran=stasher::client::base::transaction::create();

		tran->updobj(objname,
			     lock->value->uuid,
			     lock->value->toString());

		auto c_cpy=std::move(callback);

		stasher::process_request(client->put_request(tran),
					 [c_cpy]
					 (const stasher::putresults &res)
					 {
						 c_cpy(res->status
						       == stasher::
						       req_processed_stat,
						       x::tostring(res->
								   status));
					 });
	}

	//! Format a report on the heartbeat's status
	void report(//! Report's output stream
		    std::ostream &o)
	{
		typename heartbeatBase<I, T>::lock
			lock(current->current_value);

		if (lock->value.null())
		{
			o << "Heartbeat status not received yet" << std::endl;
			return;
		}

		o << "Heartbeat status (uuid "
		  << x::tostring(lock->value->uuid)
		  << "):" << std::endl;

		size_t max_len=0;

		for (auto &timestamp:lock->value->timestamps)
		{
			size_t s=x::tostring(timestamp.first).size();

			if (s > max_len)
				max_len=s;
		}

		for (auto &timestamp:lock->value->timestamps)
		{
			o << "    "
			  << std::setw(max_len)
			  << x::tostring(timestamp.first)
			  << std::setw(0)
			  << " expires on "
			  << (std::string)x::ymdhms(timestamp.second
						    .timestamp)
			  << std::endl;
			timestamp.second.report(o);
		}
	}

	//! Update the heartbeat object

	//! The application should invoke update() as a result of the
	//! callback to its request functor. update() sets up the update,
	//! then invokes the update functor callback to retrieve the current
	//! heartbeat metadata for this node.
	//!
	//! The \c update_type parameter that the application received via the
	//! received callback should be passed through here.
	//!
	//! Depending on the update type, and other situations, the
	//! heartbeat update may not take place.
	template<typename ...Args>
	void update(//! The update type originally received by the application's request callback functor
		    heartbeatCommonBase::update_type_t update_type,

		    //! For non-void metadata heartbeats, a single argument, this node's current heartbeat

		    Args && ...args)
	{
		if (update_type == heartbeatCommonBase::subsequent_update)
			// A node (possibly this one) updated its own heartbeat,
			// we don't need to do anything.
			return;

		x::ptr<x::obj> request_mcguffin;

		// One periodic/app update at a time, please.

		if (update_type == heartbeatCommonBase::periodic_update)
		{
			auto mcguffin=periodic_update_mcguffin.getptr();

			if (!mcguffin.null())
				return; // Another one is stuck somewhere

			auto new_mcguffin=x::ref<x::obj>::create();

			periodic_update_mcguffin=new_mcguffin;
			request_mcguffin=new_mcguffin;
		}

		if (update_type == heartbeatCommonBase::app_update)
		{
			auto mcguffin=app_update_mcguffin.getptr();

			if (!mcguffin.null())
				return; // Another one is stuck somewhere

			auto new_mcguffin=x::ref<x::obj>::create();

			app_update_mcguffin=new_mcguffin;
			request_mcguffin=new_mcguffin;
		}

		// We'll update our heartbeat every heartbeat_interval.
		// Announce our expiration date as a little bit beyond that,
		// to give us some extra breathing room.

		time_t my_expiration =
			std::chrono::duration_cast<std::chrono::seconds>
			(timeout_duration.getDuration()).count();

		my_expiration +=
			(my_expiration/2) > 120 ? 120:(my_expiration/2);

		my_expiration += time(NULL);

		heartbeatCommonBase::log_next_expiration(x::tostring(nodename),
							 my_expiration);

		auto tran=stasher::client::base::transaction::create();

		// Hold a lock on the current value while preparing the
		// transaction.

		typename heartbeatBase<I, T>::lock
			lock(current->current_value);

		current_heartbeatptr<I, T> existing_heartbeatptr=lock->value;

		if (existing_heartbeatptr.null())
		{
			heartbeatCommonBase::log_new_heartbeat();

			auto new_heartbeat=current_heartbeat<I, T>::create();

			new_heartbeat->update(nodename, my_expiration,
					      std::forward<Args>(args)...);

			tran->newobj(objname,
				     new_heartbeat->toString());
		}
		else
		{
			current_heartbeat<I, T> existing_heartbeat
				=existing_heartbeatptr;

			heartbeatCommonBase::log_updating_heartbeat();

			// Purge stale heartbeats

			time_t t=time(NULL);
			time_t stale=std::chrono::duration_cast
				<std::chrono::seconds>
				(stale_duration.getDuration()).count();

			for (auto b=existing_heartbeat->timestamps.begin(),
				     e=existing_heartbeat->timestamps.end();
			     b != e; )
			{
				auto p=b;

				++b;

				if (p->second.timestamp + stale > t)
					continue;

				heartbeatCommonBase
					::log_stale_heartbeat(x::tostring
							      (p->first));
				existing_heartbeat->timestamps.erase(p);
			}

			existing_heartbeat->update(nodename, my_expiration,
						   std::forward<Args>(args)...);

			tran->updobj(objname,
				     existing_heartbeat->uuid,
				     existing_heartbeat->toString());
		}

		auto update_cpy=updatereq;

		stasher::versioned_put_request_from
			(client, tran,
			 [update_cpy, request_mcguffin]
			 (const stasher::putresults &res)
			 {
				 heartbeatCommonBase::log_update_processed
					 (res->status);

				 if (res->status == stasher::req_rejected_stat)
					 update_cpy->update(heartbeatCommonBase
							    ::collision_update);
				 // Again, handle collisions.
			 },

			 // Versioned object that went into the
			 // transaction.
			 lock->value);
	}
};

//! A heartbeat object

//! \code
//!
//! class myMeta {
//!
//! // ...
//!
//! };
//!
//! void myThreadObj::run()
//! {
//!     INSERT_STASHER_NAMESPACE::manager manager=INSERT_STASHER_NAMESPACE::manager::create();
//!
//!     INSERT_STASHER_NAMESPACE::client client=INSERT_STASHER_NAMESPACE::client::base::connect();
//!
//!     auto me=x::ref<myThreadObj>(this);
//!
//!     INSERT_STASHER_NAMESPACE::heartbeat<int, myMeta> heartbeat=
//!         INSERT_STASHER_NAMESPACE::heartbeat<int, myMeta>::create(manager, client,
//!                 "heartbeat", 1,
//!                 L"refresh_interval", std::chrono::minutes(10),
//!                 L"stale_interval", std::chrono::minutes(30),
//!                 [me]
//!                 (INSERT_STASHER_NAMESPACE::heartbeat<int, myMeta>
//!                             ::base::update_type_t update_type)
//!                 {
//!                     me->update_heartbeat_request(update_type);
//!                 });
//!
//!     this->heartbeatptr= &heartbeat;
//!
//! // ...
//!
//! }
//!
//! void myThreadObj::dispatch(const update_heartbeat_request_msg &msg)
//! {
//!     (*heartbeatptr)->update(msg.update_type, this->current_meta);
//! }
//!
//! // ...
//!
//! INSERT_STASHER_NAMESPACE::heartbeat<int, myMeta>::base::lock lock(**heartbeatptr);
//!
//! if (!lock->value.null())
//! {
//!    INSERT_STASHER_NAMESPACE::current_heartbeat<int, myMeta> heartbeat(lock->value);
//!
//!    INSERT_STASHER_NAMESPACE::heartbeat<int, myMeta>::base::timestamps_t &timestamps=heartbeat->timestamps;
//!
//!    for (auto &timestamp: timestamps)
//!    {
//!        std::string node=timestamp.first;
//!        time_t t=timestamp.second.timestamp;
//!        myMeta &meta=timestamp.second.meta;
//!    }
//! }
//! \endcode

template<typename I, typename T>
#ifdef DOXYGEN
class heartbeat {}; // TODO
#else
using heartbeat=x::ref<heartbeatObj<I, T>, heartbeatBase<I, T> >;
#endif

//! A nullable reference pointer to a \ref heartbeat "heartbeat" object.

template<typename I, typename T>
#ifdef DOXYGEN
class heartbeatptr {}; // TODO
#else
using heartbeatptr=x::ptr<heartbeatObj<I, T>, heartbeatBase<I, T> >;
#endif

STASHER_NAMESPACE_END

#endif
