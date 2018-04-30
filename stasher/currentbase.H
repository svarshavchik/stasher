/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_currentbase_H
#define stasher_currentbase_H

#include <stasher/namespace.H>
#include <stasher/client.H>
#include <stasher/manager.H>

STASHER_NAMESPACE_START

//! Adapt create_manage_object() to invoke reference-counted object methods.

//! The template parameter is an x::ptr to an object that's described by
//! create_managed_object().
//!
//! Define a subclass that implements update(), and
//! optionally connection_update().
//! Invoking manage() with a manager object, a client handle, and an object
//! name. manage() takes care of invoking create_managed_object(), followed
//! by the \ref managerObj::manage_object() "manager->manage_object()",
//! supplying functors that invoke update() and connection_update() in a
//! manner that's described by create_managed_object()'s documentation.

template<typename ptr_type>
class currentBaseObj : public STASHER_NAMESPACE::managedconnectionupdateObj {

public:

	//! Constructor
	currentBaseObj() {}

	//! Destructor
	~currentBaseObj() {}

	//! Implemented in the subclass

	virtual void update(//! Updated value of the object in the repository.

			    //! This is a null pointer if the object was
			    //! removed.
			    const ptr_type &newvalue,

			    //! This is the initial value obtained from the repository after connecting or reconnecting to it.

			    bool isinitial)=0;

	//! Optionally implemented in the subclass.

	//! Default no-op.
	//!
	virtual void connection_update(//! Report the connection status to the object repository.
				       req_stat_t status)
		override
	{
	}

	//! Create a managed object subscription.

	//! \return a mcguffin for the subscription. update() and
	//! connect_update() get invoked until the mcguffin goes out of scope
	//! and gets destroyed (or the manager object gets destroyed, of
	//! course). This is the return value from manage_object().

	x::ref<x::obj> manage(const manager &manager,
			      const client &client,
			      const std::string &objname)
	{
		x::ref<currentBaseObj<ptr_type> > me(this);

		auto callback=create_managed_object<ptr_type>
			([me] (const ptr_type &newvalue,
			       bool isfirst)
			 {
				 me->update(newvalue, isfirst);
			 },

			 [me] (req_stat_t stat)
			 {
				 me->connection_update(stat);
			 });

		return manager->manage_object(client, objname, callback);

	}
};

STASHER_NAMESPACE_END

#endif
