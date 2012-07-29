/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clusternotifier_H
#define clusternotifier_H

#include <x/obj.H>
#include <x/eventcallback.H>

#include "clusterinfo.H"
#include "clusternotifierfwd.H"

//! An interface for notifications of node cluster updates

//! This interface defines a virtual method, clusterupdated(). After it is
//! registered with clusterinfo, clusterupdated() gets invoked any time
//! the node's status changes.

class clusternotifierObj : virtual public x::obj {

public:
	//! Constructor
        clusternotifierObj();

	//! Destructor
        ~clusternotifierObj() noexcept;

        //! Status notification update.

        virtual void clusterupdated(//! New node status
				    const clusterinfoObj::cluster_t &newStatus)
=0;
};

#endif
