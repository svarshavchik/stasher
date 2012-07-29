#include <x/destroycallback.H>
#include <x/destroycallbackflag.H>
#include <stasher/client.H>
#include <stasher/manager.H>
#include <stasher/current.H>
#include <stasher/versionedptr.H>
#include <stasher/process_request.H>
#include "inventory.H"
#include "warehouses.H"

#include <sstream>
#include <list>
#include <map>
#include <type_traits>


// Inventory adjustment

// The parameters to adjinventory are groups of three:
// name, what, howmany.
//
// "name" gives the name of an inventoryptr in the object repository.
// "what" is the inventory item's name. "howmany" is how many of "what" to be
// adjusted in the "name" inventory object.
//
// Note that the number of inventory objects, called warehouses here, that
// can be processed is limited by the maximum number of subscriptions, however
// there's no limit (within reason) as to the actual number of adjustments to
// same or different inventory items in different warehouses. The
// same warehouse may have multiple adjustments of the same or different item.
//
// All adjustments get processed at the same time. There is no guarantee of the
// order of adjustments, so some of them may be rejected due to insufficient
// inventory in the warehouse (even if another adjustment was given of the same
// inventory item in the same warehouse, that would've increased the inventory
// level sufficiently).

class adjinfoObj;
typedef x::ref<adjinfoObj> adjinfo;

void do_adjustments(const stasher::client &client,
		    const warehouses_t &warehouses,
		    std::list<adjinfo> &adjs);

// Information about a transfer: the name of the warehouse,
// what it is, and how many of them.

class adjinfoObj : virtual public x::obj {

public:
	std::string name;
	std::string what;
	int howmuch;

	// The processing status gets placed here

	stasher::req_stat_t processed;

	adjinfoObj(const std::string &nameArg,
		   const std::string &whatArg,
		   int howmuchArg) : name(nameArg),
				     what(whatArg), howmuch(howmuchArg)
	{
	}

	~adjinfoObj() noexcept
	{
	}

	std::string descr() const
	{
		std::ostringstream o;

		o << "adjust " << what << " by " << howmuch
		  << " in " << name;
		return o.str();
	}
};

void adjinventory(int argc, char **argv)
{
	// The list of transfers parsed from the command line.

	std::list<adjinfo> adjs;

	// All the warehouses elicited from the transfers, combined:
	warehouses_t warehouses=warehouses_t::create();

	// Parse command line options.

	for (int i=1; i+2 < argc; i += 3)
	{
		int n=0;

		std::istringstream(argv[i+2]) >> n;

		auto adjinfo=adjinfo::create(argv[i], argv[i+1], n);

		warehouses->createwarehouse(adjinfo->name);
		adjs.push_back(adjinfo);
	}

	auto client=stasher::client::base::connect();
	auto manager=stasher::manager::create();

	// Load the existing inventory, start subscriptions

	std::list<x::ref<x::obj> > mcguffins;

	load_warehouses(warehouses, client, manager, mcguffins);

	std::cout << "Existing inventory:" << std::endl;

	warehouses->inventory();

	// Perform the transfers. If any of them where req_rejected_stat-ed,
	// repeat them.

	do
	{
		// Submit the transfers, wait for them to get processed.

		do_adjustments(client, warehouses, adjs);

		for (auto b=adjs.begin(), e=adjs.end(), p=b;
		     b != e; )
		{
			p=b;
			++b;

			if ((*p)->processed != stasher::req_rejected_stat)
				adjs.erase(p);
		}
	} while (!adjs.empty());
}

// Helper object to invoke a functor, that takes a stasher::putresults as an
// argument, from a destructor callback.

template<typename Functor> class invokeProcessedFunctorObj
	: public x::destroyCallbackObj {

public:

	Functor f;
	stasher::putresults res;

	template<typename FunctorArg>
	invokeProcessedFunctorObj(FunctorArg && fArg,
				  const stasher::putresults &resArg)
		: f(std::forward<FunctorArg>(fArg)), res(resArg)
	{
	}

	~invokeProcessedFunctorObj() noexcept
	{
	}

	void destroyed() noexcept
	{
		f(res);
	}
};

// Lock the inventory object in a warehouse. Pass the inventory object to a
// functor. The functor is expected to return a transactionptr object. If it's
// not null, the functor doesn't want to do a transaction.
//
// Returns a std::pair of the transactionptr object, and the current inventory
// object's version mcguffin.

template<typename Functor>
std::pair<stasher::client::base::transactionptr, x::ref<x::obj> >
make_transaction(const warehouse_t &warehouse, Functor &&f)
{
	warehouse_t::base::current_value_t::lock
		lock(warehouse->current_value);

	return std::make_pair(f((const inventoryptr &)lock->value),
			      lock->value.getversion());
}

// The first functor gets passed to make_transaction. The returned transaction
// is submitted to the object repository. The second functor gets invoked with
// the transaction's stasher::putresults as an argument.
//
// If the transaction fails with a req_rejected_stat, the functor gets called
// only after the original inventory object has already been updated with
// the new value of the inventory object in the object repository. This is done
// by invoking it as part of the destructor callback of the original object's
// version mcguffin. This is an immediate process if the inventory object has
// already been updated, because our reference would be the last one on the
// mcguffin, so its destructor callback will get invoked immediately, after
// the destructor callback gets installed, and everything goes out of scope.


template<typename MakeTransactionFunctor,
	 typename ProcessTransactionFunctor>
void do_adjustment(const stasher::client &client,
		   const warehouse_t &warehouse,
		   MakeTransactionFunctor && make_tran,
		   ProcessTransactionFunctor && process_tran)
{
	auto transaction=make_transaction(warehouse,
					  std::forward<MakeTransactionFunctor>
					  (make_tran));

	if (transaction.first.null())
		return; // The first functor did not want to do it.

	auto mcguffin=transaction.second;

	stasher::process_request(client->put_request(transaction.first),
				 [process_tran, mcguffin]
				 (const stasher::putresults &res)
				 {
					 auto callback=x::ref<
						 invokeProcessedFunctorObj
						 <ProcessTransactionFunctor>

						 >::create(std::move
							   (process_tran),
							   res);

					 if (res->status ==
					     stasher::req_rejected_stat)
					 {
						 mcguffin->addOnDestroy
							 (callback);
						 return;
					 }

					 // Invoke the functor now.

					 callback->destroyed();
				 });
}

// Create a transaction that adjusts the inventory
// This is called by the first functor in do_adjustment().

stasher::client::base::transactionptr
adjust_inventory(const inventoryptr &ptr,
		 const adjinfo &adj)
{
	inventoryptr i;

	if (adj->howmuch == 0)
	{
		std::cout << "Very funny: " + adj->descr() + "\n"
			  << std::flush;
		return stasher::client::base::transactionptr();
	}

	if (ptr.null())
	{
		// No existing inventory object

		if (adj->howmuch < 0)
		{
			std::cout << "Insufficient stock: " + adj->descr()
				+ "\n"
				  << std::flush;
			return stasher::client::base::transactionptr();
		}

		i=inventory::create();
		i->stock[adj->what]=adj->howmuch;
	}
	else
	{
		i=inventoryptr::create(*ptr); // Clone the object

		inventoryObj &newi= *i;

		// Find existing item. Make it 0, if it does not exist.
		auto iter=newi.stock.find(adj->what);

		if (iter == newi.stock.end())
			iter=newi.stock.insert(std::make_pair(adj->what, 0))
				.first;

		if (adj->howmuch < 0 && -iter->second > adj->howmuch)
		{
			std::cout << "Insufficient stock: " + adj->descr()
				+ "\n"
				  << std::flush;
			return stasher::client::base::transactionptr();
		}

		iter->second += adj->howmuch;

		// Delete 0-stocked item. If this is the last item in the
		// inventory, delete the entire object.

		if (iter->second == 0)
			newi.stock.erase(iter);

		if (newi.stock.empty())
		{
			auto tran=stasher::client::base::transaction::create();

			tran->delobj(adj->name, ptr->uuid);
			return tran;
		}
	}

	// Serialize the new inventory object, then insert or update it.

	std::string s;

	typedef std::back_insert_iterator<std::string> insert_iter_t;

	insert_iter_t insert_iter(s);

	x::serialize::iterator<insert_iter_t> ser_iter(insert_iter);

	i->serialize(ser_iter);

	auto tran=stasher::client::base::transaction::create();

	if (ptr.null())
	{
		// No existing object, insert it.

		tran->newobj(adj->name, s);
	}
	else
	{
		// Update it.

		tran->updobj(adj->name, ptr->uuid, s);
	}
	return tran;
}

void do_adjustments(const stasher::client &client,
		    const warehouses_t &warehouses,
		    std::list<adjinfo> &adjs)
{
	// Each functor, below, captures this mcguffin by value.
	// Guard the mcguffin. When it is completely destroyed, it means that
	// all transactions have been processed.

	x::destroyCallbackFlag::base::guard guard;

	x::ref<x::obj> mcguffin=x::ref<x::obj>::create();

	guard(mcguffin);

	for (auto &adj : adjs)
	{
		// Be optimistic.
		adj->processed=stasher::req_processed_stat;

		do_adjustment(client,
			      warehouses->warehouses.find(adj->name)
			      ->second,
			      [adj]
			      (const inventoryptr &ptr)
			      {
				      std::cout << "Adjusting: "
					      + adj->descr() + "\n"
						<< std::flush;
				      return adjust_inventory(ptr, adj);
			      },

			      [adj, mcguffin]
			      (const stasher::putresults &res)
			      {
				      std::cout << x::tostring(res->status)
						+ ": "
						+ adj->descr() + "\n"
						<< std::flush;

				      adj->processed=res->status;
			      });
	}
}

int main(int argc, char **argv)
{
	try {
		adjinventory(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		return 1;
	}
	return 0;
}
