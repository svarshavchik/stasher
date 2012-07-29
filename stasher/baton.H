/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef baton_H
#define baton_H

#include "objrepo_config.h"
#include "tobjrepo.H"
#include "batonfwd.H"
#include "repocontrollermasterfwd.H"
#include "repopeerconnectionfwd.H"

#include <x/obj.H>
#include <x/mpobj.H>

//! Transferring the baton

//! This is the process of orderly transition to a different master.
//!
//! A master controller receives a message requesting it to transfer master
//! authority to another peer.
//!
//! The master controller starts a short-lived thread that, first, obtains
//! a commit lock
//! On the master controller node, a write lock is obtained on the repository,
//! then a baton message is sent to the master controller thread.
//!
//! The baton contains the name of the requested new master node.
//!
//! If the master controller is not in quorum, or the master controller is
//! not synchronized with the requested slave, the master controller drops
//! the baton message.
//!
//! Otherwise a temporary baton gets installed with both the resigning and the
//! replacement node being the master node. This keeps the node's status locked
//! to being a master, until the master is ready to become a slave to the
//! new master.
//!
//! The master controller then sends a
//! \c baton_master_announce
//! message, and a mcguffin, to all connection
//! threads to its slaves, except for the future master.
//!
//! Each connection thread stores a reference to the mcguffin
//! in its master metadata, then sends this
//! message to the peer node, and when it gets a reply to this
//! message
//! it drops the reference to the mcguffin.
//! Because the reference is held in the master data, it also gets dropped
//! if the peer connection thread loses the connection to the master controller.
//!
//! The master controller sets up a destructor callback on the
//! mcguffin. The destructor callback sends a masterbaton_announced() message
//! to the master controller thread, once all peers have replied and the
//! mcguffin gets destroyed.
//!
//! Upon receiving the \c masterbaton_announced message, the master controller
//! sends the baton in a \c baton_transfer_request to the connection thread
//! to the future master's connection thread.
//!
//! The connection thread to the future master now owns the baton, and sends
//! a message to the peer, informing the future master that it has a baton
//! for a handover. Upon receipt, the peer is expected to change its status
//! to a master, then acknowledge the baton. A destructor
//! callback on a mcguffin, that's set up waiting for that response, sends
//! masterbaton_handedover() message to the master controller.
//!
//! Upon receipt, the master controller distributes the permanent baton
//! to all connection threads as a "former master baton", then installs the
//! permanent baton into the cluster. This should result in the current master
//! node switching its status to be the new master's slave, and the connection
//! threads will hold that baton until its peer, that was originally this
//! node's slave, reports a different master, now that the node is no longer
//! a master. This should result in the baton forcing this node's status
//! to be the new master's slave, until all peers have switched over.
//!
//! When a connection thread receives a \c batonisyours message from its
//! master, instructing it to become a new master, the connection thread creates
//! a baton object and immediately installs it, changes its node's status to
//! a master, and distributes the baton to all connection threads as a
//! "former master baton", pings all peers to verify that they received
//! the node's status as a master, then acknowledges the baton from its
//! former master.
//!
//! In this manner, the existing master receives an acknowledgement from the
//! new master only after the new master becomes a master and its status
//! is reported to all of its peers. The existing master then becomes the
//! new master's slave.
//!
//! When all other slaves of the former master receive a baton announcement,
//! they install a temporary baton that locks their status as the existing
//! master's slave.
//!
//! When the existing master reports that it's no longer a
//! master, the slave's connection thread to the existing master
//! installs the permanent baton, making them
//! the new master's slave (since they should've already received the new
//! master's node status as a master).
//!
//! When a connection thread receives a status change from its peer, and the
//! connection thread has a baton indicating the peer is the baton's
//! resigning master --  on the new master's node, or on other slaves'
//! nodes -- the connection thread distributes the baton as a former
//! master baton to all connection threads on that node, making sure that the
//! baton keeps the node's status as a slave to the new master, until all peers
//! report that they're no longer the former master's slave.

class batonObj : virtual public x::obj {

public:
	//! Temporary baton with current master set as both resigning and replacement

	batonptr temp_baton;

	//! This is used in installbaton() to verify that the new baton is the replacement baton

	//! Normally, installbaton() fails if a baton is already installed,
	//! but since a temporary baton gets installed before the replacement
	//! one, this special case needs to be allowed. The constructor cleares
	//! this flag to \c false, and installbaton() uses it to verify that
	//! the existing baton is referenced by \c temp_baton of the new baton,
	//! got it?

	bool is_replacement_baton;

	//! The resigning master
	std::string resigningnode;

	//! The resigning master's UUID
	x::uuid resigninguuid;

	//! The replacement master
	std::string replacementnode;

	//! The unique identifier used to pass the baton.
	x::uuid batonuuid;

	//! Describe this baton

	operator std::string() const;
		
	//! Constructor
	batonObj(//! The resigning master
		 const std::string &resigningnodeArg,

		 //! The resigning master's UUID
		 const x::uuid &resigninguuidArg,

		 //! The replacement master
		 const std::string &replacementnodeArg,

		 //! The unique identifier used to pass the baton.
		 const x::uuid &batonuuidArg);

	~batonObj() noexcept;

private:

	//! Internal metadata

	class privatemetainfo {

	public:
		privatemetainfo() {}
		~privatemetainfo() noexcept {}

		//! The commit lock on the repository
		tobjrepoObj::commitlockptr_t commitlock;

		//! Baton created by the current master controller

		//! This is the master controller object. The destructor
		//! invokes handoff_failed() if this weak reference is valid.

		x::weakptr<repocontrollermasterptr> master_ptr;

		//! Connection object to the new master

		//! When this baton is created by the master controller, this
		//! is the connection object to the new master.

		x::weakptr<repopeerconnectionptr> master_newconn;

		//! Peers that received the baton from the master controller

		//! When this baton is created by the master controller, this
		//! is the list of connection object to peers who were given
		//! the baton.
		//!
		//! The destructor goes through the list, and invokes
		//! baton_master_recall for anything found in that list.
		//!
		//! When this baton is created in other contexts, the list
		//! is empty, of course.

		std::list<x::weakptr<repopeerconnectionptr> > master_peers;
	};

	//! A container for the private metadata

	typedef x::mpobj<privatemetainfo> privatemeta_t;

	//! Private metadata

	privatemeta_t privatemeta;

public:

	//! When creating the baton on the current master's side, set the master controller object

	void set_master_ptr(const repocontrollermaster &master_ptrArg);


	//! Put a commit lock into the baton.

	void set_commitlock(const tobjrepoObj::commitlock_t &commitlockArg);


	//! Retrieve the commit lock from the baton

	tobjrepoObj::commitlock_t get_commitlock();


	//! On the current master, set the connection object to the new master.

	void set_master_newconn(const repopeerconnectionptr &master_newconnArg);


	//! On the current master, add a connection object to a slave
	void push_back_master_peers(const repopeerconnectionptr &peer);


	//! Existing master finished handing off the baton to the new slave

	//! On the old master controller, after a batonisyours message gets
	//! sent to the new peer, and it reports that it became a master,
	//! this gets invoked. It clears the slave connection list and the
	//! master controller reference, so the destructor will not dispatch
	//! recall messages to the slaves any more, and the master controller
	//! does not restart commits. Past the point of no return.

	void master_handoff_complete();

	//! Get names of connections that were added by push_back_master_peers()

	void get_master_peers_names(std::set<std::string> &names);


	//! Notify master controller that the baton has been announced to all peers

	void announced();

	//! On the current master node send a transfer request to new master's connection object

	void send_transfer_request();

	// repopeerconnectionObj:

	//! This baton was received by the connection object to the new master

	//! Used by repopeerconnectionObj to flag the baton as
	//! received by the connection object to the new master.

	bool received_flag;

	//! This baton to be cancelled

	//! Used by repopeerconnectionObj to mark the baton as
	//! being cancelled, because the connection object to the
	//! former master was dropped, or because the new master
	//! changed its state to something other than a slave of the
	//! current master, or itself being the master.

	bool cancelled_flag;


	//! This baton is being cleared from the clusterinfo object
	bool cluster_cleared_flag;
};

#endif
