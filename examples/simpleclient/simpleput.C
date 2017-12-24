#include <iostream>
#include <stasher/client.H>

void simpleput(int argc, char **argv)
{
	if (argc < 2)
		throw EXCEPTION("Usage: simpleput {object} {value}");

	stasher::client client=stasher::client::base::connect();

	stasher::client::base::getreq req
		=stasher::client::base::getreq::create();

	req->objects.insert(argv[1]);

	stasher::contents contents=client->get(req)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	stasher::client::base::transaction tran=
		stasher::client::base::transaction::create();

	auto existing=contents->find(argv[1]);

	const char *what;

	if (existing != contents->end())
	{
		if (argc > 2)
		{
			what="updated";
			tran->updobj(argv[1], existing->second.uuid, argv[2]);
		}
		else
		{
			what="deleted";
			tran->delobj(argv[1], existing->second.uuid);
		}
	}
	else
	{
		if (argc > 2)
			tran->newobj(argv[1], argv[2]);
		else
			throw EXCEPTION("Object doesn't exist anyway");
		what="created";
	}

	stasher::putresults results=client->put(tran);

	if (results->status == stasher::req_processed_stat)
	{
		std::cout << "Object "
			  << what
			  << ", new uuid: "
			  << x::tostring(results->newuuid)
			  << std::endl;
	}
	else
	{
		throw EXCEPTION(x::tostring(results->status));
	}
}

int main(int argc, char **argv)
{
	try {
		simpleput(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}

	return 0;
}
