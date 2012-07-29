/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopydstinterface_H
#define objrepocopydstinterface_H

#include "objrepocopy.H"
#include "objrepocopydstinterfacefwd.H"

class objserializer;

//! Messages received by the destination side of an object repository copy

class objrepocopydstinterfaceObj : virtual public x::obj {

public:

	objrepocopydstinterfaceObj();

	~objrepocopydstinterfaceObj() noexcept;

	//! Process a batonrequest message
	virtual void event(const objrepocopy::batonrequest &msg)=0;

	//! Process a masterlist message
	virtual void event(const objrepocopy::masterlist &msg)=0;

	//! Process a masterlistdone message
	virtual void event(const objrepocopy::masterlistdone &msg)=0;

	//! Process a slaveliststart message
	virtual void event(const objrepocopy::slaveliststart &msg)=0;

	//! Process a masterack message
	virtual void event(const objrepocopy::masterack &msg)=0;

	//! Process a copycomplete message
	virtual void event(const objrepocopy::copycomplete &msg)=0;

	//! Process an objserializer message
	virtual void event(const objserializer &msg)=0;
};

#endif
