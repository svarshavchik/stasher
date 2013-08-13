#include "Stasherapi.h"
#include "stasher/client.H"

#include <sstream>

#define SAVE_HANDLE(voidh,h,t) (voidh=reinterpret_cast<void *>(new t(h)))

#define GET_HANDLE(handle,type) reinterpret_cast<type *>(handle)

#define FREE_HANDLE(handle,type) do {					\
		auto p=GET_HANDLE((handle), type);			\
									\
		(handle)=nullptr;					\
		if (p) delete p;					\
	} while (0)

static void set_error(std::string &error, const x::exception &e)
{
	std::ostringstream o;

	o << e;
	error=o.str();
}

void api_defaultnodes(std::string &error, std::set<std::string> &s)
{
	try {
		stasher::client::base::defaultnodes(s);
	} catch (const x::exception &e)
	{
		set_error(error, e);
	}
}

void api_connect(std::string &error, void *&handle,
		 const char *node,
		 int admin, int async)
{
	if (handle)
	{
		error="Handle already used";
		return;
	}

	try {
		auto client=admin ?
			async ? stasher::client::base::admin_client(node)
			: stasher::client::base::admin(node) :
			async ? stasher::client::base::connect_client(node)
			: stasher::client::base::connect(node);

		SAVE_HANDLE(handle, client, stasher::client);

	} catch (const x::exception &e)
	{
		set_error(error, e);
	}
}

void api_disconnect(void *&handle)
{
	FREE_HANDLE(handle, stasher::client);
}

/////////////////////////////////////////////////////////////////////////////

int api_request_done(void *handleptr)
{
	auto req=reinterpret_cast<stasher::client::base::request *>(handleptr);

	if (!req)
		return 0;

	return (*req)->done() ? 1:0;
}

void api_request_wait(void *handleptr)
{
	auto req=reinterpret_cast<stasher::client::base::request *>(handleptr);

	if (req)
		(*req)->wait();
}

void api_request_free(void *&request_handleptr)
{
	FREE_HANDLE(request_handleptr, stasher::client::base::request);
}

static stasher::client *getclient(void *handleptr, std::string &error)
{
	auto p=reinterpret_cast<stasher::client *>(handleptr);

	if (p == nullptr)
		error="Not connected to an internal handle";

	return p;
}

void api_limits(void *handleptr, std::vector<std::string> &buffer)
{
	buffer.push_back("");

	auto &errmsg=*--buffer.end();

	try {
		stasher::client *handle=getclient(handleptr, errmsg);

		if (!handle)
			return;

		stasher::userinit limits=(*handle)->getlimits();

#define GETLIMIT(n,v) do {				\
			std::ostringstream o;		\
			o << (v);			\
			buffer.push_back(n);		\
			buffer.push_back(o.str());	\
		} while (0)

		GETLIMIT("maxobjects", limits.maxobjects);
		GETLIMIT("maxobjectsize", limits.maxobjectsize);
		GETLIMIT("maxsubs", limits.maxsubs);
	}
	catch (const x::exception &e)
	{
		set_error(errmsg, e);
	};
}

void api_getrequest_free(void *&getrequest_handleptr)
{
	FREE_HANDLE(getrequest_handleptr, stasher::getrequest);
}

void api_get_objects(void *handleptr, std::string &error,
		     void *&request_handle,
		     void *&get_request_handle,
		     int openobjects, std::vector<std::string> &names)
{
	stasher::client *handle=getclient(handleptr, error);

	if (!handle)
		return;

	if (names.empty())
		return;
	
	try {
		auto getreq=stasher::client::base::getreq::create();

		getreq->openobjects=openobjects ? true:false;

		for (const auto &name:names)
			getreq->objects.insert(name);

		auto request=(*handle)->get_request(getreq);

		SAVE_HANDLE(request_handle, request.second,
			    stasher::client::base::request);

		SAVE_HANDLE(get_request_handle, request.first,
			    stasher::getrequest);
	} catch (const x::exception &e)
	{
		set_error(error, e);
	}
}

void api_get_results(void *handle, std::vector<std::string> &buffer)
{
	buffer.push_back("");

	auto &errmsg=*--buffer.end();

	try {
		auto get_handle=GET_HANDLE(handle, stasher::getrequest);

		if (!get_handle)
			return;

		auto contents=(*get_handle)->getmsg()->objects;

		if (!contents->succeeded)
		{
			errmsg=x::tostring(contents->errmsg);
			return;
		}

		buffer.reserve(1+contents->size()*3);

		for (const auto &obj:*contents)
		{
			buffer.push_back(obj.first);
			buffer.push_back(x::tostring(obj.second.uuid));
			buffer.push_back(obj.second.fd.null() ? "":
					 x::tostring(obj.second.fd->getFd()));
		}
	} catch (const x::exception &e)
	{
		set_error(errmsg, e);
	}
}

///////////////////////////////////////////////////////////////////////////

void api_putrequest_free(void *&putrequest_handleptr)
{
	FREE_HANDLE(putrequest_handleptr, stasher::putrequest);
}

void api_put_objects(void *handleptr, std::string &error,
		     void *&request_handle,
		     void *&put_request_handle,
		     std::vector<std::string> &names)
{
	stasher::client *handle=getclient(handleptr, error);

	if (!handle)
		return;

	try {
		auto tran=stasher::client::base::transaction::create();

		auto b=names.begin(), e=names.end();

		// Decode the array constructed in update_request()

		while (b != e)
		{
			std::string name=*b++;

			if (b == e)
			{
			bad:
				throw EXCEPTION("Bad API call");
			}

			char c=*b->c_str();

			if (++b == e)
				goto bad;

			if (c == 'd')
			{
				// Delete object, uuid:
				tran->delobj(name, *b++);
				continue;
			}

			// "n" or "u", a uuid follows the "u".

			std::string uuidstr;

			if (c == 'u')
			{
				uuidstr=*b++;
				if (b == e)
					goto bad;
			}
			else if (c != 'n')
				goto bad;

			if (*b == "v")
			{
				// Explicit value given

				if (++b == e)
					goto bad;

				if (c == 'u')
					tran->updobj(name, uuidstr, *b);
				else
				{
					tran->newobj(name, *b);
				}
			}
			else
			{
				// File descriptor given

				if (++b == e)
					goto bad;

				int n=-1;

				std::istringstream(*b) >> n;

				auto fd=x::fd::base::dup(n);

				if (c == 'u')
					tran->updobj(name, uuidstr, fd);
				else
				{
					tran->newobj(name, fd);
				}
			}
			++b;
		}
		auto request=(*handle)->put_request(tran);

		SAVE_HANDLE(request_handle, request.second,
			    stasher::client::base::request);

		SAVE_HANDLE(put_request_handle, request.first,
			    stasher::putrequest);
	} catch (const x::exception &e)
	{
		set_error(error, e);
	}
}

void api_put_results(void *handle, std::string &error,
		     std::string &newuuid)
{
	try {
		auto put_handle=GET_HANDLE(handle, stasher::putrequest);

		if (!put_handle)
			throw EXCEPTION("No request made");

		auto results=(*put_handle)->getmsg();

		switch (results->status) {
		case stasher::req_processed_stat:
			newuuid=x::tostring(results->newuuid);
			break;
		case stasher::req_rejected_stat:
			break;
		default:
			throw EXCEPTION(x::tostring(results->status));
		}
	} catch (const x::exception &e)
	{
		set_error(error, e);
	}
}

////////////////////////////////////////////////////////////////////////////

void api_dirrequest_free(void *&dirrequest_handleptr)
{
	FREE_HANDLE(dirrequest_handleptr, stasher::getdirrequest);
}

void api_dir_request(void *handleptr, std::string &error,
		     void *&request_handle, void *&dir_request_handle,
		     const char *hierarchy)
{
	stasher::client *handle=getclient(handleptr, error);

	if (!handle)
		return;

	try {
		auto request=(*handle)->getdir_request(hierarchy);

		SAVE_HANDLE(request_handle, request.second,
			    stasher::client::base::request);

		SAVE_HANDLE(dir_request_handle, request.first,
			    stasher::getdirrequest);

	} catch (const x::exception &e)
	{
		set_error(error, e);
	}
}

void api_getdir_results(void *handle, std::vector<std::string> &buffer)
{
	buffer.push_back("");

	auto &errmsg=*--buffer.end();

	try {
		auto request=GET_HANDLE(handle, stasher::getdirrequest);

		if (!request)
			return;

		auto results=(*request)->getmsg();

		if (results->status != stasher::req_processed_stat)
			throw EXCEPTION(x::tostring(results->status));

		buffer.reserve(1+results->objects.size());
		for (const auto &object:results->objects)
			buffer.push_back(object);
	} catch (const x::exception &e)
	{
		set_error(errmsg, e);
	}
}

