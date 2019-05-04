/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/heartbeat.H"

#include <x/ymdhms.H>

LOG_CLASS_INIT(STASHER_NAMESPACE::heartbeatCommonBase);

STASHER_NAMESPACE_START

void heartbeatCommonBase::log_corrupted()
{
	LOG_FATAL("Heartbeat object corrupted, removing it");
}

void heartbeatCommonBase::log_initial()
{
	LOG_DEBUG("Received initial heartbeat, updating my own timestamp");
}

void heartbeatCommonBase::log_new_heartbeat()
{
	LOG_TRACE("Heartbeat object does not exist, creating new one");
}

void heartbeatCommonBase::log_updating_heartbeat()
{
	LOG_TRACE("Updating existing heartbeat object");
}

void heartbeatCommonBase::log_stale_heartbeat(const std::string &nodename)
{
	LOG_WARNING("Stale heartbeat for " << nodename << " purged");
}

void heartbeatCommonBase::log_next_expiration(const std::string &nodename,
					      time_t my_expiration)
{
	LOG_TRACE("My expiration(" << nodename << ") is "
		  << (std::string)x::ymdhms(my_expiration));
}

void heartbeatCommonBase::log_update_processed(req_stat_t status)
{
	LOG_TRACE("Heartbeat update processed: " + x::to_string(status));
}

x::timertask
heartbeatCommonBase::create_timer_task(const x::ref<heartbeatUpdateRequestBaseObj>
				       &update)
{
	return x::timertask::base::
		make_timer_task([update]
				{
					update->update(heartbeatCommonBase
						       ::periodic_update);
				});
}

STASHER_NAMESPACE_END
