/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repomg.H"
#include <x/dir.H>
#include <x/fd.H>
#include <x/threads/run.H>
#include <x/gnutls/session.H>
#include <x/gnutls/credentials.H>
#include <list>

#define BITSIZE "weak"
#define ALGO	"sha1"
#define CERTTYPE "rsa"

class get_keys : public repomg::key_list_callback {

public:

	mutable std::list<std::string> idlist;

	void operator()(const std::string &keyfilename,
			const std::string &clustername,
			time_t activation_time,
			time_t expiration_time) const;
};

void get_keys::operator()(const std::string &keyfilename,
			  const std::string &clustername,
			  time_t activation_time,
			  time_t expiration_time) const
{
	idlist.push_back(keyfilename);
}

static void test1(const char *clusterdir, const char *nodedir)
{
	time_t now(time(NULL));

	repomg::clustkey_generate(clusterdir, "test",
				  now,
				  now + 24 * 60 * 60,
				  CERTTYPE, BITSIZE, ALGO); // [INITCLUSTGEN]

	bool thrown=false;

	try {
		repomg::nodekey_generate(nodedir, clusterdir,
					 "",
					 "node",
					 now,
					 now+48 * 60 * 60,
					 BITSIZE, ALGO); // [EXPIRATIONCHK]
	} catch (const x::exception &e)
	{
		thrown=true;
		std::cout << "Expected exception:" << std::endl
			  << e << std::endl;
	}

	if (!thrown)
		throw EXCEPTION("Expected exception was not thrown");

	repomg::clustkey_generate(clusterdir, "test",
				  now,
				  now + 7 * 24 * 60 * 60,
				  CERTTYPE, BITSIZE, ALGO); // [NEWCLUSTGEN]
		
	repomg::nodekey_generate(nodedir, clusterdir,
				 "",
				 "node",
				 now,
				 now+48 * 60 * 60,
				 BITSIZE, ALGO); // [INITNODEGEN]

	repomg::nodekey_generate(nodedir, clusterdir,
				 "",
				 "node",
				 now,
				 now+48 * 60 * 60,
				 BITSIZE, ALGO); // [NEWNODEGEN]

	repomg::key_export(clusterdir, nodedir);

	get_keys gk;

	repomg::cluster_key_list(clusterdir, gk); // [CLUSTLIST]

	if (gk.idlist.size() != 2)
		throw EXCEPTION("Expected two keys, got something else");

	repomg::clustkey_remove(clusterdir, gk.idlist.front()); // [CLUSTREMOVE]

	repomg::nodekey_generate(nodedir, clusterdir,
				 "",
				 "node",
				 now,
				 now+48 * 60 * 60,
				 BITSIZE, ALGO);
}

class testthread : virtual public x::obj {

public:
	gnutls_connection_end_t mode;
	x::gnutls::credentials::certificate cred;

	std::string peername;

	testthread(gnutls_connection_end_t modeArg,
		   x::gnutls::credentials::certificate &credArg)
		: mode(modeArg), cred(credArg)
	{
	}

	~testthread() noexcept
	{
	}

	void run(const x::fd &start_arg);
};

void testthread::run(const x::fd &start_arg)
{
	try {
		int dummy;

		x::gnutls::session sess(x::gnutls::session::create(mode,
								   start_arg));

		sess->credentials_set(cred);
		sess->set_default_priority();
		sess->server_set_certificate_request(GNUTLS_CERT_REQUIRE);
		sess->set_private_extensions();
		if (!sess->handshake(dummy))
			throw EXCEPTION("Handshake failed");

		sess->verify_peer();

		std::list<x::gnutls::x509::crt> certList;

		sess->get_peer_certificates(certList);

		if (!sess->bye(dummy))
			throw EXCEPTION("Bye failed");

		if (!certList.empty())
			peername=certList.front()->get_dn(GNUTLS_OID_X520_COMMON_NAME);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		throw;
	}
}


static void test2(const char *clientclusterdir, const char *serverclusterdir)
{
	std::pair<x::fdptr, x::fdptr> sock(x::fd::base::socketpair());

	std::list<x::gnutls::x509::crt> nodeCert, clusterCerts;

	// [INITCRED]

	x::gnutls::credentials::certificate
		clientcert(repomg::init_credentials(clientclusterdir,
						    nodeCert, clusterCerts));

	x::gnutls::credentials::certificate
		servercert(repomg::init_credentials(serverclusterdir,
						    nodeCert, clusterCerts));

	auto clientthread=x::ref<testthread>::create(GNUTLS_CLIENT,
						     clientcert);

	auto serverthread=x::ref<testthread>::create(GNUTLS_SERVER,
						     servercert);

	auto clientret=x::run(clientthread, sock.first);
	auto serverret=x::run(serverthread, sock.second);

	sock=std::pair<x::fdptr, x::fdptr>();

	clientret->get();
	serverret->get();

	std::cout << "Client connected to " << clientthread->peername
		  << std::endl;

	std::cout << "Server connected to " << serverthread->peername
		  << std::endl;
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	static const char clusterdir1[]="conftestcluster1.dir";
	static const char nodedir1[]="conftestnode1.dir";

	static const char clusterdir2[]="conftestcluster2.dir";
	static const char nodedir2[]="conftestnode2.dir";

	try {
		x::dir::base::rmrf(clusterdir1);
		x::dir::base::rmrf(nodedir1);
		x::dir::base::rmrf(clusterdir2);
		x::dir::base::rmrf(nodedir2);

		std::cout << "test1, Part I" << std::endl;

		test1(clusterdir1, nodedir1);

		std::cout << "test1, Part II" << std::endl;
		test1(clusterdir2, nodedir2);

		std::cout << "test2, Part I" << std::endl;
		test2(nodedir1, nodedir1);

		std::cout << "test2, Part II" << std::endl;
		bool thrown=false;
		try {
			test2(nodedir1, nodedir2);
		} catch (const x::exception &e)
		{
			std::cerr << "Expected exception received: "
				  << e << std::endl;
			thrown=true;
		}

		if (!thrown)
			throw EXCEPTION("Certificate was not rejected");

		x::dir::base::rmrf(clusterdir1);
		x::dir::base::rmrf(nodedir1);
		x::dir::base::rmrf(clusterdir2);
		x::dir::base::rmrf(nodedir2);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}
