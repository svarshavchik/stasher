/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repomg_H
#define repomg_H

#include <x/exception.H>
#include <x/gnutls/sec_param.H>
#include <x/gnutls/x509_crt.H>
#include <x/gnutls/x509_privkey.H>
#include <x/gnutls/credentials.H>

//! Object repository cluster management functions.

//! clustkey_generate() creates a repository cluster key and saves it into a
//! newly created directory. [INITCLUSTGEN]
//! If invoked for an existing directory, a new cluster key gets generated and
//! saved in the directory. [NEWCLUSTGEN]
//!
//! nodekey_generate() creates a node key, signed by the repository cluster
//! key, and saves it into a newly created directory. [INITNODEGEN]
//! If invoked for an existing directory, a new node key gets generated and
//! replaces the previous key. [NEWNODEGEN]
//!
//! If the server is running, a message is sent to the server that
//! causes it to reload the node certificate. [NODEKEYUPDATE]
//!
//! nodekey_generate() fails if the node key's expiration time is after the
//! cluster key's expiration time. [EXPIRATIONCHK]
//!
//! cluster_key_list() enumerates keys in the repository cluster directory
//! to std::cout. [CLUSTLIST]
//!
//! key_export() installs the current list of cluster keys into a node key
//! directory, updating the list of cluster keys. If there's a server running
//! the cluster keys get updated in the object repository directly, and
//! distributed to all nodes [KEYEXPORTDIST].
//!
//! clustkey_remove() removes a cluster key from the cluster directory.
//! [CLUSTREMOVE]

class repomg {

	//! Sort keys in timestamp order (embedded in filename)

	class sort {

	public:
		//! Comparison operator

		bool operator()(const std::string &a,
				const std::string &b) noexcept;

		//! Find the most recent key in a directory

		static std::string lastkey(const std::string &dirname);

	};

	class keylist;

	//! Find a certificate file

	static std::string keyfile_find(//! Cluster's directory
					const std::string &dirname,

					//! Specify the certificate by its name, or
					//! by the raw filename

					const std::string &name);


	//! Key generation

	static std::pair<x::gnutls::x509::crt, x::gnutls::x509::privkey>
	key_generate(//! Output directory
		     const std::string &dirname,

		     //! Name of the certificate
		     const std::string &name,

		     //! Activation time
		     time_t activation_time,

		     //! Expiration time
		     time_t expiration_time,

		     //! rsa or dsa
		     const std::string &key_type,

		     //! Key strength.
		     const x::gnutls::sec_param &strength,

		     //! Digest algorithm
		     const x::gnutls::digest_algorithm &algorithm,

		     //! Signing certificate. If null, self signed cert
		     const x::gnutls::x509::crtptr &signcert,

		     //! Signing certificate's corresponding public key
		     const x::gnutls::x509::privkeyptr &signkey);


public:
	//! Generate a new cluster key

	static void clustkey_generate(//! Output directory, created if does not exist
				      const std::string &dirname,

				      //! Cluster's name
				      const std::string &name,

				      //! Activation time
				      time_t activation_time,
				      //! Expiration time
				      time_t expiration_time,

				      //! rsa or dsa
				      const std::string &key_type,

				      //! Key strength.
				      const x::gnutls::sec_param &strength,

				      //! Digest algorithm
				      const x::gnutls::digest_algorithm &algorithm);


	//! Generate a node certificate

	//! Call nodekey_generate_local() to create the actual
	//! certificate, then attempt to connect to the running server,
	//! and tell it to reload the certificate.

	static void nodekey_generate(//! Node key directory
				     const std::string &nodekeydir,

				     //! Cluster key directory
				     const std::string &clustkeydir,

				     //! Sign new node with this key. Blank for the most
				     //! recent cert
				     const std::string &signkeyname,

				     //! Name of the new node
				     const std::string &nodename,

				     //! Activation time
				     time_t activation_time,

				     //! Expiration time
				     time_t expiration_time,

				     //! Key strength.
				     const x::gnutls::sec_param &strength,

				     //! Digest algorithm
				     const x::gnutls::digest_algorithm &algorithm);

	//! Create a node certificate, return it in string form.

	static std::string
	nodekey_generate_local(//! Node key directory
			       const std::string &nodekeydir,

			       //! Cluster key directory
			       const std::string &clustkeydir,

			       //! Sign new node with this key. Blank for the most recent cert
			       const std::string &signkeyname,

			       //! Name of the new node
			       const std::string &nodename,

			       //! Activation time
			       time_t activation_time,

			       //! Expiration time
			       time_t expiration_time,

			       //! Key strength.
			       const x::gnutls::sec_param &strength,

			       //! Digest algorithm
			       const x::gnutls::digest_algorithm &algorithm);

	//! Manually reload the certificate

	static void nodekey_certreload(const std::string &nodekeydir);


	//! Callback method for cluster_key_list()

	class key_list_callback {

	public:
		//! Functor invoked by cluster_key_list()

		virtual
		void operator()(//! The key's identifier
				const std::string &keyfilename,

				//! Cluster name, should be the same in all keys
				const std::string &clustername,

				//! Activation time,
				time_t activation_time,

				//! Expiration time
				time_t expiration_time) const=0;
	};

	//! Enumerate cluster keys.

	static void cluster_key_list(const std::string &dirname,
				     const key_list_callback &callback);


	//! Update cluster keys in a node directory

	//! If there's a server running, the cluster keys get
	//! updated directly in the object repository, otherwise this
	//! calls key_export_local();

	static void key_export(const std::string &dirname,
			       const std::string &nodedirname);


	//! Update cluster keys in a node directory only

	static void key_export_local(const std::string &dirname,
				     const std::string &nodedirname);


	//! Remove a cluster key

	static void clustkey_remove(const std::string &dirname,
				    const std::string &name);


	//! Filename in the node directory where root certificates are kept

	static std::string rootcerts(const std::string &dirname) noexcept;

	//! Filename in the node directory where node certificate is saved

	static std::string nodecert(const std::string &dirname) noexcept;

	//! Initialize a credentials object

	//! Given a node certificate directory, load the private key, cert,
	//! and the root cert list. Returns the credentials object; also
	//! returns the node certificate and the root cert list. [INITCRED]

	static x::gnutls::credentials::certificate
	init_credentials(//! Node certificate directory
			 const std::string &dirname,

			 //! The node certificate is loaded here
			 std::list<x::gnutls::x509::crt> &nodeCert,

			 //! The root certificates that are returned
			 std::list<x::gnutls::x509::crt> &clusterCerts);

	//! Extract the name of a PEM-formatted certificate

	static std::string
	certificate_name(//! A PEM formatted certificate
			 const std::string &certificate,

			 //! Where the certificate came from (included in a thrown exception's error message)
			 const std::string &what);

	//! Update certificate in a node directory.

	//! Checks to make sure that the DNs are the same. Returns \c false
	//! if they're not, \c true if they are, and the certificate was
	//! installed.

	static bool update_certificate(const std::string &dirname,
				       const std::string &newcert);

	//! Convenience function: load certificates from a file

	static
	x::gnutls::datum_t loadcerts(//! Filename
				     const std::string &filename,

				     //! Certificates are loaded here
				     std::list<x::gnutls::x509::crt> &certList);

private:

	//! Point gnutls to a random seed file

	static void set_random_seed(const std::string &dirname);

	//! Write certificate keys into a file descriptor

	static void key_export_to(const x::fd &ofd,
				  const std::string &dirname);

	//! Save a certificate into a directory
	static void save_certificate(const std::string &dirname,
				     const std::string &newcert);

	//! Send the certificate to another cluster peer

	static bool nodekey_certpeer(const std::string &nodekeydir,
				     const std::string &certificate)
		LIBCXX_INTERNAL;
};

#endif
