/***************************************************************************
 *   Copyright (C) 2006 by BUI Quang Minh, Steffen Klaere, Arndt von Haeseler   *
 *   minh.bui@univie.ac.at   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "mexttree.h"

void MExtTree::generateRandomTree(TreeGenType tree_type, Params &params, bool binary) {
	switch (tree_type) {
	case YULE_HARDING: 
		generateYuleHarding(params, binary);
		break;
	case UNIFORM:
		generateUniform(params.sub_size, binary);
		break;
	case CATERPILLAR:
		generateCaterpillar(params.sub_size);
		break;
	case BALANCED:
		generateBalanced(params.sub_size);
		break;
	case STAR_TREE:
		generateStarTree(params);
		break;
	default:
		break;
	}

}

void MExtTree::setZeroInternalBranches(int num_zero_len) {
	NodeVector nodes, nodes2;
	getInternalBranches(nodes, nodes2);
	if (num_zero_len > nodes.size()) outError("The specified number of zero branches is too much");
	for (int i = 0; i < num_zero_len;) {
		int id = floor(((double) rand() / RAND_MAX) * nodes.size());
		if (id >= nodes.size()) id = nodes.size()-1;
		if (!nodes[id]) continue;
		i++;
		nodes[id]->findNeighbor(nodes2[id])->length = 0.0;
		nodes2[id]->findNeighbor(nodes[id])->length = 0.0;
		nodes[id] = NULL;
		nodes2[id] = NULL;
	}
}

void MExtTree::collapseZeroBranches(Node *node, Node *dad) {
	if (!node) node = root;
	FOR_NEIGHBOR_DECLARE(node, dad, it) {
		collapseZeroBranches((*it)->node, node);
	}
	NeighborVec nei_vec;
	nei_vec.insert(nei_vec.begin(), node->neighbors.begin(), node->neighbors.end());
	for (it = nei_vec.begin(); it != nei_vec.end(); it++) 
	if ((*it)->node != dad) {
		if ((*it)->length == 0.0) { // delete the child node
			Node *child = (*it)->node;
			bool first = true;
			FOR_NEIGHBOR_IT(child, node, it2) {
				if (first)
					node->updateNeighbor(child, (*it2)->node, (*it2)->length);
				else
					node->addNeighbor((*it2)->node, (*it2)->length);
				(*it2)->node->updateNeighbor(child, node);
				first = false;
			}
			delete child;
		}
	}
}

void MExtTree::generateCaterpillar(int size) {
	if (size < 3)
		outError(ERR_FEW_TAXA);
	root = newNode();
	int i;
	NodeVector myleaves;
	NodeVector innodes;
	Node *node;
	double len;

	innodes.push_back(root);
	// create initial tree with 3 leaves
	for (i = 0; i < 3; i++)
	{
		node = newNode();
		len = (double)(rand())/RAND_MAX;
		root->addNeighbor(node, len);
		node->addNeighbor(root, len);
		myleaves.push_back(node);
	}

	// additionally add a leaf
	for (i = 3; i < size; i++)
	{
		int index;
		index = i-1;

		node = myleaves[index];
		innodes.push_back(node);
		// add the first leaf
		Node *newleaf = newNode();
		len = (double)(rand())/RAND_MAX;
		node->addNeighbor(newleaf, len);
		newleaf->addNeighbor(node, len);
		myleaves[index] = newleaf;

		// add the second leaf
		newleaf = newNode();
		len = (double)(rand())/RAND_MAX;
		node->addNeighbor(newleaf, len);
		newleaf->addNeighbor(node, len);
		myleaves.push_back(newleaf);

	}

	root = myleaves[0];
	// indexing the leaves
	setLeavesName(myleaves);

	leafNum = myleaves.size();
	nodeNum = leafNum;
	initializeTree();

}


void MExtTree::generateBalanced(int size) {
	if (size < 3)
		outError(ERR_FEW_TAXA);
	root = newNode();
	int i;
	NodeVector myleaves;
	Node *node;
	double len;

	myleaves.push_back(root);
	// create initial tree with 2 leaves
	node = newNode();
	len = (double)(rand())/RAND_MAX;
	root->addNeighbor(node, len);
	node->addNeighbor(root, len);
	myleaves.push_back(node);

	while (myleaves.size() < size) {

		int cur_size = myleaves.size();
		// additionally add a leaf
		for (i = 0; i < cur_size && myleaves.size() < size; i++)
		{
			int index = i;
	
			node = myleaves[index];
			// add the first leaf
			Node *newleaf = newNode();
			len = (double)(rand())/RAND_MAX;
			node->addNeighbor(newleaf, len);
			newleaf->addNeighbor(node, len);
			myleaves[index] = newleaf;
	
			// add the second leaf
			newleaf = newNode();
			len = (double)(rand())/RAND_MAX;
			node->addNeighbor(newleaf, len);
			newleaf->addNeighbor(node, len);
			myleaves.push_back(newleaf);
	
		}
	}

	root = myleaves[0];
	// indexing the leaves
	setLeavesName(myleaves);

	leafNum = myleaves.size();
	nodeNum = leafNum;
	initializeTree();

}

/**
	generate a random tree following uniform model
*/
void MExtTree::generateUniform(int size, bool binary)
{
	if (size < 3)
		outError(ERR_FEW_TAXA);
	int i;

	// list of left- and right-end of branches
	NodeVector leftend, rightend, myleaves;
	Node *node;
	double len;

	root = newNode(0, "0");
	// create initial tree with 2 leaves
	node = newNode(1, "1");
	len = (double)(rand())/RAND_MAX;
	root->addNeighbor(node, len);
	node->addNeighbor(root, len);

	leftend.push_back(root);
	rightend.push_back(node);

	myleaves.push_back(root);
	myleaves.push_back(node);

	// additionally add a leaf
	for (i = 2; i < size; i++)
	{
		int index;
		index = (int)floor(((double)(rand())/RAND_MAX) * (2*i-3));
		if (index >= 2*i-3) index = 2*i-4;
		//cout << "step " << i << " left = " << leftend[index]->id << " right = " << rightend[index]->id << endl;

		// add an internal node
		Node *newnode = newNode(size+i-2);
		// reconnect the left end
		node = leftend[index];
		for (NeighborVec::iterator it = node->neighbors.begin(); it != node->neighbors.end(); it++) 
			if ((*it)->node == rightend[index]) {
				len = (double)(rand())/RAND_MAX;
				(*it)->node = newnode;
				(*it)->length = len;
				newnode->addNeighbor(node, len);
				//cout << "  left " << leftend[index]->id << " " << newnode->id << endl;
				break;
			}
		// reconnect the right end
		node = rightend[index];
		for (NeighborVec::iterator it = node->neighbors.begin(); it != node->neighbors.end(); it++) 
			if ((*it)->node == leftend[index]) {
				len = (double)(rand())/RAND_MAX;
				(*it)->node = newnode;
				(*it)->length = len;
				newnode->addNeighbor(node, len);
				//cout << "  right " << rightend[index]->id  << " " << newnode->id  << endl;
				break;
			}

		// add a new leaf
		Node *newleaf = newNode(i, i);
		len = (double)(rand())/RAND_MAX;
		newnode->addNeighbor(newleaf, len);
		newleaf->addNeighbor(newnode, len);

		// update the leftend and rightend list
		leftend.push_back(newnode);
		rightend.push_back(rightend[index]);

		leftend.push_back(newnode);
		rightend.push_back(newleaf);

		rightend[index] = newnode;

		myleaves.push_back(newleaf);

	}

	// indexing the leaves
	setLeavesName(myleaves);

	leafNum = size;
	nodeNum = leafNum;
	initializeTree();

}

/**
	generate a random tree following Yule Harding model
*/
void MExtTree::generateYuleHarding(Params &params, bool binary) {
	int size = params.sub_size;
	if (size < 3)
		outError(ERR_FEW_TAXA);
	root = newNode();
	int i;
	NodeVector myleaves;
	NodeVector innodes;
	Node *node;
	double len;

	innodes.push_back(root);
	// create initial tree with 3 leaves
	for (i = 0; i < 3; i++)
	{
		node = newNode();
		len = randomLen(params);
		root->addNeighbor(node, len);
		node->addNeighbor(root, len);
		myleaves.push_back(node);
	}

	// additionally add a leaf
	for (i = 3; i < size; i++)
	{
		int index;
		if (binary) {
			index = (int)floor(((double)(rand())/RAND_MAX) * i);
			if (index >= i) index = i-1;
		} else {
			index = (int)floor(((double)(rand())/RAND_MAX) * (i + innodes.size()));
			if (index >= i+innodes.size()) 
				index = i+innodes.size()-1;
		}
		if (index < i) {
			node = myleaves[index];
			innodes.push_back(node);
			// add the first leaf
			Node *newleaf = newNode();
			len = randomLen(params);
			node->addNeighbor(newleaf, len);
			newleaf->addNeighbor(node, len);
			myleaves[index] = newleaf;
	
			// add the second leaf
			newleaf = newNode();
			len = randomLen(params);
			node->addNeighbor(newleaf, len);
			newleaf->addNeighbor(node, len);
			myleaves.push_back(newleaf);
		}
		else {
			node = innodes[index-i];
			// add only 1 new leaf
			Node *newleaf = newNode();
			len = randomLen(params);
			node->addNeighbor(newleaf, len);
			newleaf->addNeighbor(node, len);
			myleaves.push_back(newleaf);
			
		}

	}

	root = myleaves[0];
	// indexing the leaves
	setLeavesName(myleaves);

	leafNum = myleaves.size();
	nodeNum = leafNum;
	initializeTree();


}

void MExtTree::generateStarTree(Params &params) {
	generateYuleHarding(params);
	NodeVector nodes, nodes2;
	getInternalBranches(nodes, nodes2);
	for (int i = 0; i < nodes.size(); i++) {
		nodes[i]->findNeighbor(nodes2[i])->length = 0.0;
		nodes2[i]->findNeighbor(nodes[i])->length = 0.0;
	}

}

void MExtTree::setLeavesName(NodeVector &myleaves) {
	for (int i = 0; i < myleaves.size(); i++)
	{
		myleaves[i]->id = i;
		stringstream str;
		str << 'T' << myleaves[i]->id;
		myleaves[i]->name = str.str();
	}
}


void MExtTree::reportDisagreedTrees(vector<string> &taxname, MTreeSet &trees, Split &mysplit) {
	for (MTreeSet::iterator it = trees.begin(); it != trees.end(); it++) {
		MTree *tree = (*it);
		SplitGraph sg;
		tree->convertSplits(taxname, sg);
		if (!sg.containSplit(mysplit)) {
			tree->printTree(cout, 0); // don't print branch lengths
			cout << endl;
		}
	}
}


void MExtTree::createBootstrapSupport(vector<string> &taxname, MTreeSet &trees, SplitGraph &sg, SplitIntMap &hash_ss, Node *node, Node *dad) {
	if (!node) node = root;	
	FOR_NEIGHBOR_IT(node, dad, it) {
		if (!node->isLeaf() && !(*it)->node->isLeaf()) {
			vector<int> taxa;
			getTaxaID(taxa, (*it)->node, node);
			Split mysplit(leafNum, 0.0, taxa);
			if (mysplit.shouldInvert())
				mysplit.invert();
			//mysplit.report(cout);
			//SplitIntMap::iterator ass_it = hash_ss.find(&mysplit);
			Split *sp = hash_ss.findSplit(&mysplit);
			// if found smt
			if (sp != NULL) {
				//Split *sp = ass_it->first;
				char tmp[100];
				if ((*it)->node->name.empty()) {
					sprintf(tmp, "%d", (int)(sp->getWeight()));
				} else
					sprintf(tmp, "/%d", (int)(sp->getWeight()));
				(*it)->node->name.append(tmp);
			} else {
				if (!(*it)->node->name.empty()) (*it)->node->name.append("/");
				(*it)->node->name.append("0");
				if (verbose_mode >= VB_MED) {
					cout << "split not found:" << endl;
					mysplit.report(cout);
				}
			} 
			/* new stuff: report trees that do not contain the split */
			if (strncmp((*it)->node->name.c_str(), "INFO", 4) == 0) {
				cout << "Reporting trees not containing the split " << (*it)->node->name << endl;
				reportDisagreedTrees(taxname, trees, mysplit);
			}
		}
		createBootstrapSupport(taxname, trees, sg, hash_ss, (*it)->node, node);
	}	
}

void MExtTree::createCluster(NodeVector &taxa, matrix(int) &clusters, Node *node, Node *dad) {
	if (node == NULL) node = root;
	FOR_NEIGHBOR_IT(node, dad, it) {
		// if both end-nodes are bifurcating
		Node *child = (*it)->node;
		if (!child->isLeaf()) child->name = "";
		if (node->degree() == 3 && child->degree() == 3) { 
			int count = 0;
			FOR_NEIGHBOR_DECLARE(child, node, it2)
				createCluster(count++, (*it2)->node, child);
			if (!rooted) {
				FOR_NEIGHBOR(node, child, it2) 
					createCluster(count++, (*it2)->node, node);
			} else createCluster(count++, node, child);


			clusters.resize(clusters.size()+1);
			for (NodeVector::iterator nit = taxa.begin(); nit != taxa.end(); nit++) {
				clusters.back().push_back((int)((*nit)->height));
			}
			child->name = "";
			child->name += clusters.size();
		}
		createCluster(taxa, clusters, child, node);
	}
}

void MExtTree::createCluster(int clu_num, Node *node, Node *dad) {
	if (node->isLeaf()) node->height = clu_num;
	FOR_NEIGHBOR_IT(node, dad, it) {
		createCluster(clu_num, (*it)->node, node);
	}
}

Node *MExtTree::readNCBITree(const char *infile, int root_id, const char* taxon_level) {
	ifstream in;
	cout << "Reading NCBI nodes file " << infile << endl;
	Node *parent = NULL;
	try {
		in.exceptions(ios::failbit | ios::badbit);
		in.open(infile);
		in.exceptions(ios::badbit);
		parent = readNCBITree(in, root_id, taxon_level);
		in.close();
	} catch (ios::failure) {
		outError(ERR_READ_INPUT, infile);		
	}

	return parent;
}

Node* MExtTree::readNCBITree(istream &in, int root_id, const char* taxon_level) {
	IntVector parents_id;
	NodeVector nodes;
	nodes.resize(2000000, NULL);
	StrVector node_levels;
	node_levels.resize(2000000);
	string node_level;
	int node_id, parent_id, max_node_id = 0, num_nodes = 0;
	char ch;
	in_line = in_column = 0;
	
	while (!in.eof()) {
		node_id = parent_id = 0;
		if (!(in >> node_id)) break;
		in_line++;
		num_nodes ++;
		if (node_id <= 0) throw "Wrong node ID";
		if (node_id >= nodes.size()) throw "Too large node ID";
		in >> ch;
		if (ch != '|') throw "No | between node ID and parent ID";
		in >> parent_id;		
		if (parent_id <= 0) throw "Wrong parent ID";
		if (parent_id >= nodes.size()) throw "Too large parent ID";
		in >> ch;
		if (ch != '|') throw "No | between parent ID and node rank";
		in >> node_level;

		string str;	
		getline(in, str);
		if (node_id > max_node_id) max_node_id = node_id;
		if (nodes[node_id]) throw "Duplicated node ID";
		nodes[node_id] = newNode(node_id, node_id);
		nodes[node_id]->height = parent_id; // use height temporarily for parent_id
		node_levels[node_id] = node_level;
	}
	for (node_id = 0; node_id <= max_node_id; node_id++)
	if (nodes[node_id]) {
		parent_id = nodes[node_id]->height;
		if (!nodes[parent_id]) throw "Parent ID not found";
		if (parent_id == node_id) { 
			cout << "Ignore " << node_id << " | " << parent_id << endl; 
			continue; 
		}
		nodes[node_id]->addNeighbor(nodes[parent_id],1.0);
		nodes[parent_id]->addNeighbor(nodes[node_id],1.0);
	}

	rooted = true;
	if (!nodes[root_id]) throw "Root node not available";
	root = nodes[root_id];

	if (taxon_level) {
		int pruned = pruneTaxa(node_levels, taxon_level, root, nodes[root->height]);
		if (pruned) cout << pruned << " nodes below " << taxon_level << " are pruned" << endl;
	}

	int pruned = pruneBridgeNodes(root, nodes[root->height]);
	if (pruned) cout << pruned << " nodes of degree 2 are pruned" << endl;

	leafNum = nodeNum = branchNum = 0;
	countNodeNum(root, nodes[root->height]);

/*	for (node_id = 0; node_id <= max_node_id; node_id++)
	if (nodes[node_id] && nodes[node_id]->isLeaf()) {
		Node *taxon = nodes[node_id];
		taxon->id = leafNum;
		leafNum++;
	}
	initializeTree();*/


	cout << num_nodes << " NCBI nodes, " << nodeNum << " tree nodes, " << leafNum << " leaves, " << branchNum << " branches" << endl;
	return nodes[nodes[root_id]->height];
}

int MExtTree::pruneTaxa(StrVector &node_levels, const char* taxon_level, Node *node, Node *dad) {
	int num_nodes = 0;
	if (node_levels[node->id] == taxon_level) {
		// prune subtree below node
		Neighbor *node_nei = node->findNeighbor(dad);
		FOR_NEIGHBOR_IT(node, dad, it) 
			num_nodes += freeNode((*it)->node, node);
		node->neighbors.resize(1);
		node->neighbors[0] = node_nei;
		return num_nodes;
	}
	FOR_NEIGHBOR_IT(node, dad, it) 
		num_nodes += pruneTaxa(node_levels, taxon_level, (*it)->node, node);
	return num_nodes;
}


void MExtTree::countNodeNum(Node *node, Node *dad) {
	nodeNum++;
	if (node->isLeaf()) leafNum++;
	FOR_NEIGHBOR_IT(node, dad, it) {
		branchNum++;
		countNodeNum((*it)->node, node);
	}
}

int MExtTree::pruneBridgeNodes(Node *node, Node *dad) {
	int num_nodes = 0;
	FOR_NEIGHBOR_IT(node, dad, it)
		num_nodes += pruneBridgeNodes((*it)->node, node);
	if (node->neighbors.size() == 2) {
		Node *child;
		if (node->neighbors[0]->node == dad) 
			child = node->neighbors[1]->node;
		else
			child = node->neighbors[0]->node;
		double len = node->neighbors[0]->length + node->neighbors[1]->length;
		dad->updateNeighbor(node, child, len);
		child->updateNeighbor(node, dad, len);
		delete node;
		num_nodes++;
	}
	return num_nodes;
}
