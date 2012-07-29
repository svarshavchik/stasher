#include <x/destroycallbackflag.H>
#include <x/destroycallback.H>
#include "warehouses.H"

void load_warehouses(// Our warehouses
		     const warehouses_t &warehouses,

		     // The client
		     const stasher::client &client,

		     // The connection manager
		     const stasher::manager &manager,

		     // Mcguffins for managed object subscriptions are placed here
		     std::list<x::ref<x::obj> > &mcguffins)
{
	// We'll attach a destructor callback to the warehouses' current
	// version mcguffins.
	//
	// Each warehouse is constructed, and it has a value. It's a null
	// inventoryptr, of course. When the subscription gets opened and
	// managed, the current object in the repository gets loaded, and
	// the warehouse object initialized with that object. So the original
	// null inventoryptr gets replaced, even if the object does not exist
	// in the repository -- in that case a null inventoryptr gets
	// symbolically placed in current_value.value.
	//
	// So, in either case, the version mcguffin of the initial null
	// inventoryptr is going to get destroyed.
	//
	// Attach a dummy destructor callback to each warehouse's initial
	// version mcguffin. Then wait for the dummy destructor to get
	// destroyed. A mcguffin with destructor callbacks keeps a strong
	// reference on the callbacks, until the object gets destroyed, and the
	// callbacks get invoked. All the warehouses get this destructor
	// callback attached to them, which does nothing. When all of them get
	// destroyed, then this destructor mcguffin gets destroyed. So, we wait
	// wait for the destructor callback itself to get destroyed.

	x::destroyCallbackFlag::base::guard guard;

	auto destructor_cb=x::destroyCallback::create();

	for (auto &warehouse:warehouses->warehouses)
	{
		warehouse_t::base::current_value_t::lock
			lock(warehouse.second->current_value);

		lock->value.getversion()->addOnDestroy(destructor_cb);

		mcguffins.push_back(warehouse.second->manage(manager, client,
							     warehouse.first));
	}

	std::cout << "Waiting for objects to get loaded"
		  << std::endl;

	// We want to make sure that the subscriptions were set up. Wait for
	// the status to be anything other than req_disconnected_stat.

	for (auto &warehouse:warehouses->warehouses)
	{
		warehouse_t::base::current_value_t::lock
			lock(warehouse.second->current_value);

		lock.wait([&lock]
			  {
				  return lock->connection_status !=
					  stasher::req_disconnected_stat;
			  });

		if (lock->connection_status != stasher::req_processed_stat)
			// Some kind of a subscription error
			throw EXCEPTION(warehouse.first + ": "
					+ x::tostring(lock->connection_status));
	}

	// Ok, just because everyone connected, doesn't mean that all the
	// objects have been loaded yet.

	guard(destructor_cb);
}
