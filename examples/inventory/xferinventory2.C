#include <x/destroycallback.H>
#include <x/destroycallbackflag.H>
#include <stasher/client.H>
#include <stasher/manager.H>
#include <stasher/current.H>
#include <stasher/versionedptr.H>
#include <stasher/versionedput.H>
#include "inventory.H"
#include "warehouses.H"

#include <sstream>
#include <list>
#include <map>

// Inventory transfer.

// The parameters to xferinventory are groups of four:
// from, to, what, howmany.
//
// "from" and "to" are names of inventoryptr objects.
//
// "what" is the inventory name. "howmany" is how many of "what" to be
// transferred from the "from" inventory object to the "to" one.
//
// Note that the number of inventory objects, called warehouses here, that
// can be processed is limited by the maximum number of subscriptions, however
// there's no limit (within reason) as to the actual number of transfers of
// same or different inventory items between the different warehouses. The
// same warehouse may be a source or a destination of multiple transfers.
//
// All transfers get processed at the same time. There is no guarantee of the
// order of transfers, so some of them may be rejected due to insufficient
// inventory in the "from" warehouse (even if there's another incoming transfer
// of this inventory item into this warehouse from another warehouse, since the
// transfer order is not guaranteed).
//
// This is an example of a versioned put, with an automatic retry in case of
// a collission: all transfers get kicked off at the same time; when the same
// warehouse is involved in multiple transfers, it's likely that multiple
// updates of the same object go out together, and one of them is going to
// get stasher::req_rejected_stat, in which case it's simply tried again.
// The versioned put makes sure that for a req_rejected_stat transaction, at
// least one participating object's version has changed.

// Some forward declarations

class xferinfoObj;
typedef x::ref<xferinfoObj> xferinfo;

int adjust(const inventoryptr &existing,
	   const std::string &objectname,

	   const std::string &what,
	   int howmuch,
	   const stasher::client::base::transaction &transaction);

void do_transfers(const stasher::client &client,
		  const warehouses_t &warehouses,
		  std::list<xferinfo> &transfers);


std::pair<std::string, int> do_adjust(const inventory &existing,
				      const std::string &what,
				      int howmuch);


// Information about a transfer: the name of the warehouses where something
// gets transferred from and to, what it is, and how many of them.

class xferinfoObj : virtual public x::obj {

public:
	std::string from, to;
	std::string what;
	int howmuch;

	// This is set by create_versioned_put if there's not enough
	// inventory to perform this transfer.
	bool isenough;

	// The processing status gets placed here

	stasher::req_stat_t processed;

	xferinfoObj(const std::string &fromArg,
		    const std::string &toArg,
		    const std::string &whatArg,
		    int howmuchArg) : from(fromArg), to(toArg),
				      what(whatArg), howmuch(howmuchArg),
				      isenough(true) // Be optimistic
	{
	}

	~xferinfoObj()
	{
	}

	std::string descr() const
	{
		std::ostringstream o;

		o << "transfer " << howmuch << " " << what
		  << " from " << from << " to " << to;
		return o.str();
	}
};

////////////////////////////////////////////////////////////////////////////
//
// Take our warehouses, and one transfer.
//
// Create a transaction effecting the transfer, and collect the original
// versions of each warehouse's inventory that were used to build the
// transaction that updates both warehouses' inventory objects.

std::pair<stasher::client::base::transaction,
	  stasher::versionscollected>
create_versioned_put(const warehouses_t &warehouses,
		     const xferinfo &xfer)
{
	// Lock the from and the to warehouse.

	// Note: in this example, create_versioned_put() always gets called
	// from the same thread. If this were a multithreaded context, we would
	// have to lock xfer->from and xfer->to in alphabetical order, in
	// order to avoid a potential deadlock against a thread that's doing
	// a transfer in the opposite "direction" (can be the same or a
	// different inventory item).
	//
	// But, since this is a single thread, this is ok. The manager
	// updates each current object one at a time, so there's no
	// possibility of a deadlock.
	//
	// Doing this in alphabetical order would've been ugly. Thankfully,
	// I don't have to do it.

	warehouse_t::base::current_value_t::lock
		from(warehouses->warehouses.find(xfer->from)
		     ->second->current_value);

	warehouse_t::base::current_value_t::lock
		to(warehouses->warehouses.find(xfer->to)
		   ->second->current_value);

	// Create a transaction, and collect the current versions of the
	// objects that go into the transaction.

	auto transaction=stasher::client::base::transaction::create();
	auto versions=stasher::versionscollected::base
		::create_from(from->value, to->value);

	if (adjust(from->value, xfer->from,
		   xfer->what, -xfer->howmuch, transaction) < 0)
	{
		// Not enough in the from inventory

		xfer->isenough=false;
	}
	else
	{
		adjust(to->value, xfer->to,
		       xfer->what, xfer->howmuch, transaction);
	}
	return std::make_pair(transaction, versions);
}

// Apply a transfer to an inventory, and update the transaction object,
// accordingly.

// Returns the new inventory level of the selected item.

int adjust(const inventoryptr &existing,
	   const std::string &objectname,

	   const std::string &what,
	   int howmuch,
	   const stasher::client::base::transaction &transaction)
{
	if (existing.null())
	{
		// New inventory object

		inventory dummy=inventory::create();

		auto result=do_adjust(dummy, what, howmuch);

		if (result.first.size() == 0)
		{
			// Marginal: no inventory before and after. We give up.

			return result.second;
		}

		transaction->newobj(objectname, result.first);
		return result.second;
	}

	auto result=do_adjust(existing, what, howmuch);

	if (result.first.size() == 0) // Empty inventory!
	{
		transaction->delobj(objectname, existing->uuid);
	}
	else
	{
		transaction->updobj(objectname, existing->uuid, result.first);
	}

	return result.second;
}

// Ok, the task is now reduced to taking this inventory object, updating
// the inventory level, and then serializing it back, and returning the
// new level.

std::pair<std::string, int> do_adjust(const inventory &existing,
				      const std::string &what,
				      int howmuch)
{
	// Clone the object

	auto cpy=inventory::create(*existing);

	// Find this object in the inventory map.

	auto iter=cpy->stock.find(what);

	if (iter == cpy->stock.end())
	{
		// Doesn't exist, create it.
		iter=cpy->stock.insert(std::make_pair(what, 0)).first;
	}

	iter->second += howmuch;

	std::pair<std::string, int> ret;

	ret.second=iter->second;

	// Inventory of 0 removes this item from the inventory, completely.

	if (ret.second == 0)
		cpy->stock.erase(iter);

	// Return an empty string if the inventory is empty. This results
	// in the object getting deleted.

	if (!cpy->stock.empty())
	{
		typedef std::back_insert_iterator<std::string> insert_iter_t;

		insert_iter_t insert_iter(ret.first);

		x::serialize::iterator<insert_iter_t> ser_iter(insert_iter);

		cpy->serialize(ser_iter);
	}
	return ret;
}

void xferinventory(int argc, char **argv)
{
	// The list of transfers parsed from the command line.

	std::list<xferinfo> transfers;

	// All the warehouses elicited from the transfers, combined:
	warehouses_t warehouses=warehouses_t::create();

	// Parse command line options.

	for (int i=1; i+3 < argc; i += 4)
	{
		int n=0;

		std::istringstream(argv[i+3]) >> n;

		if (n <= 0)
		{
			std::cerr << "Eh?" << std::endl;
			return;
		}

		auto xferinfo=xferinfo::create(argv[i], argv[i+1],
					       argv[i+2], n);

		warehouses->createwarehouse(xferinfo->from);
		warehouses->createwarehouse(xferinfo->to);
		transfers.push_back(xferinfo);
	}

	auto client=stasher::client::base::connect();
	auto manager=stasher::manager::create();

	// Load the existing inventory, start subscriptions

	std::list<x::ref<x::obj> > mcguffins;

	load_warehouses(warehouses, client, manager, mcguffins);

	std::cout << "Transfering between:" << std::endl;

	warehouses->inventory();

	// Perform the transfers. If any of them where req_rejected_stat-ed,
	// repeat them.

	do
	{
		// Submit the transfers, wait for them to get processed.

		do_transfers(client, warehouses, transfers);

		for (auto b=transfers.begin(), e=transfers.end(), p=b;
		     b != e; )
		{
			p=b;
			++b;

			if ((*p)->processed != stasher::req_rejected_stat)
				transfers.erase(p);
		}
	} while (!transfers.empty());
}

void do_transfers(const stasher::client &client,
		  const warehouses_t &warehouses,
		  std::list<xferinfo> &transfers)
{
	// Each versioned_put, below, captures the mcguffin by value.
	// Guard the mcguffin. When it is completely destroyed, it means that
	// all transactions have been processed.

	x::destroyCallbackFlag::base::guard guard;

	x::ref<x::obj> mcguffin=x::ref<x::obj>::create();

	guard(mcguffin);

	for (auto &transfer : transfers)
	{
		// Be optimistic.
		transfer->processed=stasher::req_processed_stat;

		if (transfer->from == transfer->to)
		{
			std::cout << "Very funny: " << transfer->descr()
				  << std::endl;
			continue;
		}

		std::pair<stasher::client::base::transaction,
			  stasher::versionscollected>
			transaction=create_versioned_put(warehouses, transfer);

		if (!transfer->isenough)
		{
			std::cout << "Insufficient inventory: "
				  << transfer->descr()
				  << std::endl;
			continue;
		}
		std::cout << "Processing: " << transfer->descr() << std::endl;

		stasher::versioned_put_request
			(client, transaction.first,
			 [transfer, mcguffin]
			 (const stasher::putresults &res)
			 {
				 std::ostringstream o;

				 o << x::tostring(res->status)
				   << ": "
				   << transfer->descr()
				   << std::endl;
				 std::cout << o.str()
					   << std::flush;
				 transfer->processed=res->status;
			 },
			 transaction.second);
	}
}

int main(int argc, char **argv)
{
	try {
		xferinventory(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		return 1;
	}
	return 0;
}
