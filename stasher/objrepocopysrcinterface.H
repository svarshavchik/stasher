/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopysrcinterface_H
#define objrepocopysrcinterface_H

#include "objrepocopy.H"
#include "objrepocopysrcinterfacefwd.H"

//! Messages received by the source side of an object repository copy

class objrepocopysrcinterfaceObj : virtual public x::obj {

public:

	objrepocopysrcinterfaceObj();

	~objrepocopysrcinterfaceObj();

	//! Process a batonresponse message
	virtual void event(const objrepocopy::batonresponse &msg)=0;

	//! Process a slavelist message

	virtual void event(const objrepocopy::slavelist &msg)=0;

	//! Process a slavelistready message

	virtual void event(const objrepocopy::slavelistready &msg)=0;

	//! Process a slavelistdone message
	virtual void event(const objrepocopy::slavelistdone &msg)=0;
};

#endif
