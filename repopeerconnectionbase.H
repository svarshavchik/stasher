/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repopeerconnectionbase_H
#define repopeerconnectionbase_H

#include <x/obj.H>
#include <x/ref.H>
#include <x/uuid.H>
#include <x/ondestroy.H>
#include <x/weakptr.H>
#include <x/mcguffinref.H>

class repocontrollerbaseObj;
class slavesyncinfoObj;
class mastersyncinfoObj;

//! Base class for connection object to repository peers

class repopeerconnectionbaseObj : virtual public x::obj {

public:

	//! Peer linkage object

	//! A reference to this object is received by connect_peer(), which gets
	//! invoked by the controller.
	//!
	//! If, in the context of the peer connection object's execution thread,
	//! the given master name and uuid matches the peer's status, a
	//! peerdisconnect_msgObj reference gets saved on the execution
	//! thread's stack, together with this object,
	//! using the information from here.
	//! Otherwise, this object gets discarded.
	//!
	//! When a nodeclusterstatus message gets received from the peer that
	//! names a different master/uuid then what's specified in the currently
	//! held peerlink object, the peer connection thread drops its
	//! reference to this object, and peerdisconnect_msgObj,
	//! from the execution stack.

	class peerlinkObj : virtual public x::obj {

	public:
		//! Constructor

		peerlinkObj(const x::ptr<repocontrollerbaseObj> &controllerArg,
			    const x::ptr<x::obj> &controller_mcguffinArg,
			    const std::string &masternameArg,
			    const x::uuid &masteruuidArg);


		//! Destructor
		~peerlinkObj();

		//! The controller

		//! The peer connection object should retrieve this object
		//! and keep a reference to it on its execution stack.

		x::weakptr<x::ptr<repocontrollerbaseObj> > controller;

		//! Controller's mcguffin

		//! When this object gets destroyed, the controller's execution
		//! thread has terminated, and the peer connection object must
		//! drop its reference to the controller.

		x::weakptr<x::ptr<x::obj> > controller_mcguffin;

		//! Master's name
		std::string mastername;

		//! Master's UUID
		x::uuid masteruuid;
	};

	//! Connect a controller with a peer

	//! This reference gets passed to repopeerconnectionObj::connect_peer,
	//! and is sent to the execution thread as a message.

	typedef x::ptr<peerlinkObj> peerlinkptr;

	//! Connection request from the controller

	virtual void connect_peer(const peerlinkptr &masterlinkArg)=0;

	//! Constructor
	repopeerconnectionbaseObj();

	//! Destructor
	~repopeerconnectionbaseObj();

	//! Invoked by slave controller, to establish linkage with a connection object to its master

	virtual void connect_slave(const x::ptr<slavesyncinfoObj> &synchandle)=0;

	//! Invoked by master controller, to establish linkage with a connection object to one of its slave

	virtual void connect_master(const x::ref<mastersyncinfoObj> &synchandle)=0;

	class peerdisconnect_msgObj;

	//! Disconnect message reference

	typedef x::ptr<peerdisconnect_msgObj> peerdisconnect_msg;

	//! Mechanism for disconnect from the controller object.

	//! When a peer accepts a peerlink request, a reference to this object
	//! is saved on the executing thread's stack, in addition to
	//! peerlinkObj. This is a subclass of x::mcguffinref that holds a
	//! reference to the controller object and sets up a destructor callback
	//! triggered by the controller's mcguffin.
	//!
	//! The mcguffin reference template clears the reference to the
	//! controller object, and the destroyed() callback invokes
	//! disconnect_peer(). When the connection thread receives
	//! disconnect_peer(), if the reference to the controller object is
	//! null, indicating that the message was sent from the currently
	//! connected peer, the peerlinkObj and this object reference get
	//! dropped.

	class peerdisconnect_msgObj :
		public x::mcguffinrefObj< x::ptr<repocontrollerbaseObj> > {

		//! Connection object that's connected to the controller
		x::weakptr<x::ptr<repopeerconnectionbaseObj> > connection_obj;

	public:
		//! Constructor
		peerdisconnect_msgObj(const x::ptr<repopeerconnectionbaseObj>
				      &objArg);

		//! Destructor
		~peerdisconnect_msgObj();

		//! Slave controller has been disconnected
		void destroyed(const x::ptr<repocontrollerbaseObj> &dummy)
			override;

	};

	//! The controller has disconnected

	virtual void disconnect_peer()=0;

	//! Helper template class for deserializing

	//! The deserialization API passes a reference to this object as
	//! to the constructor of the object being deserialized.
	//!
	//! The messages defined in objrepo just have a default constructor.
	//! This is a fix for this.

	template<typename obj_type> class deser : public obj_type {

	public:
		//! Dummy constructor

		template<typename dummy>
		deser(dummy &dummyRef)
		{
		}

		//! Destructor
		~deser()
		{
		}
	};
};


//! Reference to a peer connection

typedef x::ptr<repopeerconnectionbaseObj> repopeerconnectionbase;
#endif
