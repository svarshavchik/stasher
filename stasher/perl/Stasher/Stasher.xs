/*
** Copyright 2013 Double Precision, Inc.
** See COPYING for distribution information.
**
** XS shim between Perl and the real C++ API, in Stasherapi.C
**
** Generally, these shims call the C++ API, which traps all exceptions.
** The XS shim returns an array to the Perl shim, with the first element of
** the array containing an error string, with the remaining array values
** varying, depending on the request.
**
** A caught exception sets the error string to the exception text. The Perl
** wrapper checks for a non-empty error string, and croaks.
*/

#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#ifdef __cplusplus
}
#endif

#include "Stasherapi.h"

// Connection object shim

class Connection {

 public:

	// This is really a stasher::client *
	void *handle;

	Connection() : handle(0)
	{
	}

	~Connection()
	{
		// Frees stasher::client *
		api_disconnect(handle);
	}
};

// Common base class for all asynchronous requests. Internally we use
// Stasher's asynchronous API, which returns a request handle, and a
// response placeholder. The request handle is the same for all requests.

class Request {

 public:

	// This is really a stasher::client::base::request *
	void *request_handle;

	Request() : request_handle(0)
	{
	}

	~Request()
	{
		// Free the request handle
		api_request_free(request_handle);
	}
};

// A shim for an outstanding GET request

class GetRequest : public Request {

 public:

	// This points to a stasher::getrequest *

	void *get_request_handle;

	GetRequest() : get_request_handle(0)
	{
	}

	~GetRequest()
	{
		api_getrequest_free(get_request_handle);
	}
};

// A shim for an outstanding PUT request

class PutRequest : public Request {

 public:

	// This points to a stasher::putrequest *

	void *put_request_handle;

	PutRequest() : put_request_handle(0)
	{
	}

	~PutRequest()
	{
		api_putrequest_free(put_request_handle);
	}
};

// A shim for an outstanding DIR request

class GetdirRequest : public Request {

 public:

	// This points to a stasher::putrequest *

	void *dir_request_handle;

	GetdirRequest() : dir_request_handle(0)
	{
	}

	~GetdirRequest()
	{
		api_dirrequest_free(dir_request_handle);
	}
};

MODULE = Stasher		PACKAGE = Stasher::Request

Request *
Request::new()

void
Request::DESTROY()

int
Request::done()
CODE:
	RETVAL = api_request_done(THIS->request_handle);
OUTPUT:
	RETVAL

void
Request::wait()
CODE:
	api_request_wait(THIS->request_handle);

MODULE = Stasher		PACKAGE = Stasher::GetRequest

void
GetRequest::api_results()
PPCODE:
	{
		// This shim on the XS side passes through the results
		// as a raw array, which is further processed by the
		// shim on the Perl side.

		std::vector<std::string> buffer;

		api_get_results(THIS->get_request_handle, buffer);

		std::vector<std::string>::const_iterator b, e;

		for (b=buffer.begin(), e=buffer.end(); b != e; ++b)
		{
			XPUSHs(sv_2mortal(newSVpv(b->c_str(), b->size())));
		}
	}

GetRequest *
GetRequest::new()

void
GetRequest::DESTROY()

MODULE = Stasher		PACKAGE = Stasher::GetdirRequest

void
GetdirRequest::api_results()
PPCODE:
	{
		// This shim on the XS side passes through the results
		// as a raw array, which is further processed by the
		// shim on the Perl side.

		std::vector<std::string> buffer;

		api_getdir_results(THIS->dir_request_handle, buffer);

		std::vector<std::string>::const_iterator b, e;

		for (b=buffer.begin(), e=buffer.end(); b != e; ++b)
		{
			XPUSHs(sv_2mortal(newSVpv(b->c_str(), b->size())));
		}
	}

GetdirRequest *
GetdirRequest::new()

void
GetdirRequest::DESTROY()

MODULE = Stasher		PACKAGE = Stasher::PutRequest

void
PutRequest::api_results()
PPCODE:
	{
		std::string error;
		std::string newuuid;

		api_put_results(THIS->put_request_handle, error,
				newuuid);

		XPUSHs(sv_2mortal(newSVpv(error.c_str(), error.size())));
		XPUSHs(sv_2mortal(newSVpv(newuuid.c_str(), newuuid.size())));
	}

PutRequest *
PutRequest::new()

void
PutRequest::DESTROY()

MODULE = Stasher		PACKAGE = Stasher::Connection

Connection *
Connection::new()

void
Connection::DESTROY()

void
Connection::connect(node, admin, async)
    const char *node
    int admin
    int async
PPCODE:

    {
	std::string error;

        api_connect(error, THIS->handle, node, admin, async);

	XPUSHs(sv_2mortal(newSVpv(error.c_str(), error.size())));
    }

void
Connection::get_request(openobjects, objects)
    int openobjects
    AV *objects
PPCODE:
{
	std::string error;

	GetRequest *req;

	// Pull object names out of the array parameter
	{
		std::vector<std::string> names;

		size_t n=av_len(objects)+1;
		names.reserve(n);

		for (size_t i=0; i<n; ++i)
		{
			SV **elem=av_fetch(objects, i, 0);
			size_t l=0;
			if (elem == NULL)
				continue;

			const char *p=SvPV(*elem, l);
			names.push_back(std::string(p, p+l));
		}

		// We return a Stasher::GetRequest object.
		req=new GetRequest();
		api_get_objects(THIS->handle, error,
				req->request_handle,
				req->get_request_handle,
				openobjects, names);
	}

	XPUSHs(sv_2mortal(newSVpv(error.c_str(), error.size())));
	SV *sv=sv_newmortal();
	sv_setref_pv(sv, "Stasher::GetRequest", (void *)req);
	XPUSHs(sv);
}

void
Connection::put_request(objects)
    AV *objects
PPCODE:
{
	std::string error;

	PutRequest *req;

	// Pull object names out of the array parameter
	{
		std::vector<std::string> names;

		size_t n=av_len(objects)+1;
		names.reserve(n);

		for (size_t i=0; i<n; ++i)
		{
			SV **elem=av_fetch(objects, i, 0);
			size_t l=0;
			if (elem == NULL)
				continue;

			const char *p=SvPV(*elem, l);
			names.push_back(std::string(p, p+l));
		}

		// We return a Stasher::PutRequest object.
		req=new PutRequest();
		api_put_objects(THIS->handle, error,
				req->request_handle,
				req->put_request_handle,
				names);
	}

	XPUSHs(sv_2mortal(newSVpv(error.c_str(), error.size())));
	SV *sv=sv_newmortal();
	sv_setref_pv(sv, "Stasher::PutRequest", (void *)req);
	XPUSHs(sv);
}

void
Connection::api_dir_request(hierarchy)
   const char *hierarchy
PPCODE:
{
	std::string error;
	GetdirRequest *req=new GetdirRequest;

        api_dir_request(THIS->handle, error,
			req->request_handle,
			req->dir_request_handle,
			hierarchy);

	XPUSHs(sv_2mortal(newSVpv(error.c_str(), error.size())));
	SV *sv=sv_newmortal();
	sv_setref_pv(sv, "Stasher::GetdirRequest", (void *)req);
	XPUSHs(sv);
}

MODULE = Stasher                PACKAGE = Stasher
void
api_defaultnodes()
PPCODE:
        {
		std::string error;

		std::set<std::string> paths;
                api_defaultnodes(error, paths);

		XPUSHs(sv_2mortal(newSVpv(error.c_str(), error.size())));

		if (error.size())
			paths.clear();

		std::set<std::string>::iterator b, e;

		for (b=paths.begin(), e=paths.end(); b != e; ++b)
		{
			XPUSHs(sv_2mortal(newSVpv(b->c_str(), b->size())));
		}
        }
