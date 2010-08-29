/***************************************************************************
 *   Copyright (C) 2009 by BUI Quang Minh   *
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
#include "iqptree.h"

//TODO Only to test
int cntBranches = 0;

IQPTree::IQPTree() :
PhyloTree() {
    k_represent = 0;
    p_delete = 0.0;
    dist_matrix = NULL;
    //bonus_values = NULL;
    nbIQPIter = 0; // Number of iteration before the speed up is started
    nbNNI95 = 0.0;
    deltaNNI95 = 0;
    curScore = 0.0; // Current score of the tree
    bestScore = 0.0; // Best score found sofar
    cur_pars_score = -1;
    enable_parsimony = false;
    enableHeuris = false; // This is set true when the heuristic started (after N iterations)
    linRegModel = NULL;
}

IQPTree::~IQPTree() {
    //if (bonus_values)
    //delete bonus_values;
    //bonus_values = NULL;
    if (dist_matrix)
        delete[] dist_matrix;
    dist_matrix = NULL;
    if (root != NULL)
        freeNode();
    root = NULL;
}

void IQPTree::setRepresentNum(int k_rep) {
    k_represent = k_rep;
}

void IQPTree::setProbDelete(double p_del) {
    p_delete = p_del;
}

void IQPTree::setIQPIterations(STOP_CONDITION stop_condition,
        double stop_confidence, int min_iterations, int max_iterations) {
    stop_rule.setStopCondition(stop_condition);
    stop_rule.setConfidenceValue(stop_confidence);
    stop_rule.setIterationNum(min_iterations, max_iterations);
}

void IQPTree::setIQPAssessQuartet(IQP_ASSESS_QUARTET assess_quartet) {
    iqp_assess_quartet = assess_quartet;
}

RepresentLeafSet* IQPTree::findRepresentLeaves(vector<RepresentLeafSet*> &leaves_vec, int nei_id,
        PhyloNode *dad) {
    PhyloNode *node = (PhyloNode*) (dad->neighbors[nei_id]->node);
    int set_id = dad->id * 3 + nei_id;
    if (leaves_vec[set_id]) return leaves_vec[set_id];
    RepresentLeafSet *leaves = new RepresentLeafSet;
    RepresentLeafSet * leaves_it[2] = {NULL, NULL};
    leaves_vec[set_id] = leaves;
    RepresentLeafSet::iterator last;
    RepresentLeafSet::iterator cur_it;
    int count, i, j;
    double admit_height = 1000000;

    leaves->clear();
    if (node->isLeaf()) {
        // set the depth to zero
        //node->height = 0.0;
        leaves->insert(new RepLeaf(node, 0));
    } else {
        for (i = 0, j = 0; i < node->neighbors.size(); i++)
            if (node->neighbors[i]->node != dad) {
                leaves_it[j++] = findRepresentLeaves(leaves_vec, i, node);
            }
        assert(j == 2 && leaves_it[0] && leaves_it[1]);
        if (leaves_it[0]->empty() && leaves_it[1]->empty()) {
            cout << "wrong";
        }
        RepresentLeafSet::iterator lit[2] = {leaves_it[0]->begin(), leaves_it[1]->begin()};
        while (leaves->size() < k_represent) {
            int id = -1;
            if (lit[0] != leaves_it[0]->end() && lit[1] != leaves_it[1]->end()) {
                if ((*lit[0])->height < (*lit[1])->height)
                    id = 0;
                else if ((*lit[0])->height > (*lit[1])->height)
                    id = 1;
                else { // tie, choose at random
                    id = floor((rand() / RAND_MAX)*2);
                }
            } else
                if (lit[0] != leaves_it[0]->end())
                id = 0;
            else if (lit[1] != leaves_it[1]->end())
                id = 1;
            else break;
            assert(id < 2 && id >= 0);
            leaves->insert(new RepLeaf((*lit[id])->leaf, (*lit[id])->height + 1));
            lit[id]++;
        }
    }
    assert(!leaves->empty());
    /*
            if (verbose_mode >= VB_MAX) {
                    for (cur_it = leaves->begin(); cur_it != leaves->end(); cur_it++)
                            cout << (*cur_it)->leaf->name << " ";
                    cout << endl;
            }*/
    return leaves;
}

/*
void IQPTree::clearRepresentLeaves(vector<RepresentLeafSet*> &leaves_vec, Node *node, Node *dad) {
        int nei_id;
        for (nei_id = 0; nei_id < node->neighbors.size(); nei_id++)
                if (node->neighbors[nei_id]->node == dad) break;
        assert(nei_id < node->neighbors.size());
        int set_id = node->id * 3 + nei_id;
        if (leaves_vec[set_id]) {
                for (RepresentLeafSet::iterator rlit = leaves_vec[set_id]->begin(); rlit != leaves_vec[set_id]->end(); rlit++)
                        delete (*rlit);
                delete leaves_vec[set_id];
                leaves_vec[set_id] = NULL;
        }
        FOR_NEIGHBOR_IT(node, dad, it) {
                clearRepresentLeaves(leaves_vec, (*it)->node, node);
        }
}*/

void IQPTree::deleteLeaf(Node *leaf) {
    Node *near_node = leaf->neighbors[0]->node;
    assert(leaf->isLeaf() && near_node->degree() == 3);
    Node *node1 = NULL;
    Node *node2 = NULL;
    double sum_len = 0.0;

    FOR_NEIGHBOR_IT(near_node, leaf, it) {
        sum_len += (*it)->length;
        if (!node1)
            node1 = (*it)->node;
        else
            node2 = (*it)->node;
    }
    // make sure that the returned node1 and node2 are correct
    assert(node1 && node2);
    // update the neighbor
    node1->updateNeighbor(near_node, node2, sum_len);
    node2->updateNeighbor(near_node, node1, sum_len);
}

void IQPTree::deleteLeaves(PhyloNodeVector &del_leaves, PhyloNodeVector &adjacent_nodes) {
    NodeVector taxa;
    // get the vector of taxa
    getTaxa(taxa);
    root = NULL;
    int num_delete = floor(p_delete * taxa.size());
    int i;
    if (num_delete > taxa.size() - 4) num_delete = taxa.size() - 4;
    // now try to randomly delete some taxa of the probability of p_delete
    for (i = 0; i < num_delete;) {
        int id = floor((((double) rand()) / RAND_MAX) * taxa.size());
        if (!taxa[id]) continue;
        else i++;
        PhyloNode *taxon = (PhyloNode*) taxa[id];
        del_leaves.push_back(taxon);
        adjacent_nodes.push_back((PhyloNode*) (taxon->neighbors[0]->node));
        deleteLeaf(taxon);
        taxa[id] = NULL;
    }
    // set root to the first taxon which was not deleted
    for (i = 0; i < taxa.size(); i++)
        if (taxa[i]) {
            root = taxa[i];
            break;
        }
}

int IQPTree::assessQuartet(Node *leaf0, Node *leaf1, Node *leaf2,
        Node *del_leaf) {
    assert(dist_matrix);
    int nseq = aln->getNSeq();
    int id0 = leaf0->id, id1 = leaf1->id, id2 = leaf2->id;
    double dist0 = dist_matrix[leaf0->id * nseq + del_leaf->id]
            + dist_matrix[leaf1->id * nseq + leaf2->id];
    double dist1 = dist_matrix[leaf1->id * nseq + del_leaf->id]
            + dist_matrix[leaf0->id * nseq + leaf2->id];
    double dist2 = dist_matrix[leaf2->id * nseq + del_leaf->id]
            + dist_matrix[leaf0->id * nseq + leaf1->id];
    if (dist0 < dist1 && dist0 < dist2)
        return 0;
    if (dist1 < dist2)
        return 1;
    return 2;
}

int IQPTree::assessQuartetParsimony(Node *leaf0, Node *leaf1, Node *leaf2,
        Node *del_leaf) {
    int score[3] = {0, 0, 0};
    for (Alignment::iterator it = aln->begin(); it != aln->end(); it++) {
        char ch0 = (*it)[leaf0->id];
        char ch1 = (*it)[leaf1->id];
        char ch2 = (*it)[leaf2->id];
        char chd = (*it)[del_leaf->id];
        if (ch0 >= aln->num_states || ch1 >= aln->num_states || ch2
                >= aln->num_states || chd >= aln->num_states)
            continue;
        if (chd == ch0 && ch1 == ch2)
            score[0] += (*it).frequency;
        if (chd == ch1 && ch0 == ch2)
            score[1] += (*it).frequency;
        if (chd == ch2 && ch0 == ch1)
            score[2] += (*it).frequency;
    }
    if (score[0] == score[1] && score[0] == score[2])
        return floor(((double) (rand()) / RAND_MAX) * 3);
    if (score[0] > score[1] && score[0] > score[2])
        return 0;
    if (score[1] < score[2])
        return 2;
    return 1;
}

void IQPTree::initializeBonus(PhyloNode *node, PhyloNode *dad) {
    if (!node)
        node = (PhyloNode*) root;
    if (dad) {
        PhyloNeighbor *node_nei = (PhyloNeighbor*) node->findNeighbor(dad);
        PhyloNeighbor *dad_nei = (PhyloNeighbor*) dad->findNeighbor(node);
        node_nei->lh_scale_factor = 0.0;
        node_nei->partial_lh_computed = 0;
        dad_nei->lh_scale_factor = 0.0;
        dad_nei->partial_lh_computed = 0;
    }

    FOR_NEIGHBOR_IT(node, dad, it) {
        initializeBonus((PhyloNode*) ((*it)->node), node);
    }
}

void IQPTree::raiseBonus(Neighbor *nei, Node *dad, double bonus) {
    ((PhyloNeighbor*) nei)->lh_scale_factor += bonus;
    if (verbose_mode >= VB_DEBUG)
        cout << dad->id << " - " << nei->node->id << " : " << bonus << endl;

    //  FOR_NEIGHBOR_IT(nei->node, dad, it)
    //	raiseBonus((*it), nei->node, bonus);
}

double IQPTree::computePartialBonus(Node *node, Node* dad) {
    PhyloNeighbor *node_nei = (PhyloNeighbor*) node->findNeighbor(dad);
    if (node_nei->partial_lh_computed) return node_nei->lh_scale_factor;

    FOR_NEIGHBOR_IT(node, dad, it) {
        node_nei->lh_scale_factor += computePartialBonus((*it)->node, node);
    }
    node_nei->partial_lh_computed = 1;
    return node_nei->lh_scale_factor;
}

void IQPTree::findBestBonus(double &best_score, NodeVector &best_nodes, NodeVector &best_dads, Node *node, Node *dad) {
    double score;
    if (!node)
        node = root;
    if (!dad) {
        best_score = 0;
    } else {
        score = computePartialBonus(node, dad) + computePartialBonus(dad, node);
        if (score >= best_score) {
            if (score > best_score) {
                best_score = score;
                best_nodes.clear();
                best_dads.clear();
            }
            best_nodes.push_back(node);
            best_dads.push_back(dad);
        }
        //cout << node->id << " - " << dad->id << " : " << best_score << endl;
    }

    FOR_NEIGHBOR_IT(node, dad, it) {
        findBestBonus(best_score, best_nodes, best_dads, (*it)->node, node);
    }
}

void IQPTree::assessQuartets(vector<RepresentLeafSet*> &leaves_vec, PhyloNode *cur_root, PhyloNode *del_leaf) {
    const int MAX_DEGREE = 3;
    RepresentLeafSet * leaves[MAX_DEGREE];
    double bonus[MAX_DEGREE];
    memset(bonus, 0, MAX_DEGREE * sizeof (double));
    int cnt = 0;

    // only work for birfucating tree
    assert(cur_root->degree() == MAX_DEGREE);

    // find the representative leaf set for three subtrees

    FOR_NEIGHBOR_IT(cur_root, NULL, it) {
        leaves[cnt] = findRepresentLeaves(leaves_vec, cnt, cur_root);
        cnt++;
    }
    for (RepresentLeafSet::iterator i0 = leaves[0]->begin(); i0
            != leaves[0]->end(); i0++)
        for (RepresentLeafSet::iterator i1 = leaves[1]->begin(); i1
                != leaves[1]->end(); i1++)
            for (RepresentLeafSet::iterator i2 = leaves[2]->begin(); i2
                    != leaves[2]->end(); i2++) {
                int best_id;
                if (iqp_assess_quartet == IQP_DISTANCE)
                    best_id = assessQuartet((*i0)->leaf, (*i1)->leaf, (*i2)->leaf, del_leaf);
                else
                    best_id = assessQuartetParsimony((*i0)->leaf, (*i1)->leaf, (*i2)->leaf,
                        del_leaf);
                bonus[best_id] += 1.0;
            }
    for (cnt = 0; cnt < MAX_DEGREE; cnt++)
        if (bonus[cnt] > 0.0)
            raiseBonus(cur_root->neighbors[cnt], cur_root, bonus[cnt]);

}

void IQPTree::reinsertLeaf(Node *leaf, Node *adjacent_node, Node *node,
        Node *dad) {
    bool first = true;
    Neighbor *nei = node->findNeighbor(dad);
    double len = nei->length;

    FOR_NEIGHBOR_IT(adjacent_node, leaf, it) {
        if (first) {
            (*it)->node = node;
            (*it)->length = len / 2;
            node->updateNeighbor(dad, adjacent_node, len / 2);
        } else {
            (*it)->node = dad;
            (*it)->length = len / 2;
            dad->updateNeighbor(node, adjacent_node, len / 2);
        }
        first = false;
    }
}

void IQPTree::reinsertLeaves(PhyloNodeVector &del_leaves,
        PhyloNodeVector &adjacent_nodes) {
    PhyloNodeVector::iterator it_leaf, it_node;

    int num_del_leaves = del_leaves.size();
    assert(root->isLeaf());

    for (it_leaf = del_leaves.begin(), it_node = adjacent_nodes.begin(); it_leaf
            != del_leaves.end(); it_leaf++, it_node++) {
        if (verbose_mode >= VB_DEBUG)
            cout << "Reinserting " << (*it_leaf)->name << " (" << (*it_leaf)->id << ")" << endl;
        vector<RepresentLeafSet*> leaves_vec;
        leaves_vec.resize(nodeNum * 3, NULL);
        initializeBonus();
        NodeVector nodes;
        getInternalNodes(nodes);
        if (verbose_mode >= VB_DEBUG)
            drawTree(cout, WT_BR_SCALE | WT_INT_NODE | WT_TAXON_ID | WT_NEWLINE);
        //printTree(cout, WT_BR_LEN | WT_INT_NODE | WT_TAXON_ID | WT_NEWLINE);
        for (NodeVector::iterator it = nodes.begin(); it != nodes.end(); it++) {
            assessQuartets(leaves_vec, (PhyloNode*) (*it), (*it_leaf));
        }
        NodeVector best_nodes, best_dads;
        double best_bonus;
        findBestBonus(best_bonus, best_nodes, best_dads);
        if (verbose_mode >= VB_DEBUG)
            cout << "Best bonus " << best_bonus << " " << best_nodes[0]->id << " " << best_dads[0]->id << endl;
        assert(best_nodes.size() == best_dads.size());
        int node_id = floor((((double) rand()) / RAND_MAX) * best_nodes.size());
        if (best_nodes.size() > 1 && verbose_mode >= VB_DEBUG)
            cout << best_nodes.size()
            << " branches show the same best bonus, branch nr. "
            << node_id << " is chosen" << endl;

        reinsertLeaf(*it_leaf, *it_node, best_nodes[node_id],
                best_dads[node_id]);
        //clearRepresentLeaves(leaves_vec, *it_node, *it_leaf);
        /*if (verbose_mode >= VB_DEBUG) {
         printTree(cout);
         cout << endl;
         }*/
        for (vector<RepresentLeafSet*>::iterator rit = leaves_vec.begin(); rit != leaves_vec.end(); rit++)
            if ((*rit)) {
                RepresentLeafSet *tit = (*rit);
                for (RepresentLeafSet::iterator rlit = tit->begin(); rlit != tit->end(); rlit++)
                    delete (*rlit);
                delete (*rit);
            }
    }
}

double IQPTree::doIQP() {

    if (verbose_mode >= VB_DEBUG)
        drawTree(cout, WT_BR_SCALE | WT_INT_NODE | WT_TAXON_ID | WT_NEWLINE);

    clock_t time_begin = clock();

    PhyloNodeVector del_leaves, adjacent_nodes;
    deleteLeaves(del_leaves, adjacent_nodes);
    reinsertLeaves(del_leaves, adjacent_nodes);

    clock_t time_end = clock();

    if (verbose_mode >= VB_MED) {
        cout << "IQP Time = " << (double) (time_end - time_begin) / CLOCKS_PER_SEC << endl;
    }

    // just to make sure IQP does it right
    setAlignment(aln);
    clearAllPartialLh();
    curScore = optimizeAllBranches(1);
    if (enable_parsimony)
    	cur_pars_score = computeParsimony();
    //curScore = computeLikelihood();

    if (verbose_mode >= VB_MAX) {
        cout << "IQP Likelihood = " << curScore << "  Parsimony = " << cur_pars_score << endl;
        //printTree(cout);
        //cout << endl;
    }

    return curScore;
}

void get2RandNumb(const int size, int &first, int &second) {
    // pick a random element
    first = floor((((double) rand()) / RAND_MAX) * size);
    // pick a random element from what's left (there is one fewer to choose from)...
    second = floor((((double) rand()) / RAND_MAX) * (size-1));
    // ...and adjust second choice to take into account the first choice
    if (second >= first) {
        ++second;
    }
}

double IQPTree::swapTaxa(PhyloNode *node1, PhyloNode *node2) {
    assert( node1->isLeaf() );
    assert( node2->isLeaf() );

    PhyloNeighbor *node1nei = (PhyloNeighbor*) *( node1->neighbors.begin() );
    PhyloNeighbor *node2nei = (PhyloNeighbor*) *( node2->neighbors.begin() );

    node2nei->node->updateNeighbor(node2, node1);
    node1nei->node->updateNeighbor(node1, node2);

    // Update the new neightbors of the 2 nodes
    node1->updateNeighbor(node1->neighbors.begin(), node2nei);
    node2->updateNeighbor(node2->neighbors.begin(), node1nei);

    PhyloNeighbor *node1NewNei = (PhyloNeighbor*) *( node1->neighbors.begin() );
    PhyloNeighbor *node2NewNei = (PhyloNeighbor*) *( node2->neighbors.begin() );

    // Reoptimize the branch lengths
    optimizeOneBranch(node1, (PhyloNode*) node1NewNei->node );
    this->curScore = optimizeOneBranch(node2, (PhyloNode*) node2NewNei->node );
    //drawTree(cout, WT_BR_SCALE | WT_INT_NODE | WT_TAXON_ID | WT_NEWLINE);
    return this->curScore;
}

double IQPTree::perturb(int times) {

    while (times > 0) {
        NodeVector taxa;
        // get the vector of taxa
        getTaxa(taxa);
        int taxonid1 = floor((((double) rand()) / RAND_MAX) * taxa.size());
        PhyloNode *taxon1 = (PhyloNode*) taxa[taxonid1];
        PhyloNode *taxon2;
        int dists[taxa.size()];
        int minDist = 1000000;
        for (int i = 0; i < taxa.size(); i++) {
            if (i == taxonid1)
                continue;
            taxon2 = (PhyloNode*) taxa[i];
            int dist = taxon1->calDist(taxon2);
            dists[i] = dist;
            if (dist >= 7 && dist < minDist)
                minDist = dist;
        }

        int taxonid2;
        for (int i = 0; i < taxa.size(); i++) {
            if (dists[i] == minDist)
                taxonid2 = i;
        }

        taxon2 = (PhyloNode*) taxa[taxonid2];

        cout << "Swapping node " << taxon1->id << " and node " << taxon2->id << endl;
        cout << "Distance " << minDist << endl;
        curScore = swapTaxa(taxon1, taxon2);
        //taxa.erase( taxa.begin() + taxaID1 );
        //taxa.erase( taxa.begin() + taxaID2 -1 );

        times--;
    }
    curScore = optimizeAllBranches(1);
    return curScore;
}

double IQPTree::doILS(Params &params, int perturbLevel) {

    string tree_file_name = params.aln_file;
    tree_file_name += ".treefile";
    // keep the best tree into a string
    stringstream best_tree_string;
    printTree(best_tree_string, WT_TAXON_ID + WT_BR_LEN);
    bestScore = curScore;

    int numIter = params.min_iterations;
    for (int i=1 ; i <= numIter; i++) {

        if (i > numheu) {
            enableHeuris = true;
            nbNNI95 = estimateNumNNI();
            deltaNNI95 = estimateDeltaNNI();
        }

        cout.precision(10);
        clock_t startClock = clock();
        perturb(perturbLevel);
        clock_t endClock = clock(); 
        cout << "Perturbing Time = " << (double) (endClock - startClock) / CLOCKS_PER_SEC << endl;

        startClock = clock();
        optimizeNNI(true);
        endClock = clock();
        cout << "NNI Time = " << (double) (endClock - startClock) / CLOCKS_PER_SEC << endl;

        cout.precision(15);
        cout << "Iteration " << i << " / Log-Likelihood: "
                << curScore << endl;
        if (curScore > bestScore + TOL_LIKELIHOOD) {
            //nni_score = optimizeNNI(true);
            //curScore = optimizeAllBranches();
            cout << "BETTER TREE FOUND: " << curScore << endl;
            bestScore = curScore;
            best_tree_string.seekp(0);
            printTree(best_tree_string, WT_TAXON_ID + WT_BR_LEN);
            printTree(tree_file_name.c_str());
        } else {
            /* take back the current best tree */
            best_tree_string.seekg(0);
            freeNode();
            readTree(best_tree_string, rooted);
            assignLeafNames();
            initializeAllPartialLh();
        }
    }

    return bestScore;
}
double IQPTree::doIQPNNI(Params &params) {
    string tree_file_name = params.out_prefix;
    tree_file_name += ".treefile";
    bestScore = curScore;

    printResultTree(params);

    string treels_name = params.out_prefix;
    treels_name += ".treels";
    if (params.write_intermediate_trees)
        printTree(treels_name.c_str(), WT_TAXON_ID | WT_SORT_TAXA | WT_NEWLINE);
    //printTree(treels_name.c_str(), WT_NEWLINE | WT_BR_LEN);


    // keep the best tree into a string
    stringstream best_tree_string;
    printTree(best_tree_string, WT_TAXON_ID + WT_BR_LEN);

    // write tree's loglikelihood to a file (if nni_lh option is enabled)
    ofstream lh_file;
    if (nni_lh) {
        // Remove the .treefile ending and add iq-tree.lh ending to the file name
        string aln_file_name;
        aln_file_name.assign(tree_file_name).erase(tree_file_name.size() - 9);
        string lh_file_name = aln_file_name + ".iq-tree.lh";

        lh_file.open((lh_file_name).c_str());
        if (lh_file.is_open()) {
            lh_file.precision(15);
            lh_file << 1;
            lh_file << "\t";
            lh_file << bestScore;
            lh_file << endl;
        } else {
            cout << "Cannot open file " + lh_file_name;
        }
    }

    stop_rule.addImprovedIteration(1);
    for (int cur_iteration = 2; !stop_rule.meetStopCondition(cur_iteration); cur_iteration++) {
        if (verbose_mode >= VB_DEBUG)
            cout << "Performing IQP in iteration " << cur_iteration << endl;

        nbIQPIter++;
        if (nbIQPIter > numheu) {

            if (nbIQPIter == (numheu + 1))
                cout << "NNI-HEURISTICS STARTED FROM NOW ON !" << endl;

            enableHeuris = true;
            nbNNI95 = estimateNumNNI();
            deltaNNI95 = estimateDeltaNNI();
            //			cout.precision(10);
            //			cout << "Improvment vector size = " << vecImpProNNI.size() << endl;
            //			cout << "Number NNI vector size = " << vecNbNNI.size() << endl;
            //			cout << "Max number of NNI :" << nbNNI95 << endl;
            //			cout << "Max improvment pro NNI :" << deltaNNI95 << endl;
            //			for (int i = 0; i < vecNbNNI.size(); i++)
            //				cout << vecNbNNI[i] << " ";
            //			cout << endl;
        }
        clock_t startClock = clock();
        double iqp_score = doIQP();
        clock_t endClock = clock();
        cout.precision(15);
        if (verbose_mode >= VB_MED) {
			cout << "IQP score : " << iqp_score << endl;
			printf("Total time used for IQP : %8.6f seconds. \n", (double) (-startClock + endClock) / CLOCKS_PER_SEC);
		}
        if (verbose_mode >= VB_DEBUG) {
            string iqp_tree = tree_file_name + "IQP" + convertIntToString(cur_iteration);
            printTree(iqp_tree.c_str());
        }

        //startClock = clock();        
        optimizeNNI(true); // the new score is saved in curScore
        //endClock = clock();
        //printf("Total time used for NNI : %8.6f seconds. \n", (double) (-startClock + endClock) / CLOCKS_PER_SEC);

        if (nni_lh && lh_file.is_open()) {
            lh_file << cur_iteration;
            lh_file << "\t";
            lh_file << iqp_score;
            lh_file << endl;

            lh_file << cur_iteration;
            lh_file << "\t";
            lh_file << curScore;
            lh_file << endl;
        }
        cout.precision(15);
        cout << "Iteration " << cur_iteration << " / Log-Likelihood: "
                << curScore << endl;

        //Tung : Write tree out to compare topology
        if (verbose_mode >= VB_DEBUG) {
            if (abs(curScore - bestScore) <= 0.0001) {
                cout << "Found tree with the same score as best score" << endl;
                if (!copyFile(tree_file_name.c_str(), (tree_file_name + ".bestTree" + convertIntToString(cur_iteration)).c_str()))
                    cout << "Tree file could not be copied successfully";
                printTree((tree_file_name + ".sameScoreBestTree" + convertIntToString(cur_iteration)).c_str());
                //exit(0);
            }
        }

        if (params.write_intermediate_trees)
            printTree(treels_name.c_str(), WT_TAXON_ID | WT_NEWLINE | WT_APPEND | WT_SORT_TAXA);
        //printTree(treels_name.c_str(), WT_NEWLINE | WT_APPEND | WT_BR_LEN);


        if (curScore > bestScore + TOL_LIKELIHOOD) {
            //nni_score = optimizeNNI(true);
            //curScore = optimizeAllBranches();
            cout << "BETTER TREE FOUND: " << curScore << endl;
            bestScore = curScore;
            best_tree_string.seekp(0);
            printTree(best_tree_string, WT_TAXON_ID + WT_BR_LEN);
            printResultTree(params);
            stop_rule.addImprovedIteration(cur_iteration);
        } else {
            /* take back the current best tree */
            best_tree_string.seekg(0);
            freeNode();
            readTree(best_tree_string, rooted);
            assignLeafNames();
            initializeAllPartialLh();
        }
    }

    /*
    best_tree_string.seekg(0);
    freeNode();
    readTree(best_tree_string, rooted);
    assignLeafNames();
    initializeAllPartialLh();
    bestScore = optimizeNNI(true);
     */

    return bestScore;
}

/****************************************************************************
 Fast Nearest Neighbor Interchange by maximum likelihood
 ****************************************************************************/

double IQPTree::optimizeNNI(bool fullNNI) {

    clock_t nniBeginClock, nniEndClock;
    nniBeginClock = clock();

    // Switch to run "slow NNI"
    if (simple_nni) {
        return PhyloTree::optimizeNNI();
    }

    int nniIteration = 0; // Number of NNI steps
    bool resetLamda = true;
    int numbNNI = 0; // Total number of NNIs applied in this iteration
    int nniTotal = 0; // Number of non-conflicting NNIs found in a NNI-step

    do {
        nbNNIToApply = 0;
        // Tree get improved, lamda reset
        if (resetLamda) {
            nniIteration++;
            //N IQPNNI iterations have been done
            if (enableHeuris) {
                double maxScore = curScore + deltaNNI95 * (nbNNI95 - numbNNI);
                if (maxScore <= bestScore + TOL_LIKELIHOOD) {
                    //cout << "TREE'S SCORE BEFORE STARTING NNI-STEP " << nniIteration << " = " << curScore << endl;
                    //cout << "ESTIMATED MAX-SCORE = " << maxScore << endl;
                    //cout << "BEST SCORE :" << bestScore << endl;
                    cout << "TREE IS NOT LIKELY TO BE IMPROVED, STOP DOING NNI-SEARCH !" << endl;
                    return curScore;
                }
            }
            lambda = cmdLambda;
            nonConflictMoves.clear();
            mapOptBranLens.clear();
            savedBranLens.clear();
            possibleNNIMoves.clear();

            //Save all the current branch lengths
            saveBranchLengths();

            clock_t evaNNIbegin, evaNNIend;
            evaNNIbegin = clock();
            //Generate all possible NNI moves
            genNNIMoves();
            evaNNIend = clock();

            if (verbose_mode >= VB_MED) {
                printf("Time used for evaluating NNIs: %8.6f seconds.\n", (double) (-evaNNIbegin
                        + evaNNIend) / CLOCKS_PER_SEC);
            }

            if (possibleNNIMoves.size() == 0) {
                if (verbose_mode >= VB_DEBUG) {
                    cout << "Could not find any improving NNIs for NNI Iteration "
                            << nniIteration << endl;
                    cout << "Stop optimizing using NNI" << endl;
                }
                break;
            }

            // Sort all the possible moves (this is descending)
            sort(possibleNNIMoves.begin(), possibleNNIMoves.end());

            // Remove conflicting NNIs
            for (vector<NNIMove>::iterator iterMove = possibleNNIMoves.begin(); iterMove
                    != possibleNNIMoves.end(); iterMove++) {
                bool choosen = true;
                for (vector<NNIMove>::iterator iterNextMove =
                        nonConflictMoves.begin(); iterNextMove
                        != nonConflictMoves.end(); iterNextMove++) {
                    if ((*iterMove).node1 == (*(iterNextMove)).node1
                            || (*iterMove).node2 == (*(iterNextMove)).node1
                            || (*iterMove).node1 == (*(iterNextMove)).node2
                            || (*iterMove).node2 == (*(iterNextMove)).node2) {
                        choosen = false;
                        break;
                    }
                }
                if (choosen) {
                    nonConflictMoves.push_back(*iterMove);
                }
            }

            nniTotal = nonConflictMoves.size();

        }// The tree's topology was reverted
        else {
            if (verbose_mode >= VB_DEBUG) {
                double tmpScore = computeLikelihood();
                cout << "Score after revert: " << tmpScore << endl;
                cout << "Score should be " << curScore << endl;
                // Make sure that the tree was reverted correctly
                assert(abs(tmpScore - curScore) < 0.1);
            }
        }

        nbNNIToApply = (int) nniTotal * lambda;

        if (verbose_mode == VB_DEBUG)
            cout << "lamda = " << lambda << endl;

        if (nbNNIToApply < 1)
            nbNNIToApply = 1;

        //Applying all non-conflicting NNIs
        for (int i = 0; i < nbNNIToApply; i++) {
            // Apply the calculated optimal branch length for the center branch
            applyBranchLengthChange(nonConflictMoves.at(i).node1, nonConflictMoves.at(i).node2, false);
            //double scoreBeforeNNI = computeLikelihood();
            doNNIMove(nonConflictMoves.at(i));
        }

        double newScore;

        //Do it like in PhyML for the branch lengths
        if (phyml_opt) {
            applyAllBranchLengthChanges((PhyloNode*) root);
            newScore = computeLikelihood();
        } else {
            //Do it like in IQPNNI: Optimize all branch lengths after each NNI-Iteration
            newScore = optimizeAllBranches(1);
        }

        if (newScore > curScore - TOL_LIKELIHOOD) {
            numbNNI += nbNNIToApply;
            double delta = newScore - curScore;
            double deltaProNNI = (newScore - curScore) / nbNNIToApply;
            curScore = newScore; // Update current score
            resetLamda = true;
            if (vecImpProNNI.size() >= 100) {
                vecImpProNNI.erase(vecImpProNNI.begin());
            }
            vecImpProNNI.push_back(deltaProNNI);
            if (verbose_mode >= VB_DEBUG)
                cout << "New best tree found with score " << newScore
                    << " with " << nbNNIToApply << " NNIs"
                    << " -- improvement general "
                    << delta
                    << " and improvement pro NNI " << deltaProNNI << endl;
			if (enable_parsimony) {
				cur_pars_score = computeParsimony();
				if (verbose_mode >= VB_MAX)
					cout << "Improved parsimony score: " << cur_pars_score << endl;
			}
        } else {
            cout << "Old score = " << curScore << endl;
            cout << "New score = " << newScore << endl;
            cout << "Using lamda = " << lambda << endl;
            cout << "Total non-conflicting NNIs found = " << nniTotal << endl;
            lambda = lambda / 2;
            cout << "The tree didn't improve at NNI step " << nniIteration
                    << " (applied NNIs = " << nbNNIToApply
                    << ") -> Trying new lamda = " << lambda << endl;
            assert((nbNNIToApply - 1) != 0); //Tree cannot be worse if only 1 NNI is applied

            //Restore the tree by reverting all NNIs
            for (int i = (nbNNIToApply - 1); i >= 0; i--) {
                doNNIMove(nonConflictMoves.at(i));
            }
            //Restore the branch lengths
            restoreBranchLengths();
            clearAllPartialLh();
            resetLamda = false;
        }
    } while (fullNNI);

    if (vecNumNNI.size() >= 100) {
        vecNumNNI.erase(vecNumNNI.begin());
    }
    vecNumNNI.push_back(numbNNI);

    //return curScore;
    this->curScore = optimizeAllBranches();
    nniEndClock = clock();
    if (verbose_mode >= VB_MED) {
        cout << "Number of NNIs applied : " << numbNNI << endl;
        printf("Time used : %8.6f seconds.\n", (double) (-nniBeginClock
                + nniEndClock) / CLOCKS_PER_SEC);
    }
    return curScore;
}

int IQPTree::estimateNumNNI() {
    if ( vecNumNNI.size() == 0 )
        return 0;
    else {
        vector<int> tmpVecNumNNI (vecNumNNI.size());
        copy(vecNumNNI.begin(), vecNumNNI.end(),tmpVecNumNNI.begin());
        sort(tmpVecNumNNI.begin(), tmpVecNumNNI.end());
        int index = floor ( tmpVecNumNNI.size() * 95 / 100 );
        return tmpVecNumNNI[index];
    }
}

double IQPTree::estimateDeltaNNI() {
    if ( vecImpProNNI.size() == 0 )
        return 0;
    else {
        vector<double> tmpVecImpProNNI (vecImpProNNI.size());
        copy(vecImpProNNI.begin(), vecImpProNNI.end(), tmpVecImpProNNI.begin());
        sort(tmpVecImpProNNI.begin(), tmpVecImpProNNI.end());
        int index = floor( tmpVecImpProNNI.size() * 95 / 100 );
        return tmpVecImpProNNI[index];
    }

}

void IQPTree::applyAllBranchLengthChanges(PhyloNode *node, PhyloNode *dad) {
    applyChildBranchChanges(node, dad);

    FOR_NEIGHBOR_IT(node, dad, it) {
        if (!(*it)->node->isLeaf()) {
            applyAllBranchLengthChanges((PhyloNode*) (*it)->node, node);
        }
    }
}

double IQPTree::applyBranchLengthChange(PhyloNode *node1, PhyloNode *node2,
        bool nonNNIBranch) {

    current_it = (PhyloNeighbor*) node1->findNeighbor(node2);
    assert(current_it);
    current_it_back = (PhyloNeighbor*) node2->findNeighbor(node1);
    assert(current_it_back);

    double current_len = current_it->length;
    string key("");
    if (node1->id < node2->id) {
        key += convertIntToString(node1->id) + "->" + convertIntToString(
                node2->id);
    } else {
        key += convertIntToString(node2->id) + "->" + convertIntToString(
                node1->id);
    }

    double optLen = mapOptBranLens[key];
    double new_len;

    if (nonNNIBranch) {
        new_len = current_len + lambda * (optLen - current_len);
    } else {
        new_len = optLen;
    }

    current_it->length = new_len;
    current_it_back->length = new_len;

    node1->clearReversePartialLh(node2);
    node2->clearReversePartialLh(node1);

    return new_len;

}

double IQPTree::getBranchLength(PhyloNode *node1, PhyloNode *node2) {
    current_it = (PhyloNeighbor*) node1->findNeighbor(node2);
    assert(current_it);
    return current_it->length;
}

void IQPTree::saveBranchLengths(PhyloNode *node, PhyloNode *dad) {
    if (!node) {
        node = (PhyloNode*) root;
    }
    if (dad) {
        double len = getBranchLength(node, dad);
        string key("");
        if (node->id < dad->id) {
            key += convertIntToString(node->id) + "->" + convertIntToString(
                    dad->id);
        } else {
            key += convertIntToString(dad->id) + "->" + convertIntToString(
                    node->id);
        }
        savedBranLens.insert(MapBranchLength::value_type(key, len));
    }

    FOR_NEIGHBOR_IT(node, dad, it) {
        saveBranchLengths((PhyloNode*) (*it)->node, node);
    }
}

void IQPTree::restoreBranchLengths(PhyloNode *node, PhyloNode *dad) {
    if (!node) {
        node = (PhyloNode*) root;
    }
    if (dad) {
        string key("");
        if (node->id < dad->id) {
            key += convertIntToString(node->id) + "->" + convertIntToString(
                    dad->id);
        } else {
            key += convertIntToString(dad->id) + "->" + convertIntToString(
                    node->id);
        }
        current_it = (PhyloNeighbor*) node->findNeighbor(dad);
        assert(current_it);
        current_it_back = (PhyloNeighbor*) dad->findNeighbor(node);
        assert(current_it_back);
        current_it->length = savedBranLens[key];
        current_it_back->length = savedBranLens[key];
    }

    FOR_NEIGHBOR_IT(node, dad, it) {
        restoreBranchLengths((PhyloNode*) (*it)->node, node);
    }
}

double IQPTree::calculateOptBranchLen(PhyloNode *node1, PhyloNode *node2) {

    double negative_lh, ferror;
    current_it = (PhyloNeighbor*) node1->findNeighbor(node2);
    assert(current_it);
    current_it_back = (PhyloNeighbor*) node2->findNeighbor(node1);
    assert(current_it_back);

    double current_len = current_it->length;
    double optLength;
    if (optimize_by_newton) // Newton-Raphson method
        optLength = minimizeNewton(MIN_BRANCH_LEN, current_len, MAX_BRANCH_LEN,
            TOL_BRANCH_LEN, negative_lh);
    else
        // Brent method
        optLength = minimizeOneDimen(MIN_BRANCH_LEN, current_len,
            MAX_BRANCH_LEN, TOL_BRANCH_LEN, &negative_lh, &ferror);

    return optLength;
}

double IQPTree::getCurScore(void) {
    return curScore;
}

void IQPTree::applyChildBranchChanges(PhyloNode *node, PhyloNode *dad) {

    FOR_NEIGHBOR_IT(node, dad, it) {
        bool branchUsed = false;
        for (int i = 0; i < nbNNIToApply; i++) {
            if ((node->id == nonConflictMoves.at(i).node1->id
                    && (*it)->node->id == nonConflictMoves.at(i).node2->id)
                    || (node->id == nonConflictMoves.at(i).node2->id
                    && (*it)->node->id
                    == nonConflictMoves.at(i).node1->id)) {
                branchUsed = true;
                break;
            }
        }

        if (branchUsed) {
            continue;
        }

        applyBranchLengthChange((PhyloNode*) node,
                (PhyloNode*) (*it)->node, true);
        cntBranches++;
    }

}

double IQPTree::doNNIMove(NNIMove move) {
    PhyloNode *node1 = move.node1;
    PhyloNode *node2 = move.node2;
    NeighborVec::iterator node1Nei_it = move.node1Nei_it;
    NeighborVec::iterator node2Nei_it = move.node2Nei_it;
    Neighbor *node1Nei = *(node1Nei_it);
    Neighbor *node2Nei = *(node2Nei_it);

    assert(node1->degree() == 3 && node2->degree() == 3);

    // do the NNI swap
    node1->updateNeighbor(node1Nei_it, node2Nei);
    node2Nei->node->updateNeighbor(node2, node1);

    node2->updateNeighbor(node2Nei_it, node1Nei);
    node1Nei->node->updateNeighbor(node1, node2);

    PhyloNeighbor *node12_it = (PhyloNeighbor*) node1->findNeighbor(node2); // return neighbor of node1 which points to node 2
    PhyloNeighbor *node21_it = (PhyloNeighbor*) node2->findNeighbor(node1); // return neighbor of node2 which points to node 1

    // clear partial likelihood vector
    node12_it->clearPartialLh();
    node21_it->clearPartialLh();

    node2->clearReversePartialLh(node1);
    node1->clearReversePartialLh(node2);

    //optimizeOneBranch(node1, node2);

    // Return likelihood score only for debugging, otherwise return 0
    //return computeLikelihood();
    return 0;
}

void IQPTree::genNNIMoves(PhyloNode *node, PhyloNode *dad) {

    if (!node) {
        node = (PhyloNode*) root;
    }
    //Internal Branch
    if (!node->isLeaf() && dad && !dad->isLeaf()) {
        NNIMove myMove = getBestNNIMoveForBranch(node, dad);
        if (myMove.score != 0) {
            addPossibleNNIMove(myMove);
        }
    }//External branch
    else if (dad) {
        double optBranchLen = calculateOptBranchLen(node, dad);
        string key("");
        if (node->id < dad->id) {
            key += convertIntToString(node->id) + "->" + convertIntToString(
                    dad->id);
        } else {
            key += convertIntToString(dad->id) + "->" + convertIntToString(
                    node->id);
        }
        mapOptBranLens.insert(MapBranchLength::value_type(key, optBranchLen));
    }

    FOR_NEIGHBOR_IT(node, dad, it) {
        genNNIMoves((PhyloNode*) (*it)->node, node);
    }

}

NNIMove IQPTree::getBestNNIMoveForBranch(PhyloNode *node1, PhyloNode *node2) {
    assert(node1->degree() == 3 && node2->degree() == 3);

    NNIMove mymove;
    mymove.score = 0;

    PhyloNeighbor *node12_it = (PhyloNeighbor*) node1->findNeighbor(node2);
    PhyloNeighbor *node21_it = (PhyloNeighbor*) node2->findNeighbor(node1);

    // Array to store the branch lengths : Before the swap, optimized,1.NNI swap, 2.NNI swap
    double node12_len[4];
    node12_len[0] = node12_it->length; // Length of branch node1-node2 before the swap

    // Calculate optimal branch length for branch node1-node2
    double bestScore = optimizeOneBranch(node1, node2);
    //double bestScore = curScore;

    // Optimal branch length of the current branch
    node12_len[1] = node12_it->length;

    // save the likelihood vector at the two ends of node1-node2
    double *node1_lh_save = node12_it->partial_lh;
    double *node2_lh_save = node21_it->partial_lh;
    //save scaling vector
    double node1_lh_scale = node12_it->lh_scale_factor;
    double node2_lh_scale = node21_it->lh_scale_factor;

    // save parsimony vector
    UINT *node1_pars_save = node12_it->partial_pars;
    UINT *node2_pars_save = node21_it->partial_pars;

    // save the first found neighbor of node 1 (excluding node2) in node1_it
    FOR_NEIGHBOR_DECLARE(node1, node2, node1_it)
    break;
    Neighbor *node1_nei = *node1_it;
    double node1_len = node1_nei->length;
    int nniNr = 1;
    int chosenSwap = 1;
    // replace partial_lh with a new vector
    node12_it->partial_lh = newPartialLh();
    node21_it->partial_lh = newPartialLh();

    // replace partial_pars with a new vector
    node12_it->partial_pars = newBitsBlock();
    node21_it->partial_pars = newBitsBlock();

    FOR_NEIGHBOR_IT(node2, node1, node2_it) {
        nniNr = nniNr + 1;
        /* do the NNI swap */
        Neighbor *node2_nei = *node2_it;
        double node2_len = node2_nei->length;
        node1->updateNeighbor(node1_it, node2_nei);
        node2_nei->node->updateNeighbor(node2, node1);

        node2->updateNeighbor(node2_it, node1_nei);
        node1_nei->node->updateNeighbor(node1, node2);

        // clear partial likelihood vector
        node12_it->clearPartialLh();
        node21_it->clearPartialLh();

        double newScore = NULL;
        double lh_prediction = 100.0;
        // compute score with parsimony, accept topology if parsimony score is not so bad
        int pars_score = -10;
        if (enable_parsimony) {
            pars_score = computeParsimonyBranch(node12_it, node1);
//            if (linRegModel != NULL)
//                lh_prediction = linRegModel->getValue(pars_score);
//            else {
//                for (int i = 0; i < 3000; i++) {
//                    if (pars_scores[i] == 0) {
//                        pars_scores[i] = pars_score;
//                        newScore = optimizeOneBranch(node1, node2, false);
//                        lh_scores[i] = newScore;
//                        break;
//                    }
//                }
//                if (pars_scores[2999] != 0) {
//                    linRegModel = new Linear(3000, pars_scores, lh_scores);
//                }
//            }
            // If enough data points is collected, start linear regression

        }
        //if (lh_prediction > bestScore || pars_score < cur_pars_score)
        if (pars_score < cur_pars_score) {
            // compute the score of the swapped topology
            if (!newScore)
                newScore = optimizeOneBranch(node1, node2, false);
            node12_len[nniNr] = node12_it->length;
            // If score is better, save the NNI move
            if (newScore > bestScore + TOL_LIKELIHOOD) {
                bestScore = newScore;
                chosenSwap = nniNr;
                mymove.node1Nei_it = node1_it;
                mymove.node2Nei_it = node2_it;
                mymove.score = bestScore;
                mymove.node1 = node1;
                mymove.node2 = node2;
            }
        } else {
            //cout << "pars filtered" << endl;
        }

        // swap back and recover the branch lengths
        node1->updateNeighbor(node1_it, node1_nei, node1_len);
        node1_nei->node->updateNeighbor(node2, node1, node1_len);
        node2->updateNeighbor(node2_it, node2_nei, node2_len);
        node2_nei->node->updateNeighbor(node1, node2, node2_len);
        node12_it->length = node12_len[0];
        node21_it->length = node12_len[0];

    }

    delete[] node21_it->partial_pars;
    delete[] node12_it->partial_pars;

    delete[] node21_it->partial_lh;
    delete[] node12_it->partial_lh;
    // restore the partial likelihood vector
    node12_it->partial_pars = node1_pars_save;
    node21_it->partial_pars = node2_pars_save;
    node12_it->partial_lh = node1_lh_save;
    node21_it->partial_lh = node2_lh_save;
    node12_it->lh_scale_factor = node1_lh_scale;
    node21_it->lh_scale_factor = node2_lh_scale;


    string key("");
    if (node1->id < node2->id) {
        key += convertIntToString(node1->id) + "->" + convertIntToString(
                node2->id);
    } else {
        key += convertIntToString(node2->id) + "->" + convertIntToString(
                node1->id);
    }

    mapOptBranLens.insert(MapBranchLength::value_type(key,
            node12_len[chosenSwap]));

    return mymove;
}

void IQPTree::addPossibleNNIMove(NNIMove myMove) {
    possibleNNIMoves.push_back(myMove);
}

void IQPTree::setRootNode(char *my_root) {
    string root_name;
    if (my_root) root_name = my_root;
    else root_name = aln->getSeqName(0);
    root = findNodeName(root_name);
    assert(root);
}

void IQPTree::printResultTree(Params &params) {
    setRootNode(params.root);
    string tree_file_name = params.out_prefix;
    tree_file_name += ".treefile";
    printTree(tree_file_name.c_str(), WT_BR_LEN | WT_BR_LEN_FIXED_WIDTH | WT_SORT_TAXA);
}

void IQPTree::printResultTree(Params &params, ostream &out) {
    setRootNode(params.root);
    printTree(out, WT_BR_LEN | WT_BR_LEN_FIXED_WIDTH | WT_SORT_TAXA);
}
