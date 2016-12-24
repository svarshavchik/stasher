/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repopeerconnection_H
#define repopeerconnection_H

#include "peerstatusannouncer.H"
#include "clusterlistenerfwd.H"
#include "repopeerconnectionfwd.H"
#include "repopeerconnectionbase.H"
#include "repocontrollerslave.H"
#include "slavesyncinfo.H"
#include "mastersyncinfo.H"

#include "objrepocopy.H"
#include "objrepocopysrc.H"
#include "objrepocopydst.H"
#include "objserializer.H"
#include "trandistihave.H"
#include "trandistcancel.H"
#include "transerializer.H"
#include "tranrecvcanc.H"
#include "adminstopint.H"
#include "stasher/reqstat.H"

#include <x/ondestroy.H>
#include <x/threads/run.H>

class spacemonitorObj;
class trandistributorObj;
class clusterinfoObj;
class nodeclusterstatus;

STASHER_NAMESPACE_START
template<typename msg_type> class writtenObj;
STASHER_NAMESPACE_END

//! Exchange this node's status with our peers

//! This subclass inherits from peerstatusannouncerObj
//!
//! The constructor takes the name of the peer, this is passed down
//! to peerstatusannouncerObj as the name of the writer thread.
//!
//! \par Starting this thread
//!
//! Thread instantiation procedure is the same as fdobjrwthreadObj's, with
//! the addition of several additional parameters to this class's
//! create_startarg() method.
//!
//! \par Connection conflict resolution
//!
//! The thread invokes installpeer() as the first order of business, in order
//! to register itself with the cluster. If the installation attempt gets
//! rejected because there's an existing connection with the same peer,
//! conflict resolution takes place, that results in either this connection
//! or the existing connection terminating.
//!
//! clusterconnecterObj passes a timestamp to its connected() method, on both
//! sides of the peer connection. It is expected that the timestamp value will
//! be set in this connection object, prior to the thread getting started.
//!
//! Conflict resolution compares the existing connection object's timestamp and
//! the new connection object's timestamp. The most recent timestamp wins.
//!
//! It is presumed that there is a good reason for a connection attempt.
//! Therefore, a newer timestamp indicates that one of the peers dropped a
//! connection for some reason and needs to reestablish it, but the other peer
//! hasn't realized that the network connection went away.
//!
//! If this connection object's timestamp is larger, the existing connection
//! object's stop() method gets invoked, and a destructor callback on the
//! existing object invokes a callback that invokes installpeer() again, for
//! another go-around. If this connection object's timestamp is smaller, it
//! invokes its own stop() method [CONNCONFLICT].
//!
//! If the timestamps happen to be the same, the tiebreaker are the connection
//! uuids [CONNCONFLICTTIE].
//!
//! \par Peer linkage
//!
//! If the master name and uuid in the \ref repopeerconnectionbaseObj::peerlink "peerlink"
//! object matches the peer's current status, the \c peerlink reference
//! is copied to the executing thread's stack, otherwise it is ignored
//! [PEERCONNECT].
//!
//! Upon receipt of a nodeclusterstatus message, if the executing thread has
//! \c peerlink object reference that specifies a different master, the
//! \c peerlink object is dropped [NOLONGERPEER].
//!
//! If the controller's mcguffin gets destroyed, all references to the
//! controller objects go out of scope, including any peerlink object that
//! the execution thread owns [CONTROLLERGONE].
//!
//! \par Slave mode
//!
//! The \c slavemeta object holds additional metadata when this node is
//! the peer's slave.
//!
//! Receipt of a \c connect_slave message indicates that this
//! node became a slave to the connected peer. The reference to the
//! repository controller object is retrieved from the handle and verified
//! that the master name/uuid matches peer's status. Otherwise, the peer's
//! status must've just changed. This will be noticed by the repository
//! cluster info object separately, so this situation becomes a no-op.
//!
//! Otherwise:
//!
//! - the destination repository copy object is retrieved from the handle.
//!
//! - a srcmasterObj handle gets created, implementing the source side of a
//! repository copy operation, which simply sends the objects to the writer
//! thread, to write them to the peer; the destination repository copy object
//! thread gets started, telling it to talk to the srcmasterObj.
//!
//! - a slavesyncrequest message gets sent to the peer [SLAVESYNCREQUEST].
//!
//! - upon receipt of a nodeclusterstatus message that indicates that the
//! peer's master has changed, all the slave metadata gets quietly destroyed.
//! It's expected that this will be noticed by the repository cluster info
//! object separately, so this situation becomes a no-op.
//!
//! - upon receipt of any message that's sent by the source side of a
//! repository copy operation, if there's an initialized slave metadata
//! with a destination repository copy object, the message gets forwarded to
//! the destination repository copy object. A receipt of a \c copycomplete
//! message indicates that all the slave metadata is no longer needed, and
//! it gets erased.
//!
//! \par Notifying master which transactions this slave node has received
//!
//! If this is a connection object from a slave node to its master, the
//! \c tranrecvcanc message is used to update the master with the list of
//! transactions this node has received.
//!
//! When the slave receives a copycomplete message from its master,
//! the slave controller's installreceiver() method gets used to install
//! a transaction receiver handler. The slave controller slave invokes the
//! distributor object's installreceiver() method to install the receiver
//! handler. Immediately, the list of transactions already received gets
//! handed over to the receiver handler [SLAVERECVINIT]. Going forward,
//! as this node receives new transactions from the originating peer, and
//! the originating peer cancels transactions, the receiver handler gets
//! invoked to pass along the updates to the master [SLAVERECVUPDATE].
//!
//! The receiver handler's callbacks send \c tranrecvcanc messages to the peer.
//!
//! \par Master mode
//!
//! The \c mastermeta object holds additional metadata when this node
//! is the peer's master.
//!
//! Receipt of a \c connect_master message indicates that this node is a master
//! node to the connected peer. The reference to the master
//! repository controller object is retrieved from the handle and verified
//! that the master name/uuid matches peer's status. Otherwise, the peer's
//! status must've just changed. This will be noticed by the master
//! controller object separately, so this situation becomes a no-op.
//!
//! Otherwise:
//!
//! - the master repository controller's object gets saved.
//!
//! - the master repository controller passes a reference to a tranuuid
//! object which tracks the set of transactions the slave peer has received.
//! This object is saved. Upon receipt of a \c tranrecv message, the
//! list of uuids in the message is passed along to the tranuuid object's
//! methods, for updating. This object does not update the set of transactions,
//! but passes the list of changes to its received() and cancelled() methods.
//!
//! - the master repository controller passes a reference to its internal
//! connection object for this peer connection object. This peer connection
//! object indicates that it accepts the connection from the master by invoking
//! the master object's accept() method, passing it its own reference, and
//! the \c peerlink reference that this connection object is currently holding.
//!
//! - check_sync_start() gets invoked. check_sync_start() also gets invoked
//! upon receipt of a \c slavesyncrequest message from the peer, which indicates
//! that the peer is ready to be synced from this master. When both events
//! occur -- the connection from the master controller object, and the receipt
//! of a connect_slave message -- the master/slave repository copy process
//! gets instantiated.
//!
//! \par Commit requests
//!
//! On the master node, the \c commit_peer message contains a commit request
//! message, and a mcguffin. This sends a \c commitreq message to the slave
//! peer. The slave peer processes the commit message, and replies with
//! a \c commitack message. When the master node node keeps track of
//! \c commitack messages its waiting for, and the corresponding mcguffins.
//! Receipt of a \c commitack message drops the reference on the mcguffin
//! [COMMITPEER].
//!
//! If when the \c commit_peer message gets sent the peer is not this node's
//! slave,
//! the message gets discarded, immediately releasing the reference on the
//! mcguffin, without sending a message [COMMITPEERRACE].
//!
//! \par Ping-pong
//!
//! ping() receives a mcguffin as an argument, and sends a "ping" message
//! to the connected peers. When "ping" is received from the peer, it's
//! acknowledged with a "pong" message. When "pong" is received from the peer,
//! the reference to the mcguffin gets discarded [PINGPONG].
//!
//! This is used as a means of verifying that the current state of this node
//! has been received by the peer.
//!
//! There's an internal uuid assigned to each ping request. Each ping's
//! mcguffin is associated with the uuid. The returned pong message carries
//! the same uuid, and the mcguffin stored under that uuid is dropped.
//!
//! This mechanism is also overloaded in other situations where the execution
//! thread needs to keep something around until the peer acks it. The
//! requesting object is held under an assigned uuid, and the peer responds
//! with a pong to drop the requesting object, which for all purposes now is
//! just a mcguffin.
//!
//! \par Baton handling
//!
//! The process of an orderly migration from one master to another is called
//! "passing the baton".
//!
//! When a connection thread on the master node, with some slave, receives
//! a \ref baton_master_announce() "baton_master_announce"
//! message, which includes a mcguffin. [MASTERANNOUNCEBATON]
//! The connection thread sends the
//! message to the peer, and keeps the mcguffin, until it receives a
//! \c baton_slave_received message from the peer, at which time the mcguffin
//! gets released.
//!
//! When a connection thread on a slave node receives a \c baton_master_announce
//! message, it creates a baton object on the slave side, a commit lock,
//! and starts a temporary thread that waits until it
//! acquires a commit lock, and passes along the baton back to the
//! connection thread to the old master. [BATONSLAVEACQUIRECOMMIT]
//!
//! The connection thread with the old master keeps the baton in the slave
//! connection metadata, and sends the \c baton_slave_received message back
//! to the current master. A disconnection from the slave controller, for
//! any reason, like the old master disappearing, destroys the baton,
//! releasing commit lock. [BATONCANCELOLDMASTERGONE]
//!
//! When all connection threads on the current master have received their
//! \c baton_slave_received message, releasing each one's reference to the
//! mcguffin on the master side; when that mcguffin gets destroyed, a
//! \c masterbaton_announced message gets sent to the master controller.
//! The master controller thread, upon receiving the message (thus proving
//! that the master controller thread that started the whole thing is still
//! running, send a \c baton_transfer_request which contains the baton,
//! to the connection thread to the new master.
//!
//! The connection thread, upon receiving this message, puts the baton
//! into the master metadata, thus discarding the baton if its disconnected
//! from the master controller; then sends a \c batonisyours message to the
//! peer, placing a \c given_cb mcguffin into the list of mcguffins waiting
//! to be ponged.
//!
//! The new master, upon receiving \c batonisyours creates a baton and installs
//! it, making it a master. This should result in all peers being notified of
//! the node's new status as a master. The new master then pings all of its
//! connections, with the same mcguffin, \c baton_newmaster_installed_cb.
//! When a pong gets returned, this proves that all other peers have been
//! notified that this node is now a master [BATONNEWNOTIFIED], and the
//! \c baton_newmaster_installed_cb sends a \c batonismine message to the
//! connection thread to the old master, upon receipt which it sends a
//! pong message to the old master, which triggers the \c given_cb mcguffin on
//! the master's side [BATONNEWMASTERREADY].
//!
//! The installformermasterbaton() message installs a baton into the
//! executing thread's stack.
//! This baton is held on the executing thread's stack as long as the
//! peer reports that its master is the resigning master given in the
//! baton. As soon as the peer reports that its master is somebody
//! else, the baton gets dropped.
//!
//! This is used to keep the baton around until everyone disconnects
//! from the former master, to make sure that the baton forces this
//! node to be the new master's slave [INSTALLFORMERMASTERBATON].
//!
//! If at the time the installformermasterbaton() message gets received the
//! peer already has a different master, the baton is dropped immediately
//! [INSTALLFORMERMASTERBATONIMM].
//!
//! On the new master, when the connection thread with the old master receives
//! a \c batonismime message, it sends a pong message to the old master, with
//! the baton's uuid, ponging the old master's \c given_cb mcguffin. Then,
//! it sends the installformermasterbaton() message to all connection threads
//! on the new master, so that the baton holds the new master's state as a
//! master until all peers stop being the old master's slaves
//! [BATONNEWMASTERCUTOVER].
//!
//! On the old master, the \c given_cb mcguffin sends a
//! \c masterbaton_handedover message to the master controller. Upon receipt,
//! the master controller sends an installformermasterbaton() message to
//! all connection threads on the new master, and installs the baton, making
//! the old master the new master's slave [BATONOLDMASTERCUTOVER].
//!
//! When a connection object receives a message from the peer that it's status
//! has changed to a new master, it checks if it's a slave to the peer, and
//! there's a baton in the slave metadata, if so, the baton gets removed from
//! the slave metadata and installed,
//! making this node a slave to the new master, if its connected with the nwe
//! master, and the installformermasterbaton() message gets also sent to all
//! connection threads [BATONSLAVECUTOVER].
//!
//! On the old master, there's a destructor callback attached to the
//! temporary baton. The temporary baton getting destroyed
//! results in the release message going back out to the peers
//! [BATONRELEASESEND].
//!
//! Upon receipt [BATONRELEASERECV],
//! each slave's connection thread to the master destroys the
//! baton [BATONRELEASEFWD] if its still in the slave connection metadata.
//!
//! \par Quorum announcements
//!
//! The master node announces changes to its published quorum status
//! (whether all nods in the cluster are connected with the master, and are
//! fully synced). The master controller invokes sends the
//! master_quorum_announce() message to all connection threads to its peers.
//! The peer sends the \c master_quorum message to the slave node. The slave
//! node passes it along and sends the master_quorum_announce() message to
//! the slave controller it's connected to, which updates the quorum status
//! on that node. [QUORUMDIST]
//!
//! \par Cluster halts.
//!
//! The cluster parameter to create_startarg() is normally convertible to
//! \c adminstopint. It's not presumed, it is not in most regression tests,
//! but if it is, upon receipt of a halt message from a peer, it's stop()
//! method gets called, with the socket connection object [HALTCLUSTERRECV].

class repopeerconnectionObj: public peerstatusannouncerObj,
			     public repopeerconnectionbaseObj,
			     public STASHER_NAMESPACE::threadreportimpl2Obj {

	//! Debugging property
	static x::property::value<bool> debugnotimeout;
	//! The logger object

	LOG_CLASS_SCOPE;

	//! Master orders the slave to commit or cancel a transaction

	class commitreq {

	public:

		//! Transaction to commit or cancel
		x::uuid uuid;

		//! Transaction's originating node
		std::string source;

		//! Transaction's status
		STASHER_NAMESPACE::req_stat_t status;

		//! Constructor
		commitreq(const x::uuid &uuidArg,
			  const std::string &sourceArg,
			  STASHER_NAMESPACE::req_stat_t statusArg);


		//! Default constructor
		commitreq()=default;

		//! Destructor
		~commitreq() noexcept=default;

		//! Serialization function
		template<typename iter_type> void serialize(iter_type &iter)

		{
			iter(uuid);
			iter(source);

			unsigned char n;

			if (iter_type::serializing)
				n=(unsigned char)status;

			iter(n);

			if (!iter_type::serializing)
				status=(STASHER_NAMESPACE::req_stat_t)n;
		}
	};


//! Message from current master to new master

	class batonisyours {

	public:
		//! The resigning master
		std::string resigningnode;

		//! The resigning master's uuid
		x::uuid resigninguuid;

		//! The baton's uuid
		x::uuid batonuuid;

		//! Constructor
		batonisyours(const std::string &resigningnodeArg,
			     const x::uuid &resigninguuidArg,
			     const x::uuid &batonuuidArg)
			: resigningnode(resigningnodeArg),
			  resigninguuid(resigninguuidArg),
			  batonuuid(batonuuidArg)
		{
		}

		//! Default constructor

		batonisyours()
		{
		}

		//! Destructor
		~batonisyours() noexcept
		{
		}

		//! Serialization function

		template<typename iter_type> void serialize(iter_type &iter)

		{
			iter(resigningnode);
			iter(resigninguuid);
			iter(batonuuid);
		}
	};

	//! A slave is asking the master to hand over to another node

	class handover_request {

	public:
		//! Which node should become the new master

		std::string newmastername;

		//! This request's uuid

		x::uuid uuid;

		//! Default constructor
		handover_request() {}

		//! Constructor
		handover_request(const std::string &newmasternameArg,
				 const x::uuid &uuidArg)
			: newmastername(newmasternameArg),
			  uuid(uuidArg) {}

		//! Destructor
		~handover_request() noexcept=default;

		//! Serialization function

		template<typename iter_type> void serialize(iter_type &iter)

		{
			iter(newmastername);
			iter(uuid);
		}
	};

public:
	//! The argument to setnewcert_request()

	class setnewcertObj : virtual public x::obj {

	public:
		//! The certificate to send to the peer
		std::string certificate;

		//! The result
		bool success;

		//! Once a response has been received, it gets stored here.
		std::string result;

		setnewcertObj(const std::string &certificateArg)
			: certificate(certificateArg),
			  success(false),
			  result("Request failed")
		{
		}

		~setnewcertObj() noexcept
		{
		}

		class pendingObj;
	};

	//! A reference-counted commitreq, basically

	class commitreqObj : public STASHER_NAMESPACE::writtenobjbaseObj {

	public:

		//! The actual message
		commitreq msg;

		//! Constructor
		commitreqObj(const x::uuid &uuidArg,
			     const std::string &sourceArg,
			     STASHER_NAMESPACE::req_stat_t statusArg);


		//! Destructor
		~commitreqObj() noexcept;

		//! Serialization function
		void serialize(STASHER_NAMESPACE::objwriterObj &writer);
	};

private:

	//! The slave acknowledges master's request

	class commitack {

	public:

		//! Transaction that has been commited or cancelled.
		x::uuid uuid;

		//! Constructor
		commitack(const x::uuid &uuidArg);


		//! Default constructor
		commitack()=default;

		//! Destructor
		~commitack() noexcept=default;

		//! Serialization function
		template<typename iter_type> void serialize(iter_type &iter)

		{
			iter(uuid);
		}
	};

	//! Ping message

	class pingm {

	public:
		//! uuid-generated string

		std::string uuid;

		//! Default constructor
		pingm()=default;

		//! Constructor
		pingm(const std::string &uuidArg)
			: uuid(uuidArg)
		{
		}

		//! Destructor
		~pingm() noexcept=default;

		//! Serialization function
		template<typename iter_type> void serialize(iter_type &iter)

		{
			iter(uuid);
		}
	};

	//! Pong message

	class pongm {

	public:
		//! uuid-generated string

		std::string uuid;

		//! Default constructor
		pongm()=default;

		//! Destructor
		~pongm() noexcept=default;

		//! Serialization function
		template<typename iter_type> void serialize(iter_type &iter)

		{
			iter(uuid);
		}
	};

	//! Outstanding pings to the peer, on the executing thread's stack

	std::map<std::string, x::ptr<x::obj> > *pings_outstanding;

	//! Master controller's quorum status

	class master_quorum {

	public:
		//! Master's name
		std::string mastername;

		//! Master's uuid
		x::uuid masteruuid;

		//! The quorum value
		STASHER_NAMESPACE::quorumstate state;

		//! Constructor

		master_quorum(const std::string &masternameArg,
			      const x::uuid &masteruuidArg,
			      const STASHER_NAMESPACE::quorumstate &stateArg)
			: mastername(masternameArg),
			  masteruuid(masteruuidArg),
			  state(stateArg)
		{
		}

		//! Constructor

		master_quorum()=default;

		//! Destructor
		~master_quorum() noexcept=default;

		//! Serialization function

		template<typename iter_type>
		void serialize(iter_type &iter)
		{
			iter(mastername);
			iter(masteruuid);
			iter(state);
		}
	};

	//! Message to a peer: new certificate for this node.

	class setnewcert {
	public:
		//! uuid-generated string
		std::string uuid;

		//! The actual certificate
		std::string certificate;

		//! Default constructor

		setnewcert()=default;

		//! Constructor

		setnewcert(const std::string &certificateArg,
			   const std::string &uuidArg)
			: uuid(uuidArg), certificate(certificateArg)
		{
		}

		~setnewcert() noexcept=default;

		//! Serialization function

		template<typename iter_type> void serialize(iter_type &iter)
		{
			iter(uuid);
			iter(certificate);
		}
	};

	//! Peer's response to setnewcert

	class setnewcert_resp {
	public:
		//! uuid-generated string
		std::string uuid;

		//! Whether the request succeeded
		bool success;

		//! The result
		std::string result;

		//! Default constructor

		setnewcert_resp()=default;

		//! Constructor

		setnewcert_resp(bool successArg,
				const std::string &resultArg,
				const std::string &uuidArg)
			: uuid(uuidArg), success(successArg), result(resultArg)
		{
		}

		~setnewcert_resp() noexcept=default;

		//! Serialization function

		template<typename iter_type> void serialize(iter_type &iter)
		{
			iter(uuid);
			iter(success);
			iter(result);
		}
	};

	//! A message from a master to its slave, halting the cluster.

	class halt {
	public:
		//! Master's name
		std::string mastername;

		//! Master's uuid
		x::uuid masteruuid;

		//! Constructor

		halt(const std::string &masternameArg,
		     const x::uuid &masteruuidArg)
			: mastername(masternameArg),
			  masteruuid(masteruuidArg)
		{
		}

		//! Constructor
		halt()=default;

		//! Destructor
		~halt() noexcept
		{
		}

		//! Serialization function

		template<typename iter_type>
		void serialize(iter_type &iter)
		{
			iter(mastername);
			iter(masteruuid);
		}
	};

	class baton_master_announce_msg {
	public:
		std::string mastername;
		x::uuid masteruuid;
		x::uuid batonuuid;
		std::string newmasterpeer;

		baton_master_announce_msg(const std::string &mastername,
					  const x::uuid &masteruuid,
					  const x::uuid &batonuuid,
					  const std::string &newmasterpeer)
			: mastername(mastername),
			  masteruuid(masteruuid),
			  batonuuid(batonuuid),
			  newmasterpeer(newmasterpeer)
		{
		}

		baton_master_announce_msg() = default;

		~baton_master_announce_msg() noexcept=default;

		template<typename iter_type> void serialize(iter_type &iter)
		{
			iter(mastername);
			iter(masteruuid);
			iter(batonuuid);
			iter(newmasterpeer);
		}
	};

	class baton_master_release_msg {
	public:
		std::string mastername;
		x::uuid masteruuid;

		baton_master_release_msg(const std::string &mastername,
					 const x::uuid &masteruuid)
			: mastername(mastername),
			  masteruuid(masteruuid)
		{
		}

		baton_master_release_msg() = default;
		~baton_master_release_msg() noexcept = default;

		template<typename iter_type> void serialize(iter_type &iter)
		{
			iter(mastername);
			iter(masteruuid);
		}
	};


public:

	//! Deserialization classlist enumerator

	template<typename iter_type>
	static void classlist(iter_type &iter)
	{
		peerstatusannouncerObj::classlist(iter);

		// Master/slave synchronization

		iter.template serialize<objrepocopy::batonrequest,
					deser<objrepocopy::batonrequest> >();
		iter.template serialize<objrepocopy::batonresponse,
					deser<objrepocopy::batonresponse> >();
		iter.template serialize<objrepocopy::masterlist,
					deser<objrepocopy::masterlist> >();
		iter.template serialize<objrepocopy::masterlistdone,
					deser<objrepocopy::masterlistdone> >();
		iter.template serialize<objrepocopy::slaveliststart,
					deser<objrepocopy::slaveliststart> >();
		iter.template serialize<objrepocopy::masterack,
					deser<objrepocopy::masterack> >();
		iter.template serialize<objrepocopy::copycomplete,
					deser<objrepocopy::copycomplete> >();

		// Use objdeserializer to deserialize an objserializer

		iter.template serialize<objserializer, objdeserializer>();

		iter.template serialize<objrepocopy::slavelist,
					deser<objrepocopy::slavelist> >();
		iter.template serialize<objrepocopy::slavelistready,
					deser<objrepocopy::slavelistready> >();
		iter.template serialize<objrepocopy::slavelistdone,
					deser<objrepocopy::slavelistdone> >();
		iter.template serialize<slavesyncrequest,
					deser<slavesyncrequest> >();

		// Distributor messages

		iter.template serialize<trandistihave, deser<trandistihave> >();
		iter.template serialize<trandistcancel,
					deser<trandistcancel> >();
		iter.template serialize<transerializer, trandeserializer>();

		// Master's messages to its slave

		iter.template serialize<commitreq, deser<commitreq> >();

		iter.template serialize<commitack, deser<commitack> >();

		iter.template serialize<master_quorum, deser<master_quorum> >();

		// Transaction announcements

		iter.template serialize<tranrecvcanc,
					deser<tranrecvcanc> >();

		// Master handoff

		iter.template serialize<handover_request,
					deser<handover_request> >();

		iter.template serialize<baton_master_announce_msg,
					deser<baton_master_announce_msg> >();

		iter.template serialize<baton_master_release_msg,
					deser<baton_master_release_msg> >();

		iter.template serialize<baton_slave_received_msg,
					deser<baton_slave_received_msg> >();

		iter.template serialize<batonisyours,
					deser<batonisyours> >();

		//! Other stuff

		iter.template serialize<pingm, deser<pingm> >();
		iter.template serialize<pongm, deser<pongm> >();
		iter.template serialize<setnewcert, deser<setnewcert> >();
		iter.template serialize<setnewcert_resp,
					deser<setnewcert_resp> >();
		iter.template serialize<halt, deser<halt> >();
	}
private:
	//! Disk space usage object

	spacemonitor df;

	class srcmasterObj;
	class dstmasterObj;
	class syncslave_cbObj;

	//! Syncing object from the master repository

	//! Must construct an objserializer for deserializing into this
	//! node's slave repository.

	class objdeserializer : public objserializer {

	public:

		//! Constructor

		//! When this is connection object is not in slave mode,
		//! the repository reference will already be a null ptr.
		objdeserializer(repopeerconnectionObj &conn);


		~objdeserializer() noexcept;
	};

	//! Deserializing a transaction from a distributor peer

	//! Must construct a transerializer for this node's repository
	//!
	class trandeserializer : public transerializer {

	public:
		//! Constructor

		//! Construct a transerializer for this node's repository.
		//!
		trandeserializer(repopeerconnectionObj &conn);


		~trandeserializer() noexcept;
	};
protected:
	//! Message: slave wants to be synced by its master

	class slavesyncrequest {

	public:
		//! Constructor

		slavesyncrequest()=default;

		//! Destructor
		~slavesyncrequest() noexcept=default;

		//! Serialization function

		template<typename iter_type>
		void serialize(iter_type &iter)
		{
		}
	};

	//! Flag: a slavesyncrequest message has been received

	bool slavesyncrequest_received;

	//! This peer's controller link, on the executing thread's stack

	//! When a peerlink message is received from a controller, the
	//! controller's master name/uuid must match the remote peer's status,
	//! otherwise the peerlink message gets discarded.
	//!
	//! Upon receipt of a nodeclusterstatus message from the remote
	//! peer, if the remote peer's master/uuid is different than the
	//! installed peerlink message's, the peerlink message gets discarded.
	//!
	//! Assertion: the linked controller's master name/uuid always matches
	//! the remote peer's status.

	peerlinkptr *currentpeerlink;

	//! Currently connected controller

	//! This is a mcguffin-related reference to the connected, linked,
	//! controller object.
	//!
	//! It gets installed together with currentpeerlink, that depends
	//! on the mcguffin kept on the controller's execution thread stack.
	//!
	//! When the controller's execution thread terminates and its mcguffin
	//! gets destroyed, the callback on this mcguffin reference sends the
	//! \c disconnect_peer message. Upon receipt, this execution thread
	//! checks if there's an installed currentconnect, and its held
	//! reference is null, if so the currentconnect and currentpeerlink
	//! objects are dropped.
	//!
	//! If currentconnect still holds a valid mcguffin reference, it must
	//! be a reference to a replacement controller that has already been
	//! installed, so the disconnect message gets discarded, everything's
	//! cool.

	peerdisconnect_msg *currentconnect;

	//! Baton from the former master, on the executing thread's stack

	batonptr *formermasterbaton;

	//! Stoppable interface invoked in response to a haltcmd(), on the executing thread's stack

	x::weakptr<adminstopintptr> *haltstop;

	//! Check if the formermasterbaton should be released

	void checkformermasterbaton(const nodeclusterstatus &peerstatus);


	//! The former master's baton's replacement node is this connection object's peer.

	//! When the former master baton's is received by a connection object
	//! to the replacement master, it gets saved in this placeholder
	//! (on the executing thread's stack). It's expected that the
	//! old master's slave controller is about to die, and get succeeded
	//! by the slave controller for the new master, which will subsequently
	//! link to this connection object.
	//!
	//! The baton received by installformermasterbaton() gets placed here
	//! if its replacement node is this peer, the peer's status is
	//! master status, and the resigning node is this node's current master.
	//!
	//! This placeholder gets cleared upon:
	//!
	//! - a connection request from a master or a slave controller. If
	//! it's a slave controller, the baton is handed over to the slave
	//! controller, for synchronizing.
	//!
	//! - the peer announces that its status is not a master.
	//!
	//! - this node announces that its status is not this peer's slave.

	batonptr *pendingslavebaton;

	//! Baton pending for a master connection

	//! When checkformermasterbaton determines that the formermasterbaton
	//! needs to be removed, because the peer is no longer the former
	//! master's slave, the baton is placed into pendingmasterbaton, and
	//! checkpendingmasterbaton() gets immediately invoked, to verify that
	//! it qualifies to be held there.

	batonptr *pendingmasterbaton;

	//! Hold a baton for a pending master connection

	//! A baton is held in pendingmasterbaton as long as all the following
	//! conditions are true:
	//!
	//! - the baton's replacement node is this node
	//!
	//! - peer's status is that it's a slave to this node
	//!
	//! - this node's status is that it's a master
	void checkpendingmasterbaton(const nodeclusterstatus &peerstatus);


	//! Check if the pending baton needs to be cleared.

	void thisstatusupdated() override;

	//! Slave metadata

	//! Metadata used when this connection is controlled by a slave
	//! controller

	class slavemeta;

	//! The slavemeta object on the executing thread's stack

	slavemeta *slavemetaptr;

	//! Thread starter, on the executing thread's stack
	STASHER_NAMESPACE::stoppableThreadTrackerObj *tracker;

	//! Executing thread's mcguffin, on the executing thread's stack
	x::ptr<x::obj> *thread_mcguffin;

	//! Master metadata

	//! Metadata used when this connection is controlled by a master
	//! controller

	class mastermeta;

	//! The mastermeta object on the executing thread's stack

	mastermeta *mastermetaptr;

	//! Disconnect the peer from the controller

	void disconnect();

	//! Whether the connection is encrypted

	//! Valid only in the context of the execution thread.

	bool encrypted;

	//! The transaction distributor thread

	//! Valid only in the context of the execution thread.

	trandistributorObj *distributor;

	//! Repository being distributed

	//! Valid only in the context of the execution thread.
	tobjrepoObj *distrepo;

	//! Listener object

	//! Valid only in the context of the execution thread.

	clusterlistenerObj *listener;

public:
	//! The connection thread.

	//! The first four parameters are for fdobjrwthreadObj, and are
	//! described there. The fifth parameter is a flag that specifies
	//! whether the connection is encrypted. This is used for reporting
	//! purposes.
	//! The sixth parameter is the distributor object.
	//! The seventh parameter is the listener object, if this connection
	//! receives a setnewcert message with a new certificate to install,
	//! it gets forwarded to the listener.
	//! The eigth parameter and the ninth parameters are the peer's
	//! initial status, as given during the handshake, and the cluster that
	//! this connection is part of.
	//!
	//! When the execution thread starts, it passes the initial status
	//! to peerstatusupdate(), then invokes install(). If the cluster
	//! parameter is a null reference, this step is omitted; some
	//! regression tests pass a null cluster reference.
	//!
	//! The last parameter is the repository that's responsible for
	//! everything.

	void run(//! threadmsgdispatcher mcguffin,
		 x::ptr<x::obj> &threadmsgdispatcher_mcguffin,

		 //! The file descriptor
		 const x::fdbase &transport,

		 //! The input iterator for the file descriptor
		 const x::fd::base::inputiter &inputiter,

		 //! Thread tracker that the writer thread gets registered with
		 STASHER_NAMESPACE::stoppableThreadTracker &tracker,

		 //! The fdobjrwthreadObj mcguffin
		 const x::ptr<x::obj> &mcguffin,

		 //! Whether this is an encrypted connection
		 bool encrypted,

		 //! The distributor object
		 x::ptr<trandistributorObj> &distributor,

		 //! The listener object

		 //! Some regression tests don't supply a listener object
		 clusterlistenerptr listenerArg,

		 //! Initial node status
		 const nodeclusterstatus &peerstatusArg,

		 //! The cluster that this connection is
		 //! part of
		 clusterinfoptr clusterArg,

		 //! The repository being distributed
		 tobjrepo &distrepo);

	friend class objdeserializer;
	friend class trandeserializer;

	//! Slave reports back to the master that it received the baton

	class baton_slave_received_msg {

	public:
		baton_slave_received_msg() {}
		~baton_slave_received_msg() {}
		//! Serialization function
		template<typename iter_type> void serialize(iter_type &iter)

		{
		}
	};

	using peerstatusannouncerObj::deserialized;

	//! Constructor
	repopeerconnectionObj(const std::string &connectionName,
			      const spacemonitor &dfArg);


	//! Destructor

	~repopeerconnectionObj() noexcept;

	// --- Externally invoked methods ---

	//! Distributor: ihave message

	void distribute_peer(const trandistihave &msg)
	{
		sendevent(&repopeerconnectionObj::dispatch_trandistihave,
			  this, msg);
	}

	//! Distributor: trandistcancel message

	void distribute_peer(const trandistcancel &msg)
	{
		sendevent(&repopeerconnectionObj::dispatch_trandistcancel,
			  this, msg);
	}

	//! Distributor: prepared transerializer message

	void distribute_peer(const x::ref<STASHER_NAMESPACE::writtenObj<transerializer> > &msg)
	{
		sendevent(&repopeerconnectionObj::dispatch_transerializer, this, msg);
	}

protected:
	// --- Dispatch messages in the context of the execution thread

	//! Process distributor message in the context of this thread

	void dispatch_trandistcancel(const trandistcancel &msg);

	//! Process distributor message in the context of this thread

	void dispatch_trandistihave(const trandistihave &msg);

	//! Process distributor message in the context of this thread

	void dispatch_transerializer(const x::ref<STASHER_NAMESPACE::writtenObj<transerializer> > &msg);


public:
	//! Simulate receiption of a halt message, for debugging purposes

	void debugHalt(const std::string &mastername,
		       const x::uuid &masteruuid)
	{
		deserialized(halt(mastername, masteruuid));
	}

	//! Intercept nodeclusterstatus

	void deserialized(const nodeclusterstatus &newStatus);


	//! Syncing from a baster
	void deserialized(const objrepocopy::batonrequest &msg);


	//! Syncing from master

	void deserialized(const objrepocopy::masterlist &msg);


	//! Syncing from master

	void deserialized(const objrepocopy::masterlistdone &msg);


	//! Syncing from master

	void deserialized(const objrepocopy::slaveliststart &msg);


	//! Syncing from master

	void deserialized(const objrepocopy::masterack &msg);


	//! Syncing from master

	void deserialized(const objrepocopy::copycomplete &msg);


	//! Syncing from master

	void deserialized(const objdeserializer &msg);


	//! Received a slavesyncrequest from the peer, we must be the master

	void deserialized(const slavesyncrequest &msg);


	//! Message from a slave being synced
	void deserialized(const objrepocopy::batonresponse &msg);


	//! Message from a slave being synced
	void deserialized(const objrepocopy::slavelist &msg);


	//! Message from a slave being synced
	void deserialized(const objrepocopy::slavelistready &msg);


	//! Message from a slave being synced
	void deserialized(const objrepocopy::slavelistdone &msg);


	//! Message from a peer distributor
	void deserialized(const trandistihave &msg);


	//! Message from a peer distributor
	void deserialized(const trandistcancel &msg);


	//! Message from a peer distributor
	void deserialized(const transerializer &msg);


	//! Message from master ordering the slave to commit
	void deserialized(const commitreq &msg);


	//! Message from slave ack-ing the commit
	void deserialized(const commitack &msg);


	//! Message from a slave to master announcing transactions that it received

	void deserialized(const tranrecvcanc &msg);


	//! Slave synchronization/keepalive

	void deserialized(const pingm &msg);


	//! Slave synchronization/keepalive

	void deserialized(const pongm &msg);

	//! New certificate for this node
	void deserialized(const setnewcert &msg);

	//! Response to a set new certificate request.
	void deserialized(const setnewcert_resp &msg);

	//! Master wants this slave to halt
	void deserialized(const halt &msg);

	//! Slave is asking its master to give it up to another node
	void deserialized(const handover_request &msg);

	//! Message from master - announcement for passing the baton

	void deserialized(const baton_master_announce_msg &msg);

	//! Message from master - releaseing the baton announcement

	void deserialized(const baton_master_release_msg &msg);

	//! Message from slave - it received master's baton

	void deserialized(const baton_slave_received_msg &msg);

	//! Baton being passed from master

	void deserialized(const batonisyours &msg);

	//! Master announces its quorum

	void deserialized(const master_quorum &msg);

	class baton_slave_commitlock_thread;

	class baton_newmaster_installed_cb;
private:
	//! Disconnecting from the current master, and it gave us a baton.

	//! Check if the new master is up.

       void checknewmaster();

	//! Check whether this master should begin syncing the peer slave

	void check_sync_start();

	//! Attempt to install myself into the cluster

	void installattempt(const clusterinfo &cluster);


	class installattempt_cb;

	class baton_given_cb;

	class handover_request_cb;

	//! Report this thread's status
	std::string report(std::ostream &rep);

public:

	//! Connection request from the controller

	void connect_peer(const peerlinkptr &masterlinkArg) override
	{
		do_connect_peer(masterlinkArg);
	}

	//! The controller has disconnected

	void disconnect_peer() override
	{
		do_disconnect_peer();
	}
	//! Invoked by slave controller, to establish linkage with a connection object to its master

	void connect_slave(const x::ptr<slavesyncinfoObj> &synchandle) override
	{
		do_connect_slave(synchandle);
	}

	//! Invoked by master controller, to establish linkage with a connection object to one of its slave

	void connect_master(const x::ref<mastersyncinfoObj> &synchandle) override
	{
		do_connect_master(synchandle);
	}
protected:
	void foo() override {}

	//! The actual main loop

	MAINLOOP_DECL;

#include "repopeerconnection.msgs.H"

};

#endif
