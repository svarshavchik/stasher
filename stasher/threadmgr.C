#include "threadmgr.H"

start_thread_syncObj::start_thread_syncObj()
{
}

start_thread_syncObj::~start_thread_syncObj() noexcept
{
}

void start_thread_syncObj::thread_started()
{
	std::unique_lock<std::mutex> lock{mutex};
}
