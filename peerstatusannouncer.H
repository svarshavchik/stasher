/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef peerstatusannouncer_H
#define peerstatusannouncer_H

#include "fdobjrwthread.H"
#include "peerstatus.H"
#include "writtenobj.H"

//! Exchange this node's status with our peers

//! This subclass multiply inherits from fdobjrwthread and peerstatusObj.
//!
//! The constructor takes the name of the peer, that's used for the writer
//! thread name in
//! fdobjrwthreadObj. This sets the name of the writer thread that gets
//! created when this connection status gets brought up, from the peer's name.
//!
//! The subclass is responsible for invoking install() which is inherited from
//! peerstatusObj.
//!
//! This template class implements:
//!
//! - the initialstatus() callback that gets invoked from install(), that
//! sets this thread's view of what this node's current status is.
//! This sets the initial value for \ref thisstatus "thisstatus".
//!
//! - statusupdated() message that gets invoked when this node's status changes.
//! This results in the nodeclusterstatus message queued up for this thread,
//! and the thisstatusupdated() virtual function gets invoked.
//!
//! - dispatch() for the nodeclusterstatus message, invoked in the context
//! of the thread. The message updates \ref thisstatus "thisstatus", then
//! writes the nodeclusterstatus message to the peer.
//!
//! - the deserialized() method for the nodeclusterstatus message, and
//! the classlist() deserialization method. When the nodeclusterstatus message
//! is received from the peer, peerstatusupdate() gets invoked.
//!
//! - started(), inherited from fdobjrwthreadObj. Sends the initial status
//! of this node to the peer.
//!
//! \par Starting this thread
//!
//! Thread instantiation procedure is the same as fdobjrwthreadObj's.
//!
//! \par Functionality
//!
//! - When this execution threads starts, the status that was specified via
//! initialstatus() gets sent to the peer as a \c nodeclusterstatus message.
//! [INITIALSTATUS]
//!
//! - Any time a status gets specified by statusupdate(), it gets sent to the
//! peer as a \c nodeclusterstatus message. [UPDATEDSTATUS]
//!
//! - A nodeclusterstatus message that gets received from the peer is passed
//! to peerstatusObj::peerstatusupdate(). [RECVUPDATE]
//!

class peerstatusannouncerObj
	: public STASHER_NAMESPACE::fdobjrwthreadObj, public peerstatusObj {


protected:
	//! This node's status, in the executing thread's context.

	nodeclusterstatus thisstatus;

	void statusupdated(const nodeclusterstatus &newStatus) override;

	//! Set the initial status of this node, when installing it into the cluster

	void initialstatus(const nodeclusterstatus &newStatus) override;


private:
	//! The current status of this node has just been updated.

	void sendthisstatus();

	//! The thread has started, announce the initial status.

	void started();

	//! Deserialization wrapper for nodeclusterstatus

	class deser_nodeclusterstatus : public nodeclusterstatus {

	public:
		//! Constructor

		template<typename obj_type>
		deser_nodeclusterstatus(obj_type &dummy) {}

		//! Destructor
		~deser_nodeclusterstatus() {}
	};

public:
	//! Deserialization classlist enumerator

	template<typename iter_type>
	static void classlist(iter_type &iter)
	{
		iter.template serialize<nodeclusterstatus,
					deser_nodeclusterstatus>();
	}

	//! Deserialized a nodeclusterstatus message from peer

	void deserialized(const nodeclusterstatus &newStatus);


	//! Constructor
	peerstatusannouncerObj(const std::string &writeThreadNameArg);


	//! Destructor
	~peerstatusannouncerObj();

private:
	//! This node's status has been updated

	virtual void thisstatusupdated();

	//! The actual main loop

	MAINLOOP_DECL;

#include "peerstatusannouncer.msgs.H"
};

#endif
