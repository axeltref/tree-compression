#include <assert.h>

#include "util.h"


void printNode(pll_unode_t * node) {
  assert(node != NULL);
  printf("Index: %i\t\tLabel: %s\t\tLength: %f\n", node->node_index, node->label, node->length);
}

pll_unode_t * searchRoot(pll_utree_t * tree) {

  for (size_t i = 0; i < tree->tip_count; i++) {
    if(strcmp(tree->nodes[i]->label, "1") == 0) {
      //printf("\nROOT: ");
      //printNode(tree->nodes[i]);
      return tree->nodes[i];
    }
  }
  return NULL;
}

void printTreeRec(pll_unode_t * tree) {
  assert(tree != NULL);
  if(tree->next == NULL) {
    // leaf
    printf("Node: ");
    printNode(tree);
  } else {
    // inner node
    assert(tree->next != NULL);
    assert(tree->next->next != NULL);
    assert(tree->next->back != NULL);
    assert(tree->next->next->back != NULL);
    printNode(tree);
    printNode(tree->next);
    printNode(tree->next->next);
    printTreeRec(tree->next->back);
    printTreeRec(tree->next->next->back);
  }
}

void printTree(pll_unode_t * tree) {
  assert(tree->next == NULL);

  printNode(tree);
  printTreeRec(tree->back);

}

int setTreeRec(pll_unode_t * tree) {
  assert(tree != NULL);
  if(tree->next == NULL) {
    // leaf
    int n = atoi(tree->label);
    tree->data = (void*)(intptr_t) n;
    tree->back->data = (void*)(intptr_t) n;
    return n;
  } else {
    // inner node
    assert(tree->next != NULL);
    assert(tree->next->next != NULL);
    assert(tree->next->back != NULL);
    assert(tree->next->next->back != NULL);
    int n1 = setTreeRec(tree->next->back);
    int n2 = setTreeRec(tree->next->next->back);
    int n;
    n1 < n2 ? (n = n1) : (n = n2);
    tree->data = (void*)(intptr_t) n;
    tree->back->data = (void*)(intptr_t) n;
    return n;
  }
}

void setTree(pll_unode_t * tree) {
  assert(tree->next == NULL);
  setTreeRec(tree->back);
  //printf("\n\nSmallest: %i\n\n", setTreeRec(tree->back));

}

void assignBranchNumbersRec(pll_unode_t * tree, unsigned int * bp_idx, sdsl::bit_vector &bp,
              unsigned int * iv_idx, sdsl::int_vector<> &iv, unsigned int * bl_idx, std::vector<double> &branch_lengths,
              unsigned int * n, unsigned int* node_id_to_branch_id) {
  assert(tree != NULL);
  branch_lengths[*bl_idx] = tree->length;
  (*bl_idx)++;
  if(tree->next == NULL) {
    // leaf
    //printf("%i\t", *n);
    node_id_to_branch_id[tree->node_index] = *n;
    node_id_to_branch_id[tree->back->node_index] = *n;
    (*n)++;
    iv[*iv_idx] = atoi(tree->label);
    (*iv_idx)++;
    //printf("%s, ", tree->label);
    //printf("Node: ");
    //printNode(tree);
  } else {
    // inner node
    assert(tree->next != NULL);
    assert(tree->next->next != NULL);
    assert(tree->next->back != NULL);
    assert(tree->next->next->back != NULL);
    //printf("%i\t", *n);
    node_id_to_branch_id[tree->node_index] = *n;
    node_id_to_branch_id[tree->back->node_index] = *n;
    (*n)++;
    //printNode(tree);
    int n1 = (intptr_t) tree->next->data;
    int n2 = (intptr_t) tree->next->next->data;
    if(n1 < n2) {
      //printNode(tree->next);
      bp[*bp_idx] = 0;
      (*bp_idx)++;
      assignBranchNumbersRec(tree->next->back, bp_idx, bp, iv_idx, iv, bl_idx, branch_lengths, n, node_id_to_branch_id);
      bp[*bp_idx] = 1;
      (*bp_idx)++;
      //printNode(tree->next->next);
      bp[*bp_idx] = 0;
      (*bp_idx)++;
      assignBranchNumbersRec(tree->next->next->back, bp_idx, bp, iv_idx, iv, bl_idx, branch_lengths, n, node_id_to_branch_id);
      bp[*bp_idx] = 1;
      (*bp_idx)++;
    } else {
      //printNode(tree->next->next);
      bp[*bp_idx] = 0;
      (*bp_idx)++;
      assignBranchNumbersRec(tree->next->next->back, bp_idx, bp, iv_idx, iv, bl_idx, branch_lengths, n, node_id_to_branch_id);
      bp[*bp_idx] = 1;
      (*bp_idx)++;
      //printNode(tree->next);
      bp[*bp_idx] = 0;
      (*bp_idx)++;
      assignBranchNumbersRec(tree->next->back, bp_idx, bp, iv_idx, iv, bl_idx, branch_lengths, n, node_id_to_branch_id);
      bp[*bp_idx] = 1;
      (*bp_idx)++;
    }

  }
}

void assignBranchNumbers(pll_unode_t * tree, sdsl::bit_vector &bp, sdsl::int_vector<> &iv, std::vector<double> &branch_lengths, unsigned int* node_id_to_branch_id) {
  assert(tree->next == NULL);
  assert(atoi(tree->label) == 1);
  bp[0] = 0;
  bp[1] = 0;
  bp[2] = 1;
  iv[0] = 1; // first node is always the root
  //printNode(tree);
  unsigned int n = 2;
  bp[3] = 0;
  unsigned int bp_idx = 4;
  unsigned int iv_idx = 1;
  unsigned int bl_idx = 1;
  assignBranchNumbersRec(tree->back, &bp_idx, bp, &iv_idx, iv, &bl_idx, branch_lengths, &n, node_id_to_branch_id);
  bp[bp_idx] = 1;
  bp_idx++;
  bp[bp_idx] = 1;
  bp_idx++;
  assert(bp_idx == bp.size());
  assert(iv_idx == iv.size());
}