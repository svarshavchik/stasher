#include <stasher/client.H>
#include "inventory.H"

#include <sstream>
#include <iterator>

std::string apply_updates(const inventory &i,
			  int argc, char **argv);

void setinventory(int argc, char **argv)
{
	if (argc < 2)
		return;

	auto client=stasher::client::base::connect();

	stasher::client::base::transaction tran=
		stasher::client::base::transaction::create();

	// Get the existing object, if it exists, first.

	stasher::client::base::getreq req
		=stasher::client::base::getreq::create();

	req->openobjects=true;
	req->objects.insert(argv[1]);

	stasher::contents contents=client->get(req)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	auto existing=contents->find(argv[1]);

	if (existing == contents->end())
	{
		// Object did not exist, create a new one.

		std::string i=apply_updates(inventory::create(), argc, argv);

		if (i.empty())
			return; // New one is empty, nothing to do

		tran->newobj(argv[1], i);
	}
	else
	{
		// Update an existing object.
		auto i=apply_updates(inventory::create(existing->second.uuid,
						       existing->second.fd),
				     argc, argv);

		// If there's no inventory at all, delete it, else update it.
		if (i.empty())
			tran->delobj(argv[1], existing->second.uuid);
		else
			tran->updobj(argv[1],
				     existing->second.uuid, i);
	}

	// Note that this is a basic, simple example, that doesn't check for
	// req_rejected_stat, so if two updates occur at the same time, one
	// is going to get tossed out. This program is for demo purposes only.
	stasher::putresults results=client->put(tran);

	if (results->status != stasher::req_processed_stat)
		throw EXCEPTION(x::tostring(results->status));

	std::cout << "Updated" << std::endl;
}

// Apply updates to the inventory in an existing object.

std::string apply_updates(const inventory &i,
			  int argc, char **argv)
{
	for (int n=2; n+1<argc; n += 2)
	{
		int count=0;

		std::istringstream(argv[n+1]) >> count;

		std::string name(argv[n]);

		if (count == 0)
			i->stock.erase(name);
		else
			i->stock[name]=count;
	}

	// Now, serialize it into a string.

	std::string buf;

	if (i->stock.empty())
		return buf; // Empty inventory, return an empty string.

	typedef std::back_insert_iterator<std::string> ins_buf_iter_t;

	ins_buf_iter_t iter(buf);

	x::serialize::iterator<ins_buf_iter_t> serialize(iter);

	i->serialize(serialize);

	return buf;
}

int main(int argc, char **argv)
{
	try {
		setinventory(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		return 1;
	}
	return 0;
}
