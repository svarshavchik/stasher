/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "clusterstatusnotifier.H"

clusterstatusnotifierObj::clusterstatusnotifierObj()
{
}

clusterstatusnotifierObj::~clusterstatusnotifierObj()
{
}

void clusterstatusnotifierObj::initialstatus(//! Initial status
					     const nodeclusterstatus &newStatus)

{
	statusupdated(newStatus);
}
