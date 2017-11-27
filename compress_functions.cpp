#include "compress_functions.h"

/* set to 1 for printing splits */
#define PRINT_SPLITS 0

#define PRINT_COMPRESSION 0

/* set to 1 for printing created structures */
#define PRINT_COMPRESSION_STRUCTURES 0

/* static functions */
static void fatal (const char * format, ...);

void simple_compression(char * tree_file) {
  /* tree properties */
  pll_utree_t * tree = NULL;
  unsigned int tip_count;

  /* parse the input tree */
  tree = pll_utree_parse_newick (tree_file);
  tip_count = tree->tip_count;

  // search for the root of the tree
  pll_unode_t * root = searchRoot(tree);

  // set and order the tree
  setTree(root);
  orderTree(root);

  // succinct_structure stores the topology in balanced parantheses ("0=(, 1=)")
  sdsl::bit_vector succinct_structure(4 * tip_count - 2, 0);
  // succinct_structure stores the permutation of the taxa
  sdsl::int_vector<> node_permutation(tip_count, 0, 32);
  // branch_lengths stores all branch lengths of the tree
  std::vector<double> branch_lengths(2 * tip_count - 2);
  // create mapping from node_id to branch_id
  unsigned int* node_id_to_branch_id = (unsigned int*) malloc ((tree->inner_count * 3 + tree->tip_count) * sizeof(unsigned int));
  // fill the created structures with the given tree
  assignBranchNumbers(root, succinct_structure, node_permutation, branch_lengths, node_id_to_branch_id);

  #if(PRINT_COMPRESSION_STRUCTURES)
  {
    std::cout << "Succinct representation: " << succinct_structure << "\n";
    std::cout << "\tuncompressed size: " << sdsl::size_in_bytes(succinct_structure) << " bytes\n";
    // TODO: compressable?
    sdsl::util::bit_compress(succinct_structure);
    std::cout << "\tcompressed size: " << sdsl::size_in_bytes(succinct_structure) << " bytes\n";

    std::cout << "Node permutation: " << node_permutation << "\n";
    std::cout << "\tuncompressed size: " << sdsl::size_in_bytes(node_permutation) << " bytes\n";
    // TODO: other compression method (nodes are not ordered)?
    sdsl::util::bit_compress(node_permutation);
    std::cout << "\tcompressed size: " << sdsl::size_in_bytes(node_permutation) << " bytes\n";

    std::cout << "Branch Lengths: ";
    for(auto x: branch_lengths) {
      std::cout << x << " ";
    }
  }
  #endif

  // write stuctures to file
  store_to_file(succinct_structure, "output_files/succinct_tree.sdsl");
  store_to_file(node_permutation, "output_files/node_permutation.sdsl");
  saveArray(&branch_lengths[0], branch_lengths.size(), "output_files/branch_lengths.txt");

  #if(PRINT_COMPRESSION)
  {
    std::cout << "\n\nSimple compression compressed size (without branches): " << sdsl::size_in_bytes(succinct_structure)
              << " (topology) + " << sdsl::size_in_bytes(node_permutation) << " (leaves) = "
              << sdsl::size_in_bytes(succinct_structure) + sdsl::size_in_bytes(node_permutation)
              << " bytes\n";

    std::cout << "Branch lengths: " << branch_lengths.size() << " (#branches) * 8 byte = " << branch_lengths.size() * 8 << "\n";

    std::cout << "Simple compression compressed size (with branches): " << sdsl::size_in_bytes(succinct_structure)
              << " (topology) + " << sdsl::size_in_bytes(node_permutation) << " (leaves) + " << branch_lengths.size() * 8
              << " (branches) = " << sdsl::size_in_bytes(succinct_structure) + sdsl::size_in_bytes(node_permutation) + branch_lengths.size() * 8
              << " bytes\n";

    std::cout << "---------------------------------------------------------\n";
  }
  #endif

  free(node_id_to_branch_id);
}

std::tuple<std::vector<int>, std::vector<int>> findRFSubtreesRec(pll_unode_t * tree, const std::vector<bool> &edgeIncidentPresent2, std::queue<pll_unode_t *> &tasks) {
  assert(tree != NULL);
  std::vector<int> return_vector_topology;
  std::vector<int> return_vector_order;
  if(tree->next == NULL) {
    // leaf; edge incident is always present in both trees
    //std::cout << "Leaf: " << tree->node_index << "\t    Edge incident present: " << edgeIncidentPresent2[tree->node_index] << "\n";
  } else {
    // inner node
    assert(tree->next != NULL);
    assert(tree->next->next != NULL);
    assert(tree->next->back != NULL);
    assert(tree->next->next->back != NULL);

    //std::cout << "Inner node: " << tree->node_index << "\t    Edge incident present: " << edgeIncidentPresent2[tree->node_index] << "\n";

    //std::cout << "1. Inner node: " << tree->next->node_index <<
      // "\t    Edge incident present: " << edgeIncidentPresent2[tree->next->node_index] << "\n";
    if(edgeIncidentPresent2[tree->next->node_index]) {
          return_vector_topology.push_back(0);
          std::vector<int> temp_topology;
          std::vector<int> temp_order;
          std::tie (temp_topology, temp_order) = findRFSubtreesRec(tree->next->back, edgeIncidentPresent2, tasks);
          return_vector_topology.insert(return_vector_topology.end(), temp_topology.begin(), temp_topology.end());
          return_vector_order.insert(return_vector_order.end(), temp_order.begin(), temp_order.end());
          return_vector_topology.push_back(1);
    } else {
          return_vector_order.push_back((intptr_t) tree->next->back->data);
          tasks.push(tree->next->back);
    }
    //std::cout << "2. Inner node: " << tree->next->next->node_index <<
      // "\t    Edge incident present: " << edgeIncidentPresent2[tree->next->next->node_index] << "\n";
    if(edgeIncidentPresent2[tree->next->next->node_index]) {
          return_vector_topology.push_back(0);
          std::vector<int> temp_topology;
          std::vector<int> temp_order;
          std::tie (temp_topology, temp_order) = findRFSubtreesRec(tree->next->next->back, edgeIncidentPresent2, tasks);
          return_vector_topology.insert(return_vector_topology.end(), temp_topology.begin(), temp_topology.end());
          return_vector_order.insert(return_vector_order.end(), temp_order.begin(), temp_order.end());
          return_vector_topology.push_back(1);
    } else {
          return_vector_order.push_back((intptr_t) tree->next->next->back->data);
          tasks.push(tree->next->next->back);
    }
  }
  return std::make_tuple(return_vector_topology, return_vector_order);
}

std::tuple<std::vector<int>, std::vector<int>> findRFSubtrees(std::queue<pll_unode_t *> &tasks, const std::vector<bool> &edgeIncidentPresent2) {
  std::vector<int> return_topology;
  std::vector<int> return_order;
  if(tasks.empty()) {
    return std::make_tuple(return_topology, return_order);
  }
  pll_unode_t * tree = tasks.front();
  tasks.pop();

  if(tree == NULL) {
    return std::make_tuple(return_topology, return_order);
  } else {
      return findRFSubtreesRec(tree, edgeIncidentPresent2, tasks);
  }
}

void free_leaf(pll_unode_t * leaf) {
    if (leaf->label)
      free(leaf->label);
    free(leaf);
}

void free_inner_node(pll_unode_t * node) {
  if (node->label)
    free(node->label);
  free(node);
}

void rf_distance_compression(char * tree1_file, char * tree2_file) {
  /* tree properties */
  pll_utree_t * tree1 = NULL,
              * tree2 = NULL;
  unsigned int tip_count;

  /* parse the input trees */
  tree1 = pll_utree_parse_newick (tree1_file);
  tree2 = pll_utree_parse_newick (tree2_file);
  tip_count = tree1->tip_count;

  if (tip_count != tree2->tip_count)
    fatal("Trees have different number of tips!");

  if (!pllmod_utree_consistency_set(tree1, tree2))
    fatal("Cannot set trees consistent!");

  if (!pllmod_utree_consistency_check(tree1, tree2))
    fatal("Tip node IDs are not consistent!");

  // search for the root of the trees
  pll_unode_t * root1 = searchRoot(tree1);
  pll_unode_t * root2 = searchRoot(tree2);

  // set and order both trees
  setTree(root1);
  setTree(root2);
  orderTree(root1);
  orderTree(root2);

  // succinct_structure stores the topology in balanced parantheses ("0=(, 1=)")
  sdsl::bit_vector succinct_structure1(4 * tip_count - 2, 0);
  // succinct_structure stores the permutation of the taxa
  sdsl::int_vector<> node_permutation1(tip_count, 0, 32);
  // branch_lengths stores all branch lengths of the tree
  // TODO: lots of potential here!
  std::vector<double> branch_lengths1(2 * tip_count - 2);
  // create mapping from node_id to branch_id
  unsigned int* node_id_to_branch_id1 = (unsigned int*) malloc ((tree1->inner_count * 3 + tree1->tip_count) * sizeof(unsigned int));
  // fill the created structures with the given tree
  assignBranchNumbers(root1, succinct_structure1, node_permutation1, branch_lengths1, node_id_to_branch_id1);

  sdsl::bit_vector succinct_structure2(4 * tip_count - 2, 0);
  sdsl::int_vector<> node_permutation2(tip_count, 0, 32);
  std::vector<double> branch_lengths2(2 * tip_count - 2);
  unsigned int* node_id_to_branch_id2 = (unsigned int*) malloc ((tree2->inner_count * 3 + tree2->tip_count) * sizeof(unsigned int));
  assignBranchNumbers(root2, succinct_structure2, node_permutation2, branch_lengths2, node_id_to_branch_id2);

  #if(PRINT_COMPRESSION_STRUCTURES)
  {
    std::cout << "Succinct representation tree 1: " << succinct_structure1 << "\n";
    std::cout << "Succinct representation tree 2: " << succinct_structure2 << "\n";
  }
  #endif

  /* 1. creating the split sets manually */
  unsigned int n_splits = tip_count - 3;
  pll_unode_t ** splits_to_node1 = (pll_unode_t **) malloc(n_splits * sizeof(pll_unode_t *));
  pll_split_t * splits1 = pllmod_utree_split_create(tree1->nodes[tip_count],
                                                    tip_count,
                                                    splits_to_node1);

  #if(PRINT_SPLITS)
    {
      unsigned int i;
      for (i=0; i<n_splits; ++i)
      {
        pllmod_utree_split_show(splits1[i], tip_count);
        printf("\n");
      }
      printf("\n");
    }
  #endif

  /* compute the splits, but also the nodes corresponding to each split */
  pll_unode_t ** splits_to_node2 = (pll_unode_t **) malloc(n_splits * sizeof(pll_unode_t *));
  pll_split_t * splits2 = pllmod_utree_split_create(tree2->nodes[tip_count],
                                                    tip_count,
                                                    splits_to_node2);


  #if(PRINT_SPLITS)
    {
      unsigned int i;
      for (i=0; i<n_splits; ++i)
      {
        pllmod_utree_split_show(splits2[i], tip_count);
        printf(" node: Pmatrix:%d Nodes:%d<->%d Length:%lf\n",
                                  splits_to_node2[i]->pmatrix_index,
                                  splits_to_node2[i]->node_index,
                                  splits_to_node2[i]->back->node_index,
                                  splits_to_node2[i]->length);
      }
    }
  #endif

  // create arrays indicating whether a split is common in both trees or not
  int * s1_present = (int*) calloc(n_splits, sizeof(int));
  int * s2_present = (int*) calloc(n_splits, sizeof(int));

  // fill the arrays s1_present and s2_present
  int rf_distance = pllmod_utree_split_rf_distance_extended(splits1, splits2, s1_present, s2_present, tip_count);

  // TODO: just for test
  pllmod_utree_split_rf_distance_extended_with_branches(splits1, splits2, splits_to_node1, splits_to_node2, s1_present, s2_present, tip_count);


  #if(PRINT_COMPRESSION_STRUCTURES)
  {
    std::cout << "RF-distance: " << rf_distance << "\n";
  }
  #endif

  assert(rf_distance % 2 == 0);
  // create array containing all edges to contract in tree1
  sdsl::int_vector<> edges_to_contract(rf_distance / 2, 0);
  size_t idx = 0;

  for (size_t i = 0; i < n_splits; i++) {
      if(s1_present[i] == 0) {
          // contract edge in tree 1 to get to the conseneus tree
          contractEdge(splits_to_node1[i]);
          edges_to_contract[idx] = node_id_to_branch_id1[splits_to_node1[i]->node_index];
          idx++;
      }
  }

  // sort all edges to contract
  sort(edges_to_contract.begin(), edges_to_contract.end());

  #if(PRINT_COMPRESSION_STRUCTURES)
  {
    std::cout << "Edges to contract in tree 1: " << edges_to_contract << "\n";
    std::cout << "\tuncompressed size: " << sdsl::size_in_bytes(edges_to_contract) << " bytes\n";
    sdsl::util::bit_compress(edges_to_contract);
    // TODO: elias-fano encoding for the ordered edges?
    std::cout << "\tcompressed size: " << sdsl::size_in_bytes(edges_to_contract) << " bytes\n";
    printf("\n\n");
  }
  #endif

  // succinct_structure stores the topology fo the consensus tree in balanced parantheses ("0=(, 1=)")
  sdsl::bit_vector consensus_succinct_structure(4 * tip_count - rf_distance - 2, 0);
  sdsl::int_vector<> consensus_node_permutation(tip_count, 0, 32);
  std::vector<double> consensus_branch_lengths(2 * tip_count - 2 - (rf_distance / 2));
  unsigned int* consensus_node_id_to_branch_id = (unsigned int*) malloc ((tree1->inner_count * 3 + tree1->tip_count) * sizeof(unsigned int));
  assignBranchNumbers(root1, consensus_succinct_structure, consensus_node_permutation, consensus_branch_lengths, consensus_node_id_to_branch_id);

  #if(PRINT_COMPRESSION_STRUCTURES)
  {
    std::cout << "Consensus tree after edge contraction: " << consensus_succinct_structure << "\n";
  }
  #endif

  // create an array that indicates whether the edge incident to the node_id in tree 2 is common in both trees or not
  std::vector<bool> edgeIncidentPresent2 (tree2->inner_count * 3 + tree2->tip_count);
  for (unsigned int i=0; i<n_splits; ++i)
  {

    if(s2_present[i] == 0) {
      // edge is not common in both trees
      edgeIncidentPresent2[splits_to_node2[i]->node_index] = true;
      edgeIncidentPresent2[splits_to_node2[i]->back->node_index] = true;
    } else {
      // edge is common in both trees
      edgeIncidentPresent2[splits_to_node2[i]->node_index] = false;
      edgeIncidentPresent2[splits_to_node2[i]->back->node_index] = false;
    }
  }

  std::queue<pll_unode_t *> tasks;
  tasks.push(root2->back);

  std::vector<std::vector<int>> subtrees;
  std::vector<std::vector<int>> permutations;

  // recursively find all subtrees that need to be inserted into the consensus tree
  while(!tasks.empty()) {
      std::vector<int> subtree;
      std::vector<int> leaf_order;
      std::tie(subtree, leaf_order) = findRFSubtrees(tasks, edgeIncidentPresent2);
      if(!subtree.empty()) {
          subtrees.push_back(subtree);
          permutations.push_back(leaf_order);
      }
  }

  if(!subtrees.empty()) {
    #if(PRINT_COMPRESSION_STRUCTURES)
    {
      std::cout << "\nSubtrees: \n";
    }
    #endif
    int subtree_elements = subtrees.size() - 1;
    for (std::vector<int> i: subtrees) {
      subtree_elements += i.size();
      #if(PRINT_COMPRESSION_STRUCTURES)
      {
        for (auto j : i) {
          std::cout << j;
        }
        std::cout << "\n";
      }
      #endif
    }

    size_t subtrees_index = 0;
    // subtrees_succinct stores all subtrees to insert into the consensus tree, split by a 1
    sdsl::bit_vector subtrees_succinct(subtree_elements, 1);
    for (std::vector<int> i: subtrees) {
      for (auto j : i) {
        subtrees_succinct[subtrees_index] = j;
        subtrees_index++;
      }
      subtrees_index++;
    }
    assert(subtrees_index = subtrees_succinct.size());

    #if(PRINT_COMPRESSION_STRUCTURES)
    {
      std::cout << "\nSuccinct subtree representation: " << subtrees_succinct << "\n";
      std::cout << "\tuncompressed size: " << sdsl::size_in_bytes(subtrees_succinct) << " bytes\n";
      sdsl::util::bit_compress(subtrees_succinct);
      // TODO: compression possible?
      std::cout << "\tcompressed size: " << sdsl::size_in_bytes(subtrees_succinct) << " bytes\n";

      std::cout << "\nPermutations: \n";
      for(std::vector<int> i: permutations) {
        for (auto j : i) {
          std::cout << j << " ";
        }
        std::cout << "\n";
      }
    }
    #endif


    // map permutations to 1,2,3,4,...
    int permutation_elements = permutations.size() - 1;
    for (std::vector<int> &perm: permutations) {
      permutation_elements += perm.size();

      int temp_index, temp_min;
      int current_set_min = 0;
      while (current_set_min < perm.size()) {
        int temp_index = -1;
        int temp_min = INT_MAX;
        for (size_t j = 0; j < perm.size(); j++) {
          if(perm[j] < temp_min && perm[j] > current_set_min) {
            temp_min = perm[j];
            temp_index = j;
          }
        }
        current_set_min++;
        perm[temp_index] = current_set_min;
      }

    }

    size_t permutation_index = 0;
    // succinct_permutations stores all permutations according to the subtrees, split by a "0"
    sdsl::int_vector<> succinct_permutations(permutation_elements, 0);
    for (std::vector<int> i: permutations) {
      for (auto j : i) {
        succinct_permutations[permutation_index] = j;
        permutation_index++;
      }
      permutation_index++;
    }
    assert(permutation_index = succinct_permutations.size());

    #if(PRINT_COMPRESSION_STRUCTURES)
    {
      std::cout << "\nSuccinct permutation representation: " << succinct_permutations << "\n";
      std::cout << "\tuncompressed size: " << sdsl::size_in_bytes(succinct_permutations) << " bytes\n";
      sdsl::util::bit_compress(succinct_permutations);
      // TODO: better compression possile?
      std::cout << "\tcompressed size: " << sdsl::size_in_bytes(succinct_permutations) << " bytes\n";
    }
    #endif

    #if(PRINT_COMPRESSION)
    {
      std::cout << "\n\nRF compression compressed size (without branches): " << sdsl::size_in_bytes(edges_to_contract)
      << " (edges to contract) + " << sdsl::size_in_bytes(subtrees_succinct) << " (subtrees) + "
      << sdsl::size_in_bytes(succinct_permutations) << " (permutations) = "
      << sdsl::size_in_bytes(edges_to_contract) + sdsl::size_in_bytes(subtrees_succinct) + sdsl::size_in_bytes(succinct_permutations)
      << " bytes\n";

      std::cout << "Branch lengths: " << branch_lengths1.size() << " (#branches) * 8 byte = " << branch_lengths1.size() * 8 << "\n";

      std::cout << "RF compression compressed size (with branches): " << sdsl::size_in_bytes(edges_to_contract)
      << " (edges to contract) + " << sdsl::size_in_bytes(subtrees_succinct) << " (subtrees) + "
      << sdsl::size_in_bytes(succinct_permutations) << " (permutations) + " << branch_lengths1.size() * 8 << " (branches) = "
      << sdsl::size_in_bytes(edges_to_contract) + sdsl::size_in_bytes(subtrees_succinct) + sdsl::size_in_bytes(succinct_permutations) + branch_lengths1.size() * 8
      << " bytes\n";

      std::cout << "---------------------------------------------------------\n";
    }
    #endif
  }

  // TODO: free procs segmentation fault

  //printf("RF [manual]\n");
  //printf("distance = %d\n", rf_dist);
  //printf("relative = %.2f%%\n", 100.0*rf_dist/(2*(tip_count-3)));

  //printf("Amount of branchs with same lengths = %d\n", same_branchs);

  pllmod_utree_split_destroy(splits1);
  pllmod_utree_split_destroy(splits2);
  free(splits_to_node1);
  free(splits_to_node2);

  /* clean */
  //pll_utree_destroy_consensus (tree1);

  traverseTree(root1, free_leaf, free_inner_node);
  free(tree1->nodes);
  free(tree1);
  pll_utree_destroy (tree2, NULL);

  free(node_id_to_branch_id1);
  free(node_id_to_branch_id2);
  free(consensus_node_id_to_branch_id);
  free(s1_present);
  free(s2_present);

}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

static void fatal (const char * format, ...)
{
  va_list argptr;
  va_start(argptr, format);
  vfprintf (stderr, format, argptr);
  va_end(argptr);
  fprintf (stderr, "\n");
  exit (EXIT_FAILURE);
}
