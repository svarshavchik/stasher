/*
** Copyright 2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "threadmgr.H"

start_threadmsgdispatcher_syncObj::start_threadmsgdispatcher_syncObj()
{
}

start_threadmsgdispatcher_syncObj::~start_threadmsgdispatcher_syncObj()
{
}

void start_threadmsgdispatcher_syncObj::thread_started()
{
	std::unique_lock<std::mutex> lock{mutex};
}
