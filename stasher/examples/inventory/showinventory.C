#include <stasher/client.H>
#include <stasher/manager.H>

#include "inventory.H"

void update(const inventoryptr &ptr, bool initial);

void showinventory(int argc, char **argv)
{
	if (argc < 2)
		return;

	auto client=stasher::client::base::connect();
	auto manager=stasher::manager::create();

	std::cout << "Showing current inventory, press Enter to stop"
		  << std::endl;

	x::ref<x::obj> mcguffin=manager->
		manage_object(client, argv[1],
			      stasher::create_managed_object<inventoryptr>
			      ([](const inventoryptr &ptr, bool initial)
		{
			update(std::move(ptr), initial);
		},
			       [](stasher::req_stat_t status)
		{
			std::cout << "Connection update: "
			<< x::tostring(status) << std::endl;
		}));

	std::string dummy;

	std::getline(std::cin, dummy);
}

void update(const inventoryptr &ptr, bool initial)
{
	std::cout << (initial ? "Current inventory:":"Updated inventory:")
		  << std::endl;

	if (ptr.null())
	{
		std::cout << "    (none)" << std::endl;
	}
	else
	{
		std::cout << "    "
			  << std::setw(30) << std::left
			  << "Item"
			  << "   "
			  << std::setw(8) << std::right
			  << "Count" << std::setw(0)
			  << std::endl;

		std::cout << "    "
			  << std::setfill('-') << std::setw(30)
			  << ""
			  << "   "
			  << std::setw(8)
			  << "" << std::setw(0) << std::setfill(' ')
			  << std::endl;

		for (auto &item:ptr->stock)
		{
			std::cout << "    "
				  << std::setw(30) << std::left
				  << item.first
				  << "   "
				  << std::setw(8) << std::right
				  << item.second << std::setw(0)
				  << std::endl;
		}
		std::cout << std::setw(75) << std::setfill('=') << ""
			  << std::setw(0) << std::setfill(' ') << std::endl;
	}

}

int main(int argc, char **argv)
{
	try {
		showinventory(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		return 1;
	}
	return 0;
}
