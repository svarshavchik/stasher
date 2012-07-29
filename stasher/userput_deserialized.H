/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef userput_deserialized_H
#define userput_deserialized_H

#include "stasher/userput.H"
#include "spacemonitor.H"
#include "objrepo.H"
#include "stasher/puttransaction.H"
#include "stasher/reqstat.H"
#include "nsview.H"
#include "newtranfwd.H"
#include "tobjrepofwd.H"

STASHER_NAMESPACE_START

//! After deserializing, this object gets constructed

class userput::deserializedObj : virtual public x::obj {

	//! The logger
	LOG_CLASS_SCOPE;
public:
	//! The deserialized metadata
	x::ptr<STASHER_NAMESPACE::puttransactionObj> puttran;

	//! Repository
	tobjrepo repo;

	//! The space reservation object
	spacemonitorObj::reservationptr diskspace;

	//! The transaction object being built.
	newtran tran;

	//! Request uuid
	x::uuid requuid;

	//! Namespace mapper
	nsview namespaceview;

	//! Constructor

	deserializedObj(//! Deserialized metadata
			const x::ptr<STASHER_NAMESPACE::puttransactionObj> &puttran,

			//! Connection object
			const userinit &limits,

			//! Disk space monitor
			const spacemonitor &diskspace,

			//! Repository object
			const tobjrepo &repoArg,

			//! Request uuid
			const x::uuid &requuidArg,
			
			//! Client's namespace
			const nsview &namespaceviewArg);


	//! Destructor

	~deserializedObj() noexcept;

	//! Received a content str

	void received(const STASHER_NAMESPACE::puttransactionObj::content_str &str);


	//! Received contents in a file descriptor

	void received(const x::fd &socket);


	//! Return an indication whether the transaction is complete
	bool completed();

	//! Error code (req_processed_stat=deserialized ok)
	req_stat_t errcode;

private:
	//! Current object's contents being deserialized
	size_t cur_object;

	//! Current object's contents
	x::fdptr cur_object_fd;

	//! How many bytes of the current object's contents have been received
	off_t cur_object_cnt;

	//! Prepare next object for deserialization
	void next_object();
};

STASHER_NAMESPACE_END

#endif
