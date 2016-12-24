/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objwriterthread.H"
#include <x/stopexception.H>

LOG_CLASS_INIT(STASHER_NAMESPACE::objwriterthreadObj);

STASHER_NAMESPACE_START

class objwriterthreadObj::requestObj : virtual public x::obj {

public:
	requestObj();
	~requestObj() noexcept;

	virtual void serialize(objwriterObj &writer)
=0;
};

objwriterthreadObj::requestObj::requestObj()
{
}

objwriterthreadObj::requestObj::~requestObj() noexcept
{
}

class objwriterthreadObj::writeRequestObj : public requestObj {

	x::ptr<writtenobjbaseObj> object;

public:
	writeRequestObj(const x::ptr<writtenobjbaseObj> &objectArg)
;
	~writeRequestObj() noexcept;

	void serialize(objwriterObj &writer);
};

objwriterthreadObj::writeRequestObj
::writeRequestObj(const x::ptr<writtenobjbaseObj> &objectArg)
 : object(objectArg)
{
}

objwriterthreadObj::writeRequestObj::~writeRequestObj() noexcept
{
}

void objwriterthreadObj::writeRequestObj::serialize(objwriterObj &writer)

{
	object->serialize(writer);
}

class objwriterthreadObj::stopRequestObj : public requestObj {

public:
	stopRequestObj();
	~stopRequestObj() noexcept;

	void serialize(objwriterObj &writer);
};

objwriterthreadObj::stopRequestObj::stopRequestObj()
{
}

objwriterthreadObj::stopRequestObj::~stopRequestObj() noexcept
{
}

class objwriterthreadObj::closeexception : public x::exception {

public:
	closeexception() noexcept;
	~closeexception() noexcept;
};

objwriterthreadObj::closeexception::closeexception() noexcept
{
}

objwriterthreadObj::closeexception::~closeexception() noexcept
{
}


void objwriterthreadObj::stopRequestObj::serialize(objwriterObj &writer)

{
	throw closeexception();
}


objwriterthreadObj::objwriterthreadObj(const std::string &nameArg,
				       size_t bufsize)
	: objwriterObj(bufsize), name(nameArg)
{
}

std::string objwriterthreadObj::getName() const noexcept
{
	return name;
}

objwriterthreadObj::~objwriterthreadObj() noexcept
{
}

void objwriterthreadObj::dispatch_setmaxqueuesize(size_t max_request_count)
{
	this->max_request_count=max_request_count/2;
	request_count_warn_level=this->max_request_count/2;
}

void objwriterthreadObj::write(const x::ref<writtenobjbaseObj> &object)
{
	write_object(object);
}

void objwriterthreadObj::dispatch_write_object(const x::ref<writtenobjbaseObj> &object)
{
	if (max_request_count)
	{
		size_t s=requests.size();

		if (s >= max_request_count)
			throw EXCEPTION ("Write queue size exceeded");

		if (s >= request_count_warn_level)
		{
			LOG_WARNING(getName() + ": write queue is growing");

			request_count_warn_level = max_request_count;
		}

		if (s < max_request_count / 4)
			request_count_warn_level=max_request_count/2;
	}

	requests.push(x::ref<writeRequestObj>::create(object));
}

void objwriterthreadObj::dispatch_request_close()
{
	requests.push(x::ref<stopRequestObj>::create());
}

void objwriterthreadObj::process_messages()
{
	LOG_TRACE("Processing messages");

	msgqueue_t msgqueue=get_msgqueue();

	while (!msgqueue->empty())
		msgqueue->pop()->dispatch();
}

void objwriterthreadObj::run(msgqueue_auto &msgqueue,
			     const x::ref<x::obj> &mcguffin)
{
	max_request_count=0;

	try {
		try {
			while (1)
			{
				process_messages();

				if (!requests.empty())
				{
					LOG_TRACE("Serializing objects");
					do
					{
						request req(requests.front());

						requests.pop();

						req->serialize(*this);
					} while (!requests.empty());
					continue;
				}

				if (buffered())
				{
					LOG_TRACE("Flushing output");
					pubflush();
					continue;
				}

				// Nothing to flush, no messages, wait for
				// an event on this blocking descriptor.
				msgqueue->getEventfd()->event();
			}
		} catch (const closeexception &e)
		{
			LOG_INFO("Flushing remaining output");
			try {
				while (buffered())
					pubflush();
			} catch (...) {
			}
		}
	} catch (const x::stopexception &e)
	{
		LOG_TRACE("Writer thread caught a stop()");
	} catch (const x::exception &e)
	{
		LOG_FATAL(e);
	}
}

void objwriterthreadObj::writeflush()
{
	while (buffered())
		pubflush();
}

std::string objwriterthreadObj::report(std::ostream &rep)
{
	rep << requests.size() << " unwritten messages" << std::endl;
	return getName();
}

STASHER_NAMESPACE_END
