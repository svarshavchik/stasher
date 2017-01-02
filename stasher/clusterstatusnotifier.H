/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clusterstatusnotifier_H
#define clusterstatusnotifier_H

#include <x/obj.H>
#include <x/ref.H>

#include "clusterstatusnotifierfwd.H"

class nodeclusterstatus;

//! An interface for notifications of node cluster status updates

//! This interface defines a virtual method, statusupdated(). After it is
//! registered with clusterinfo, statusupdated() gets invoked any time
//! the node's status changes.

class clusterstatusnotifierObj : virtual public x::obj {

public:
        clusterstatusnotifierObj();
        ~clusterstatusnotifierObj();

	//! Callback argument type

	typedef nodeclusterstatus arg_t;

	//! Initial status, upon registering.

	//! The default implementation invokes statusupdated().
	//!
        virtual void initialstatus(//! Initial status
				   const nodeclusterstatus &newStatus);


        //! Status notification update.

        virtual void statusupdated(//! New node status
                                   const nodeclusterstatus &newStatus)=0;
};

#endif
