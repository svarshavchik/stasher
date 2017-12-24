#include <stasher/client.H>
#include <stasher/manager.H>
#include <stasher/currentbase.H>

#include "inventory.H"

class subscriptionObj : public stasher::currentBaseObj<inventoryptr> {

public:
	subscriptionObj() {}
	~subscriptionObj() {}

	void update(const inventoryptr &newvalue, bool isinitial) override;

	void connection_update(stasher::req_stat_t) override;
};

void update(const inventoryptr &ptr, bool initial);

void showinventory(int argc, char **argv)
{
	if (argc < 2)
		return;

	auto client=stasher::client::base::connect();
	auto manager=stasher::manager::create();

	std::cout << "Showing current inventory, press Enter to stop"
		  << std::endl;

	auto subscriber=x::ref<subscriptionObj>::create();
	x::ref<x::obj> mcguffin=subscriber->manage(manager, client,
						   argv[1]);

	std::string dummy;

	std::getline(std::cin, dummy);
}

void subscriptionObj::update(const inventoryptr &ptr, bool initial)
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

void subscriptionObj::connection_update(stasher::req_stat_t status)
{
	std::cout << "Connection update: "
		  << x::tostring(status) << std::endl;
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
