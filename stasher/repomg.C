/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repomg.H"
#include "node.H"
#include "stasher/client.H"
#include <x/fileattr.H>
#include <x/exception.H>
#include <x/pwd.H>
#include <x/grp.H>
#include <x/dir.H>
#include <x/gnutls/init.H>
#include <x/strftime.H>
#include <x/basicstringstreamobj.H>
#include <unistd.h>
#include <ctime>
#include <sys/time.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>

// Compare two certificate filenames, based on their timestamp

bool repomg::sort::operator()(const std::string &a, const std::string &b) noexcept
{
	std::string::const_iterator ab=a.begin(), bb=b.begin();

	std::string::const_iterator ap=std::find(ab, a.end(), '.'),
		bp=std::find(bb, b.end(), '.');

	// Last six digits are the microseconds.

	if (ap - ab < 6)
		ap=ab;
	else
		ap -= 6;

	if (bp - bb < 6)
		bp=bb;
	else
		bp -= 6;

	time_t ta=0, tb=0;

	std::istringstream(std::string(ab, ap)) >> ta;
	std::istringstream(std::string(bb, bp)) >> tb;

	if (ta != tb)
		return ta < tb;

	ta=tb=0;

	std::istringstream(std::string(ap, a.end())) >> ta;
	std::istringstream(std::string(bp, b.end())) >> tb;

	return ta < tb;
}

// Retrieve the name of the most recent generated certificate.
// An empty string gets returned if the directory is empty.

std::string repomg::sort::lastkey(const std::string &dirname)
{
	sort sortkeys;

	std::string filename;

	auto dirref=x::dir::create(dirname);

	for (auto keyname: *dirref)
	{
		std::string k_filename= keyname;

		std::string::iterator kb=k_filename.begin(),
			ke=k_filename.end(),
			kp=std::find(kb, ke, '.');

		if (std::string(kp, ke) != ".crt")
			continue;

		k_filename=std::string(kb, kp);

		if (filename.size() == 0 ||
		    sortkeys(filename, k_filename))
			filename=k_filename;
	}
	return filename;
}

void repomg::set_random_seed(const std::string &dirname)
{
	x::gnutls::init::set_random_seed_file(dirname + "/randseed.dat");
}

// Generate a new key

std::pair<x::gnutls::x509::crt, x::gnutls::x509::privkey>
repomg::key_generate(const std::string &dirname,
		     const std::string &nameArg,
		     time_t activation_time,
		     time_t expiration_time,
		     const std::string &key_typeArg,
		     const x::gnutls::sec_param &strength,
		     const x::gnutls::digest_algorithm &algorithm,
		     const x::gnutls::x509::crtptr &signcert,
		     const x::gnutls::x509::privkeyptr &signkey)
{
	std::string name=nameArg;
	std::string key_type=key_typeArg;
	bool need_create=false; // Need to create output directory?

	bool isdir;

	try {
		isdir=S_ISDIR(x::fileattr::create(dirname)->stat()->st_mode);
	} catch (const x::exception &e)
	{
		isdir=true;
		need_create=true;
	}

	if (!isdir)
	{
		throw EXCEPTION(dirname + " is not a directory");
	}

	if (name.size() && !signcert.null())
		name=name + "." + signcert->get_dn(GNUTLS_OID_X520_COMMON_NAME);

	if (!need_create)
	{
		std::string filename;

		if (signcert.null())
		{
			// Generating a cluster key.

			filename=sort::lastkey(dirname);

			if (filename.size())
				filename = dirname + "/" + filename + ".crt";
		}
		else
		{
			// Node key directory contains only cert.pem

			filename = nodecert(dirname);

			if (access(filename.c_str(), R_OK) < 0)
				filename="";
		}

		if (filename.size())
		{
			std::list<x::gnutls::x509::crt> certlist;

			try {
				x::gnutls::x509::crt::base::import_cert_list
					(certlist, filename,
					 GNUTLS_X509_FMT_PEM);

				if (certlist.empty())
					throw EXCEPTION("corrupted certificate file");

				std::string existing_name=certlist.front()
					->get_dn(GNUTLS_OID_X520_COMMON_NAME);

				// If generating a new cert, default name and
				// key type same as previous cert

				if (name.size() == 0)
					name=existing_name;
				else if (name != existing_name &&
					 signcert.null())
					throw EXCEPTION("Cannot change the name of a cluster");

				unsigned int dummy;

				if (key_type.size() == 0)
					key_type=x::gnutls::x509::privkey
						::base::get_algorithm_name
						(certlist.front()
						 ->get_pk_algorithm(dummy));

			} catch (x::exception &e)
			{
				throw EXCEPTION(filename << ": " << e);
			}
		}
	}

	if (name == "")
	{
		throw EXCEPTION("Must specify cluster's or node's name");
	}

	if (std::find_if(name.begin(), name.end(),
			 std::bind2nd(std::less_equal<unsigned char>(), ' '))
	    != name.end())
	{
		throw EXCEPTION("The cluster/node name contains an invalid character");
	}

	if (need_create)
	{
		if (mkdir(dirname.c_str(),
			  signkey.null() ? 0700:0777) < 0)
			throw SYSEXCEPTION(dirname);

		std::cout << "Created " << dirname << "..." << std::endl;

		if (geteuid() == 0)
			try {
				x::fileattr::create(dirname)
					->chown( x::passwd("daemon")->pw_uid,
						 x::group("daemon")->gr_gid);
			} catch (const x::exception &e)
			{
				std::cerr << e << std::endl;
			}
	}

	{
		x::filestat st(x::fileattr::create(dirname)->stat());

		if (setgid(st->st_gid) < 0 ||
		    setuid(st->st_uid) < 0)
			throw SYSEXCEPTION(dirname);
	}
			
	std::cout << "Generating new key..." << std::endl;

	auto retval=std::make_pair(x::gnutls::x509::crt::create(),
				   x::gnutls::x509::privkey::create());

	{
		x::gnutls::progress_notifier notifier;

		retval.second->generate(key_type, strength);
	}

	retval.first->set_version();

	if (signkey.null())
		retval.first->set_basic_constraints(true);

	retval.first->set_activation_time(activation_time);

	if (!signcert.null() &&
	    expiration_time > signcert->get_expiration_time())
	{
		throw EXCEPTION("Cluster certificate expires before the new "
				"certificate's expiration. "
				"Need to generate a new cluster certificate, "
				"first.");

	}

	retval.first->set_expiration_time(expiration_time);
	retval.first->set_dn_by_oid(GNUTLS_OID_X520_COMMON_NAME,
			       name);
	{
		std::ostringstream o;

		o << "object repository (created on "
		  << x::strftime(x::tzfile::base::local(), x::locale::create(""))
			(activation_time)("%F %T %Z") << ")";

		retval.first->set_dn_by_oid(GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME,
				       o.str());
	}
	retval.first->set_dn_by_oid(GNUTLS_OID_X520_ORGANIZATION_NAME,
				    signkey.null() ? "cluster":"node");

	retval.first->set_key(retval.second);
	retval.first->set_key_usage(GNUTLS_KEY_DIGITAL_SIGNATURE |
			       GNUTLS_KEY_NON_REPUDIATION |
			       GNUTLS_KEY_KEY_AGREEMENT |
			       (signkey.null() ? GNUTLS_KEY_KEY_CERT_SIGN:0) |
			       GNUTLS_KEY_CRL_SIGN);

	x::gnutls::x509::crtptr c=signcert;

	x::gnutls::x509::privkeyptr k=signkey;

	if (signkey.null())
	{
		c=retval.first;
		k=retval.second;
		retval.first->set_serial(1);
	}

	retval.first->set_subject_key_id();
	retval.first->sign(c, k, algorithm);

	x::gnutls::init::save_random_seed_file();
	return retval;
}

// Generate a new cluster key

void repomg::clustkey_generate(const std::string &dirname,
			       const std::string &name,
			       time_t activation_time,
			       time_t expiration_time,
			       const std::string &key_type,
			       const x::gnutls::sec_param &strength,
			       const x::gnutls::digest_algorithm &algorithm)

{
	set_random_seed(dirname);

	auto crtkey=
		key_generate(dirname, name, activation_time, expiration_time,
			     key_type, strength, algorithm,
			     x::gnutls::x509::crtptr(),
			     x::gnutls::x509::privkeyptr());

	std::string certfilenamebase;

	{
		std::ostringstream o;
		struct timeval tv;

		gettimeofday(&tv, 0);

		o << dirname << "/" << tv.tv_sec
		  << std::setw(6) << std::setfill('0')
		  << tv.tv_usec;

		certfilenamebase=o.str();
	}

	bool alreadyexists=false;
	try {
		x::fileattr::create(certfilenamebase+".key")->stat();

		alreadyexists=true;
	} catch (...)
	{
	}

	if (alreadyexists)
		throw EXCEPTION("Generating keys too fast: "
				+ certfilenamebase + " already exists");

	crtkey.second->export_pkcs1(certfilenamebase+".key",
				    GNUTLS_X509_FMT_PEM);
	crtkey.first->export_cert(certfilenamebase+".crt", GNUTLS_X509_FMT_PEM);
}

// Read cluster key directory

// Convenience list opens the directory and reads all *.crt files in the
// directory.

class repomg::keylist {

	x::dir keydir;
	x::dir::base::iterator b, e;

public:
	keylist(const std::string &dirname)
		: keydir(x::dir::create(dirname)),
		  b(keydir->begin()), e(keydir->end())
	{
	}

	// Retrieve the next *.crt file from the cluster key directory.

	bool operator>>(// Returned filename, sans the .crt suffix

			std::string &filename)
	{
		while (b != e)
		{
			std::string n= *b++;

			std::string::iterator sb=n.begin(),
				se=n.end(),
				p=std::find(sb, se, '.');

			if (se-p == 4 && std::equal(p, se, ".crt"))
			{
				filename=std::string(sb, p);
				return true;
			}
		}
		return false;
	}

	// Read a certificate, from the certificate file directory.

	static void retrieve_cert(// Directory name
				  const std::string &dirname,

				  // Certificate filename, sans the .crt extension
				  const std::string &certname,

				  // Output certificate name (subject name)
				  std::string &name,

				  // Output certificate starting time
				  time_t &act_time,

				  // Output certificate ending time
				  time_t &end_time)
	{
		std::string filename=dirname + "/" + certname + ".crt";

		try {
			std::list<x::gnutls::x509::crt> certlist;

			x::gnutls::x509::crt::base
			::import_cert_list(certlist, filename,
					   GNUTLS_X509_FMT_PEM);

			if (certlist.empty())
				throw EXCEPTION("corrupted certificate file");

			name=certlist.front()->get_dn
				(GNUTLS_OID_X520_COMMON_NAME);

			act_time=certlist.front()->get_activation_time();
			end_time=certlist.front()->get_expiration_time();
		} catch (const x::exception &e)
		{
			throw EXCEPTION(filename << ": " << e);
		}
	}

	static void retrieve_cert(const std::string &dirname,
				  const std::string &certname,
				  std::string &name)
	{
		time_t dummy;

		retrieve_cert(dirname, certname, name, dummy, dummy);
	}
};

void repomg::cluster_key_list(const std::string &dirname,
			      const key_list_callback &callback)

{
	// Read and sort the list of cluster keys, by their filenames, which
	// are timestamps.

	std::list<std::string> keys;

	{
		std::list<std::string> keys_raw;

		std::vector<std::string> keys_array;

		keylist kl(dirname);

		std::string n;

		while (kl >> n)
			keys_raw.push_back(n);

		keys_array.reserve(keys_raw.size());
		keys_array.insert(keys_array.end(),
				  keys_raw.begin(), keys_raw.end());
		std::sort(keys_array.begin(), keys_array.end(), sort());
		keys.insert(keys.end(), keys_array.begin(), keys_array.end());
	}

	std::list<std::string>::const_iterator
		b=keys.begin(), e=keys.end();

	while (b != e)
	{
		std::string n= *b++;

		std::string name;
		time_t act_time;
		time_t end_time;

		keylist::retrieve_cert(dirname, n,
				       name, act_time, end_time);

		callback(n, name, act_time, end_time);
	}
}

std::string repomg::rootcerts(const std::string &dirname) noexcept
{
	return dirname + "/rootcerts.pem";
}

std::string repomg::nodecert(const std::string &dirname) noexcept
{
	return dirname + "/cert.pem";
}

// Refresh a node key directory with cluster certificates

void repomg::key_export(const std::string &dirname,
			const std::string &nodedirname)

{
	STASHER_NAMESPACE::clientptr cl;

	try {
		cl=STASHER_NAMESPACE::client::base::admin(nodedirname);
	} catch (const x::exception &e)
	{
		key_export_local(dirname, nodedirname);

		std::cerr << "Warning: server not running, certificates are installed locally." << std::endl;

		return;
	}

	std::cerr << "Connected to " << nodedirname
		  << ", checking existing certificates"
		  << std::endl;

	std::set<std::string> objnameset;

	objnameset.insert(node::rootcerts);

	STASHER_NAMESPACE::contents cont=cl->get(objnameset)->objects;

	if (!cont->succeeded)
		throw EXCEPTION(cont->errmsg);

	STASHER_NAMESPACE::contents::base::map_t::iterator
		iter=cont->find(node::rootcerts);

	if (iter == cont->end())
		throw EXCEPTION("Cluster certificates not found");

	STASHER_NAMESPACE::client::base::transaction tran=
		STASHER_NAMESPACE::client::base::transaction::create();

	x::fd ofd=x::fd::base::tmpfile();

	key_export_to(ofd, dirname);

	ofd->seek(0, SEEK_SET);

	tran->updobj(node::rootcerts, iter->second.uuid, ofd);

	std::cerr << "Updating cluster certificates"
		  << std::endl;

	STASHER_NAMESPACE::putresults res=cl->put(tran);

	if (res->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::tostring(res->status));
}

void repomg::key_export_local(const std::string &dirname,
			      const std::string &nodedirname)

{
	std::string otmpname=rootcerts(nodedirname);

	x::fd ofd(x::fd::create(otmpname));

	key_export_to(ofd, dirname);

	ofd->close();
}

void repomg::key_export_to(const x::fd &ofd,
			   const std::string &dirname)

{
	x::ostream ofs=ofd->getostream();

	std::string n;

	keylist kl(dirname);

	while (kl >> n)
	{
		std::string i=dirname + "/" + n + ".crt";

		std::ifstream ifs( i.c_str());

		if (!ifs.is_open())
			throw SYSEXCEPTION(i);

		(*ofs) << ifs.rdbuf();
	}

	(*ofs) << std::flush;

	if (ofs->fail())
		throw SYSEXCEPTION("Cannot install cluster keys");
}

// Locate a specific certificate in the cluster's certificate list
std::string repomg::keyfile_find(// Cluster's directory
				 const std::string &dirname,

				 // Specify the certificate by its name, or
				 // by the raw filename

				 const std::string &name)
{
	std::string n;

	{
		keylist kl(dirname);

		while (kl >> n)
		{
			std::string keyname;

			keylist::retrieve_cert(dirname, n, keyname);

			if (keyname == name)
			{
				// Found a certificate by name. Make sure it's
				// not ambiguous.

				std::string nn;

				while (kl >> nn)
				{
					keylist::retrieve_cert(dirname, nn,
							       keyname);

					if (keyname == name)
					{
						std::cerr
							<< "Duplicate key name: "
							<< name
							<< std::endl;
						goto not_found;
					}
				}

				return n;
			}
		}
	}

	// name must be a certificate filename, sans the .crt suffix

	if (std::find(name.begin(), name.end(), '/') == name.end())
	{
		n=name;

		if (access((dirname + "/" + n + ".crt").c_str(), R_OK) == 0 &&
		    access((dirname + "/" + n + ".key").c_str(), R_OK) == 0)
			return n;
	}

	std::cerr << "Key " << name << " not found" << std::endl;

 not_found:
	return "";
}

// Remove a specific certificate

void repomg::clustkey_remove(const std::string &dirname,
			     const std::string &name)
{
	std::string n=keyfile_find(dirname, name);

	if (n.size() == 0)
		throw EXCEPTION(name + " not found");

	unlink((dirname + "/" + n + ".crt").c_str());
	unlink((dirname + "/" + n + ".key").c_str());
}

// Create a new node key

void repomg::nodekey_generate(const std::string &nodekeydir,
			      const std::string &clustkeydir,
			      const std::string &signkeyname,
			      const std::string &nodename,
			      time_t activation_time,
			      time_t expiration_time,
			      const x::gnutls::sec_param &strength,
			      const x::gnutls::digest_algorithm &algorithm)
{
	std::string certificate=
		nodekey_generate_local(nodekeydir, clustkeydir,
				       signkeyname, nodename,
				       activation_time, expiration_time,
				       strength, algorithm);

	if (nodekey_certpeer(nodekeydir, certificate))
		return;

	save_certificate(nodekeydir, certificate);
	key_export(clustkeydir, nodekeydir);

	std::cerr << "Certificate installed on this node, notifying server"
		  << std::endl;
	nodekey_certreload(nodekeydir);
}

bool repomg::nodekey_certpeer(const std::string &nodekeydir,
			      const std::string &certificate)
{
	std::string nodecertname=nodecert(nodekeydir);

	if (access(nodecertname.c_str(), 0))
		return false;

	x::fd file=x::fd::base::open(nodecertname, O_RDONLY);

	x::istream i=file->getistream();

	std::string existing_certificate=
		std::string(std::istreambuf_iterator<char>(*i),
			    std::istreambuf_iterator<char>());

	if (certificate_name(existing_certificate, "existing certificate") ==
	    certificate_name(certificate, "new certificate"))
		return false;

	STASHER_NAMESPACE::client cl=
		STASHER_NAMESPACE::client::base::admin(nodekeydir);

	auto result=cl->setnewcert(certificate);

	if (!result->success)
		throw EXCEPTION(result->message);

	std::cout << result->message << std::endl;
	return true;
}

void repomg::nodekey_certreload(const std::string &nodekeydir)

{
	try {
		STASHER_NAMESPACE::client::base::admin(nodekeydir)
			->certreload();
	} catch (const x::exception &e)
	{
		std::cerr << "Server is not running" << std::endl;
		return;
	}

	std::cerr << "Certificate reloaded" << std::endl;
}

std::string
repomg::nodekey_generate_local(const std::string &nodekeydir,
			       const std::string &clustkeydir,
			       const std::string &signkeyname,
			       const std::string &nodename,
			       time_t activation_time,
			       time_t expiration_time,
			       const x::gnutls::sec_param &strength,
			       const x::gnutls::digest_algorithm &algorithm)
{
	std::string signkeyfile;

	set_random_seed(nodekeydir);

	if (signkeyname.size() > 0)
	{
		signkeyfile=keyfile_find(clustkeydir, signkeyname);
	}
	else
	{
		signkeyfile=sort::lastkey(clustkeydir);
	}

	if (signkeyfile.size() == 0)
	{
		throw EXCEPTION("Cannot find the signing key");
	}

	x::gnutls::x509::crt signcert;

	{
		std::list<x::gnutls::x509::crt> certlist;

		std::string crtname=clustkeydir + "/" + signkeyfile + ".crt";
		x::gnutls::x509::crt::base::import_cert_list(certlist, crtname,
						       GNUTLS_X509_FMT_PEM);

		if (certlist.empty())
			throw EXCEPTION("corrupted certificate file: "
					+ crtname);

		if (certlist.size() != 1)
			throw EXCEPTION(crtname +
					" cannot contain a chained cert");

		signcert=certlist.front();
	}

	unsigned int nbits;

	std::string key_type=x::gnutls::x509::privkey::base
		::get_algorithm_name(signcert->get_pk_algorithm(nbits));

	x::gnutls::x509::privkey signkey(x::gnutls::x509::privkey::create());

	signkey->import_pkcs1(clustkeydir + "/" + signkeyfile + ".key",
			      GNUTLS_X509_FMT_PEM);

	auto crtkey=
		key_generate(nodekeydir, nodename, activation_time,
			     expiration_time,
			     x::gnutls::x509::privkey::base
			     ::get_algorithm_name(signkey->get_pk_algorithm()),
			     strength, algorithm, signcert, signkey);

	x::ostringstream os=x::ostringstream::create();

	(*os) << crtkey.first->print() << std::endl;
	crtkey.first->export_cert(GNUTLS_X509_FMT_PEM)->write(os);
	crtkey.second->export_pkcs1(GNUTLS_X509_FMT_PEM)->write(os);

	os->flush();
	return os->str();
}

x::gnutls::datum_t repomg::loadcerts(const std::string &filename,
				     std::list<x::gnutls::x509::crt> &certList)

{
	x::gnutls::datum_t datum(x::gnutls::datum_t::create());

	datum->load(filename);

	x::gnutls::x509::crt::base::import_cert_list(certList, datum,
						     GNUTLS_X509_FMT_PEM);

	if (certList.empty())
		throw EXCEPTION("Empty certificate chain: " + filename);

	return datum;
}

std::string repomg::certificate_name(const std::string &certificate,
				     const std::string &what)
{
	std::list<x::gnutls::x509::crt> cert;

	x::gnutls::x509::crt::base
		::import_cert_list(cert,
				   x::gnutls::datum_t
				   ::create(certificate.begin(),
					    certificate.end()),
				   GNUTLS_X509_FMT_PEM);

	if (cert.empty())
		throw EXCEPTION("Empty certificate chain: " + what);

	return cert.front()->get_dn(GNUTLS_OID_X520_COMMON_NAME);
}

bool repomg::update_certificate(const std::string &dirname,
				const std::string &newcert)
{
	std::list<x::gnutls::x509::crt> existing;
	std::list<x::gnutls::x509::crt> clusterCerts;

	init_credentials(dirname, existing, clusterCerts);

	if (existing.empty() ||
	    existing.front()->get_dn(GNUTLS_OID_X520_COMMON_NAME) !=
	    certificate_name(newcert, "new certificate"))
		return false;

	save_certificate(dirname, newcert);
	return true;
}

void repomg::save_certificate(const std::string &dirname,
			      const std::string &newcert)
{
	x::fd newfile=x::fd::create(nodecert(dirname), 0600);

	newfile->write_full(&newcert[0], newcert.size());
	newfile->close();
}

x::gnutls::credentials::certificate
repomg::init_credentials(const std::string &dirname,
			 std::list<x::gnutls::x509::crt> &nodeCert,
			 std::list<x::gnutls::x509::crt> &clusterCerts)

{
	set_random_seed(dirname);

	x::gnutls::credentials::certificate
		cred(x::gnutls::credentials::certificate::create());

	x::gnutls::datum_t datum=loadcerts(nodecert(dirname), nodeCert);

	clusterCerts.clear();
	loadcerts(rootcerts(dirname), clusterCerts);
	cred->set_x509_key_mem(datum, datum, GNUTLS_X509_FMT_PEM);
	cred->set_x509_trust(clusterCerts);

	return cred;
}
