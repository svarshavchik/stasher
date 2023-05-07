/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef tst_nodes_H
#define tst_nodes_H

#ifndef DOXYGEN

#include "repomg.H"
#include "node.H"
#include "repocontrollermasterfwd.H"

#include <set>
#include <vector>

class tstnodes {

	size_t n;

public:

	static const char clusterdir[];

	bool useencryption;

	tstnodes(size_t n);
	~tstnodes();

	static void clustkey_generate(time_t now);
	static void update_cluster_keys(size_t i);
	static void update_cluster_keys_local(size_t i);
	static void update_node_key(time_t, size_t i);

	static std::string getnodedir(size_t i);
	static std::string getnodename(size_t i);
	static std::string getnodefullname(size_t i);

	class nodeObj : public node, virtual public x::obj {

	public:
		nodeObj(const std::string &dir);
		~nodeObj();
	};

	typedef x::ptr<nodeObj> noderef;

	void init(std::vector<noderef> &nodes,
		  const char *root_ns="");

	void init(std::vector<noderef> &nodes,
		  const std::map<std::string, std::string> &rwmap,
		  const std::map<std::string, std::string> &romap);

	void setmasteron0(std::vector<noderef> &nodes);

	repocontrollermasterptr
	startmastercontrolleron0(std::vector<noderef> &nodes);


	void startmastercontrolleron0_int(std::vector<noderef> &nodes);


	void any1canbemaster(std::vector<noderef> &nodes);


	void thesecantbemaster(std::vector<noderef> &nodes,
				      const std::set<size_t> &nomasters);


	void startreconnecter(const std::vector<noderef> &nodes);

};

#endif

#endif
