/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopy_H
#define objrepocopy_H

#include "objuuidlist.H"

//! Messages used when copying the contents of one repository into another.

//! The sequence of events is as follows:
//!
//! - the master sends masterlist messages to the slave, enumerating
//!   uuids in master's repository, followed by masterlistdone. [SENDMASTERLIST]
//!
//! - the slave responds to each masterlist message with an
//!   slavelist message, which contains an objuuidlist. If a master gave
//!   an object name that does not exist in slave's object repository, the
//!   slave puts that object name into objnouuids [NOOBJECT]. If the object
//!   exists but has a different uuid, the slave puts that object name, and
//!   slave's uuid, into objuuids, in the reply message. [UUIDMISMATCH].
//!   If the object exists in slave's repository with the same uuid,
//!   the slave does not include that object in its reply message. [SAMEUUID]
//!
//! - when the slave receives a masterlistdone message it responds
//!   with a slavelistready message. [RECVMASTERLISTDONE]
//!
//! - when the master receives a slavelistready message it sends
//!   an slaveliststart message. [SLAVELISTSTART]
//!
//! - when the slave receives a slaveliststart message, it then
//!   sends slavelist messages to the master, enumerating uuids of
//!   all objects in its repository, followed by slavelistdone. If
//!   the repository is empty, slavelistdone is sent in response to
//!   slaveliststart. [RECVSLAVELISTSTART] The slave waits until it receives
//!   a masterack message from the master before sending the next message.
//!   [RECVMASTERACK]
//!
//! - when a master receives a slavelist message, it checks each uuid
//!   from the slave against the object in the master repository, and, if
//!   different, sends the slave an objserializer message.
//!   The master also acknowledges each message with a masterack message.
//!   [SENDMASTERACK]
//!
//! - when a master receives a slavelistdone message, it responds with
//!   an copycomplete message. [SENDCOPYCOMPLETE] [RECVCOPYCOMPLETE]
//!
//! - While the repository is being copied, any concurrent changes to the
//!   repository are sent to the slve using objserializer messages.
//!   [OBJSERIALIZER]

class objrepocopy {

public:

	//! Master is asking the slave for a baton
	class batonrequest {

	public:
		batonrequest() noexcept;
		~batonrequest();

		//! Serialization function
		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
		}
	};

	//! Slave transmits its baton to the master

	class batonresponse {

	public:
		batonresponse() noexcept;
		~batonresponse();

		//! Baton uuid, if not empty
		std::string uuid;

		//! Serialization function
		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
			iter(ptr->uuid);
		}
	};

	//! Enumerated master repository uuids

	class masterlist {

	public:
		masterlist() noexcept;
		~masterlist();

		//! master repository uuids

		objuuidlist uuids;

		//! Serialization function

		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
			iter(ptr->uuids);
		}
	};

	//! End of enumerated master repository uuids

	class masterlistdone {

	public:
		masterlistdone() noexcept;
		~masterlistdone();

		//! Serialization function

		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
		}
	};

	//! slave repository uuids

	class slavelist {

	public:
		slavelist() noexcept;
		~slavelist();

		//! Slave repository uuids

		objuuidlist uuids;

		//! Serialization function

		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
			iter(ptr->uuids);
		}
	};

	//! End of slave repository uuids

	class slavelistdone {

	public:
		slavelistdone() noexcept;
		~slavelistdone();

		//! Serialization function

		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
		}
	};

	//! Slave ready to enumerate its repository's uuids

	class slavelistready {

	public:
		slavelistready() noexcept;
		~slavelistready();

		//! Serialization function

		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
		}
	};

	//! Master ready to receive slave repository's uuids

	class slaveliststart {

	public:
		slaveliststart() noexcept;
		~slaveliststart();

		//! Serialization function

		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
		}
	};

	//! Master ack of slavelist

	class masterack {

	public:
		masterack() noexcept;
		~masterack();

		//! Serialization function

		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
		}
	};

	//! Copy complete

	class copycomplete {

	public:
		copycomplete() noexcept;
		~copycomplete();

		//! Serialization function

		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
		}
	};
};

#endif
