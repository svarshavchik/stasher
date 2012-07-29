/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "baton.H"
#include "repocontrollermaster.H"
#include "repopeerconnection.H"

#include <sstream>

batonObj::batonObj(const std::string &resigningnodeArg,
		   const x::uuid &resigninguuidArg,
		   const std::string &replacementnodeArg,
		   const x::uuid &batonuuidArg)
	: is_replacement_baton(false),
	  resigningnode(resigningnodeArg),
	  resigninguuid(resigninguuidArg),
	  replacementnode(replacementnodeArg),
	  batonuuid(batonuuidArg),

	  received_flag(false),
	  cancelled_flag(false),
	  cluster_cleared_flag(false)
{
}

batonObj::~batonObj() noexcept
{
	privatemeta_t::lock lock(privatemeta); // Useless

	for (auto peer : lock->master_peers)
	{
		repopeerconnectionptr ptr=peer.getptr();

		if (ptr.null())
			continue;

		ptr->baton_master_release(resigningnode, resigninguuid);
	}

	repocontrollermasterptr ptr=lock->master_ptr.getptr();

	if (!ptr.null())
		ptr->handoff_failed();

}

void batonObj::set_master_ptr(const repocontrollermaster &master_ptrArg)

{
	privatemeta_t::lock(privatemeta)->master_ptr=master_ptrArg;
}

void batonObj::set_commitlock(const tobjrepoObj::commitlock_t &commitlockArg)

{
	privatemeta_t::lock(privatemeta)->commitlock=commitlockArg;
}

tobjrepoObj::commitlock_t batonObj::get_commitlock()
{
	privatemeta_t::lock lock(privatemeta);

	tobjrepoObj::commitlockptr_t commitlock=lock->commitlock;

	return commitlock;
}

LOG_FUNC_SCOPE_DECL(batonObj::announced, announcedLog);

void batonObj::announced()
{
	repocontrollermasterptr ptr=
		({
			LOG_FUNC_SCOPE(announcedLog);

			privatemeta_t::lock lock(privatemeta);

			LOG_INFO("Baton received by all peers except for the new master");

			repocontrollermasterptr p=lock->master_ptr.getptr();

			p;
		});

	if (!ptr.null())
		ptr->masterbaton_announced(batonptr(this));
}

void batonObj::set_master_newconn(const repopeerconnectionptr
				  &master_newconnArg)

{
	privatemeta_t::lock(privatemeta)->master_newconn=master_newconnArg;
}

void batonObj::push_back_master_peers(const repopeerconnectionptr &peer)

{
	privatemeta_t::lock(privatemeta)->master_peers.push_back(peer);
}

void batonObj::master_handoff_complete()
{
	privatemeta_t::lock lock(privatemeta);

	lock->master_peers.clear();
	lock->master_ptr=repocontrollermasterptr();
}

LOG_FUNC_SCOPE_DECL(batonObj::send_transfer_request, sendLog);

void batonObj::send_transfer_request()
{
	LOG_FUNC_SCOPE(sendLog);

	privatemeta_t::lock lock(privatemeta);

	LOG_INFO("Sending the baton to the new master");

	repopeerconnectionptr ptr=lock->master_newconn.getptr();

	if (ptr.null())
		LOG_ERROR("No connection to transfer the baton");
	else
		ptr->baton_transfer_request(baton(this));
}

void batonObj::get_master_peers_names(std::set<std::string> &names)

{
	privatemeta_t::lock lock(privatemeta);

	for (auto peer : lock->master_peers)
	{
		repopeerconnectionptr ptr=peer.getptr();

		if (ptr.null())
			continue;

		names.insert(ptr->peername);
	}
}

batonObj::operator std::string() const
{
	std::ostringstream o;

	o << "Resigning master: " << resigningnode
	  << " (" << x::tostring(resigninguuid) << "), new master: "
	  << replacementnode << ", baton uuid: " << x::tostring(batonuuid);

	return o.str();
}
