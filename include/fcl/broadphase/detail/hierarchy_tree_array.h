/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011-2014, Willow Garage, Inc.
 *  Copyright (c) 2014-2016, Open Source Robotics Foundation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Open Source Robotics Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** @author Jia Pan  */

#ifndef FCL_HIERARCHY_TREE_ARRAY_H
#define FCL_HIERARCHY_TREE_ARRAY_H

#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include "fcl/common/warning.h"
#include "fcl/math/bv/AABB.h"
#include "fcl/broadphase/detail/morton.h"
#include "fcl/broadphase/detail/node_base_array.h"

namespace fcl
{

namespace detail
{

namespace implementation_array
{

/// @brief Class for hierarchy tree structure
template<typename BV>
class HierarchyTree
{
  using S = typename BV::S;
  typedef NodeBase<BV> NodeType;
  
  struct SortByMorton
  {
    bool operator() (size_t a, size_t b) const
    {
      if((a != NULL_NODE) && (b != NULL_NODE))
        return nodes[a].code < nodes[b].code;
      else if(a == NULL_NODE)
        return split < nodes[b].code;
      else if(b == NULL_NODE)
        return nodes[a].code < split;

      return false;
    }

    NodeType* nodes;
    uint32 split;
  };

public:
  /// @brief Create hierarchy tree with suitable setting.
  /// bu_threshold decides the height of tree node to start bottom-up construction / optimization;
  /// topdown_level decides different methods to construct tree in topdown manner.
  /// lower level method constructs tree with better quality but is slower.
  HierarchyTree(int bu_threshold_ = 16, int topdown_level_ = 0);

  ~HierarchyTree();

  /// @brief Initialize the tree by a set of leaves using algorithm with a given level.
  void init(NodeType* leaves, int n_leaves_, int level = 0);

  /// @brief Initialize the tree by a set of leaves using algorithm with a given level.
  size_t insert(const BV& bv, void* data);

  /// @brief Remove a leaf node
  void remove(size_t leaf);

  /// @brief Clear the tree 
  void clear();

  /// @brief Whether the tree is empty 
  bool empty() const;
 
  /// @brief update one leaf node 
  void update(size_t leaf, int lookahead_level = -1);

  /// @brief update the tree when the bounding volume of a given leaf has changed
  bool update(size_t leaf, const BV& bv);

  /// @brief update one leaf's bounding volume, with prediction 
  bool update(size_t leaf, const BV& bv, const Vector3<S>& vel, S margin);

  /// @brief update one leaf's bounding volume, with prediction 
  bool update(size_t leaf, const BV& bv, const Vector3<S>& vel);

  /// @brief get the max height of the tree
  size_t getMaxHeight() const;

  /// @brief get the max depth of the tree
  size_t getMaxDepth() const;

  /// @brief balance the tree from bottom 
  void balanceBottomup();

  /// @brief balance the tree from top 
  void balanceTopdown();

  /// @brief balance the tree in an incremental way 
  void balanceIncremental(int iterations);

  /// @brief refit the tree, i.e., when the leaf nodes' bounding volumes change, update the entire tree in a bottom-up manner
  void refit();

  /// @brief extract all the leaves of the tree 
  void extractLeaves(size_t root, NodeType*& leaves) const;

  /// @brief number of leaves in the tree
  size_t size() const;

  /// @brief get the root of the tree
  size_t getRoot() const;

  /// @brief get the pointer to the nodes array
  NodeType* getNodes() const;

  /// @brief print the tree in a recursive way
  void print(size_t root, int depth);

private:

  /// @brief construct a tree for a set of leaves from bottom -- very heavy way 
  void bottomup(size_t* lbeg, size_t* lend);
  
  /// @brief construct a tree for a set of leaves from top 
  size_t topdown(size_t* lbeg, size_t* lend);

  /// @brief compute the maximum height of a subtree rooted from a given node
  size_t getMaxHeight(size_t node) const;

  /// @brief compute the maximum depth of a subtree rooted from a given node
  void getMaxDepth(size_t node, size_t depth, size_t& max_depth) const;

  /// @brief construct a tree from a list of nodes stored in [lbeg, lend) in a topdown manner.
  /// During construction, first compute the best split axis as the axis along with the longest AABB<S> edge.
  /// Then compute the median of all nodes' center projection onto the axis and using it as the split threshold.
  size_t topdown_0(size_t* lbeg, size_t* lend);

  /// @brief construct a tree from a list of nodes stored in [lbeg, lend) in a topdown manner.
  /// During construction, first compute the best split thresholds for different axes as the average of all nodes' center.
  /// Then choose the split axis as the axis whose threshold can divide the nodes into two parts with almost similar size.
  /// This construction is more expensive then topdown_0, but also can provide tree with better quality.
  size_t topdown_1(size_t* lbeg, size_t* lend);

  /// @brief init tree from leaves in the topdown manner (topdown_0 or topdown_1)
  void init_0(NodeType* leaves, int n_leaves_);

  /// @brief init tree from leaves using morton code. It uses morton_0, i.e., for nodes which is of depth more than the maximum bits of the morton code,
  /// we use bottomup method to construct the subtree, which is slow but can construct tree with high quality.
  void init_1(NodeType* leaves, int n_leaves_);

  /// @brief init tree from leaves using morton code. It uses morton_0, i.e., for nodes which is of depth more than the maximum bits of the morton code,
  /// we split the leaves into two parts with the same size simply using the node index. 
  void init_2(NodeType* leaves, int n_leaves_);

  /// @brief init tree from leaves using morton code. It uses morton_2, i.e., for all nodes, we simply divide the leaves into parts with the same size simply using the node index.
  void init_3(NodeType* leaves, int n_leaves_);

  size_t mortonRecurse_0(size_t* lbeg, size_t* lend, const uint32& split, int bits);

  size_t mortonRecurse_1(size_t* lbeg, size_t* lend, const uint32& split, int bits);

  size_t mortonRecurse_2(size_t* lbeg, size_t* lend);

  /// @brief update one leaf node's bounding volume 
  void update_(size_t leaf, const BV& bv);

  /// @brief Insert a leaf node and also update its ancestors 
  void insertLeaf(size_t root, size_t leaf);

  /// @brief Remove a leaf. The leaf node itself is not deleted yet, but all the unnecessary internal nodes are deleted.
  /// return the node with the smallest depth and is influenced by the remove operation 
  size_t removeLeaf(size_t leaf);

  /// @brief Delete all internal nodes and return all leaves nodes with given depth from root 
  void fetchLeaves(size_t root, NodeType*& leaves, int depth = -1);

  size_t indexOf(size_t node);

  size_t allocateNode();

  /// @brief create one node (leaf or internal)
  size_t createNode(size_t parent, 
                    const BV& bv1,
                    const BV& bv2,
                    void* data);

  size_t createNode(size_t parent,
                    const BV& bv, 
                    void* data);

  size_t createNode(size_t parent,
                    void* data);

  void deleteNode(size_t node);

  void recurseRefit(size_t node);

protected:
  size_t root_node;
  NodeType* nodes;
  size_t n_nodes;
  size_t n_nodes_alloc;
  
  size_t n_leaves;
  size_t freelist;
  unsigned int opath;

  int max_lookahead_level;

public:
  /// @brief decide which topdown algorithm to use
  int topdown_level;

  /// @brief decide the depth to use expensive bottom-up algorithm
  int bu_threshold;

public:
  static const size_t NULL_NODE = -1;
};

template<typename BV>
const size_t HierarchyTree<BV>::NULL_NODE;


/// @brief Functor comparing two nodes
template<typename BV>
struct nodeBaseLess
{
  nodeBaseLess(const NodeBase<BV>* nodes_, size_t d_);

  bool operator() (size_t i, size_t j) const;

private:

  /// @brief the nodes array
  const NodeBase<BV>* nodes;

  /// @brief the dimension to compare
  size_t d;
};

/// @brief select the node from node1 and node2 which is close to the query-th
/// node in the nodes. 0 for node1 and 1 for node2.
template<typename BV>
size_t select(size_t query, size_t node1, size_t node2, NodeBase<BV>* nodes);

/// @brief select the node from node1 and node2 which is close to the query
/// AABB<S>. 0 for node1 and 1 for node2.
template<typename BV>
size_t select(const BV& query, size_t node1, size_t node2, NodeBase<BV>* nodes);

//============================================================================//
//                                                                            //
//                              Implementations                               //
//                                                                            //
//============================================================================//

//==============================================================================
template<typename BV>
HierarchyTree<BV>::HierarchyTree(int bu_threshold_, int topdown_level_)
{
  root_node = NULL_NODE;
  n_nodes = 0;
  n_nodes_alloc = 16;
  nodes = new NodeType[n_nodes_alloc];
  for(size_t i = 0; i < n_nodes_alloc - 1; ++i)
    nodes[i].next = i + 1;
  nodes[n_nodes_alloc - 1].next = NULL_NODE;
  n_leaves = 0;
  freelist = 0;
  opath = 0;
  max_lookahead_level = -1;
  bu_threshold = bu_threshold_;
  topdown_level = topdown_level_;
}

//==============================================================================
template<typename BV>
HierarchyTree<BV>::~HierarchyTree()
{
  delete [] nodes;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::init(NodeType* leaves, int n_leaves_, int level)
{
  switch(level)
  {
  case 0:
    init_0(leaves, n_leaves_);
    break;
  case 1:
    init_1(leaves, n_leaves_);
    break;
  case 2:
    init_2(leaves, n_leaves_);
    break;
  case 3:
    init_3(leaves, n_leaves_);
    break;
  default:
    init_0(leaves, n_leaves_);
  }
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::init_0(NodeType* leaves, int n_leaves_)
{
  clear();

  n_leaves = n_leaves_;
  root_node = NULL_NODE;
  nodes = new NodeType[n_leaves * 2];
  memcpy(nodes, leaves, sizeof(NodeType) * n_leaves);
  freelist = n_leaves;
  n_nodes = n_leaves;
  n_nodes_alloc = 2 * n_leaves;
  for(size_t i = n_leaves; i < n_nodes_alloc; ++i)
    nodes[i].next = i + 1;
  nodes[n_nodes_alloc - 1].next = NULL_NODE;

  size_t* ids = new size_t[n_leaves];
  for(size_t i = 0; i < n_leaves; ++i)
    ids[i] = i;

  root_node = topdown(ids, ids + n_leaves);
  delete [] ids;

  opath = 0;
  max_lookahead_level = -1;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::init_1(NodeType* leaves, int n_leaves_)
{
  clear();

  n_leaves = n_leaves_;
  root_node = NULL_NODE;
  nodes = new NodeType[n_leaves * 2];
  memcpy(nodes, leaves, sizeof(NodeType) * n_leaves);
  freelist = n_leaves;
  n_nodes = n_leaves;
  n_nodes_alloc = 2 * n_leaves;
  for(size_t i = n_leaves; i < n_nodes_alloc; ++i)
    nodes[i].next = i + 1;
  nodes[n_nodes_alloc - 1].next = NULL_NODE;


  BV bound_bv;
  if(n_leaves > 0)
    bound_bv = nodes[0].bv;
  for(size_t i = 1; i < n_leaves; ++i)
    bound_bv += nodes[i].bv;

  morton_functor<typename BV::S, uint32> coder(bound_bv);
  for(size_t i = 0; i < n_leaves; ++i)
    nodes[i].code = coder(nodes[i].bv.center());


  size_t* ids = new size_t[n_leaves];
  for(size_t i = 0; i < n_leaves; ++i)
    ids[i] = i;

  FCL_SUPPRESS_MAYBE_UNINITIALIZED_BEGIN
  SortByMorton comp;
  FCL_SUPPRESS_MAYBE_UNINITIALIZED_END
  comp.nodes = nodes;
  std::sort(ids, ids + n_leaves, comp);
  root_node = mortonRecurse_0(ids, ids + n_leaves, (1 << (coder.bits()-1)), coder.bits()-1);
  delete [] ids;

  refit();

  opath = 0;
  max_lookahead_level = -1;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::init_2(NodeType* leaves, int n_leaves_)
{
  clear();

  n_leaves = n_leaves_;
  root_node = NULL_NODE;
  nodes = new NodeType[n_leaves * 2];
  memcpy(nodes, leaves, sizeof(NodeType) * n_leaves);
  freelist = n_leaves;
  n_nodes = n_leaves;
  n_nodes_alloc = 2 * n_leaves;
  for(size_t i = n_leaves; i < n_nodes_alloc; ++i)
    nodes[i].next = i + 1;
  nodes[n_nodes_alloc - 1].next = NULL_NODE;


  BV bound_bv;
  if(n_leaves > 0)
    bound_bv = nodes[0].bv;
  for(size_t i = 1; i < n_leaves; ++i)
    bound_bv += nodes[i].bv;

  morton_functor<typename BV::S, uint32> coder(bound_bv);
  for(size_t i = 0; i < n_leaves; ++i)
    nodes[i].code = coder(nodes[i].bv.center());


  size_t* ids = new size_t[n_leaves];
  for(size_t i = 0; i < n_leaves; ++i)
    ids[i] = i;

  FCL_SUPPRESS_MAYBE_UNINITIALIZED_BEGIN
  SortByMorton comp;
  FCL_SUPPRESS_MAYBE_UNINITIALIZED_END
  comp.nodes = nodes;
  std::sort(ids, ids + n_leaves, comp);
  root_node = mortonRecurse_1(ids, ids + n_leaves, (1 << (coder.bits()-1)), coder.bits()-1);
  delete [] ids;

  refit();

  opath = 0;
  max_lookahead_level = -1;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::init_3(NodeType* leaves, int n_leaves_)
{
  clear();

  n_leaves = n_leaves_;
  root_node = NULL_NODE;
  nodes = new NodeType[n_leaves * 2];
  memcpy(nodes, leaves, sizeof(NodeType) * n_leaves);
  freelist = n_leaves;
  n_nodes = n_leaves;
  n_nodes_alloc = 2 * n_leaves;
  for(size_t i = n_leaves; i < n_nodes_alloc; ++i)
    nodes[i].next = i + 1;
  nodes[n_nodes_alloc - 1].next = NULL_NODE;


  BV bound_bv;
  if(n_leaves > 0)
    bound_bv = nodes[0].bv;
  for(size_t i = 1; i < n_leaves; ++i)
    bound_bv += nodes[i].bv;

  morton_functor<typename BV::S, uint32> coder(bound_bv);
  for(size_t i = 0; i < n_leaves; ++i)
    nodes[i].code = coder(nodes[i].bv.center());


  size_t* ids = new size_t[n_leaves];
  for(size_t i = 0; i < n_leaves; ++i)
    ids[i] = i;

  FCL_SUPPRESS_MAYBE_UNINITIALIZED_BEGIN
  SortByMorton comp;
  FCL_SUPPRESS_MAYBE_UNINITIALIZED_END
  comp.nodes = nodes;
  std::sort(ids, ids + n_leaves, comp);
  root_node = mortonRecurse_2(ids, ids + n_leaves);
  delete [] ids;

  refit();

  opath = 0;
  max_lookahead_level = -1;
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::insert(const BV& bv, void* data)
{
  size_t node = createNode(NULL_NODE, bv, data);
  insertLeaf(root_node, node);
  ++n_leaves;
  return node;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::remove(size_t leaf)
{
  removeLeaf(leaf);
  deleteNode(leaf);
  --n_leaves;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::clear()
{
  delete [] nodes;
  root_node = NULL_NODE;
  n_nodes = 0;
  n_nodes_alloc = 16;
  nodes = new NodeType[n_nodes_alloc];
  for(size_t i = 0; i < n_nodes_alloc; ++i)
    nodes[i].next = i + 1;
  nodes[n_nodes_alloc - 1].next = NULL_NODE;
  n_leaves = 0;
  freelist = 0;
  opath = 0;
  max_lookahead_level = -1;
}

//==============================================================================
template<typename BV>
bool HierarchyTree<BV>::empty() const
{
  return (n_nodes == 0);
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::update(size_t leaf, int lookahead_level)
{
  size_t root = removeLeaf(leaf);
  if(root != NULL_NODE)
  {
    if(lookahead_level > 0)
    {
      for(int i = 0; (i < lookahead_level) && (nodes[root].parent != NULL_NODE); ++i)
        root = nodes[root].parent;
    }
    else
      root = root_node;
  }
  insertLeaf(root, leaf);
}

//==============================================================================
template<typename BV>
bool HierarchyTree<BV>::update(size_t leaf, const BV& bv)
{
  if(nodes[leaf].bv.contain(bv)) return false;
  update_(leaf, bv);
  return true;
}

//==============================================================================
template<typename BV>
bool HierarchyTree<BV>::update(size_t leaf, const BV& bv, const Vector3<S>& vel, S margin)
{
  if(nodes[leaf].bv.contain(bv)) return false;
  update_(leaf, bv);
  return true;
}

//==============================================================================
template<typename BV>
bool HierarchyTree<BV>::update(size_t leaf, const BV& bv, const Vector3<S>& vel)
{
  if(nodes[leaf].bv.contain(bv)) return false;
  update_(leaf, bv);
  return true;
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::getMaxHeight() const
{
  if(root_node == NULL_NODE) return 0;

  return getMaxHeight(root_node);
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::getMaxDepth() const
{
  if(root_node == NULL_NODE) return 0;

  size_t max_depth;
  getMaxDepth(root_node, 0, max_depth);
  return max_depth;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::balanceBottomup()
{
  if(root_node != NULL_NODE)
  {
    NodeType* leaves = new NodeType[n_leaves];
    NodeType* leaves_ = leaves;
    extractLeaves(root_node, leaves_);
    root_node = NULL_NODE;
    memcpy(nodes, leaves, sizeof(NodeType) * n_leaves);
    freelist = n_leaves;
    n_nodes = n_leaves;
    for(size_t i = n_leaves; i < n_nodes_alloc; ++i)
      nodes[i].next = i + 1;
    nodes[n_nodes_alloc - 1].next = NULL_NODE;


    size_t* ids = new size_t[n_leaves];
    for(size_t i = 0; i < n_leaves; ++i)
      ids[i] = i;

    bottomup(ids, ids + n_leaves);
    root_node = *ids;

    delete [] ids;
  }
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::balanceTopdown()
{
  if(root_node != NULL_NODE)
  {
    NodeType* leaves = new NodeType[n_leaves];
    NodeType* leaves_ = leaves;
    extractLeaves(root_node, leaves_);
    root_node = NULL_NODE;
    memcpy(nodes, leaves, sizeof(NodeType) * n_leaves);
    freelist = n_leaves;
    n_nodes = n_leaves;
    for(size_t i = n_leaves; i < n_nodes_alloc; ++i)
      nodes[i].next = i + 1;
    nodes[n_nodes_alloc - 1].next = NULL_NODE;

    size_t* ids = new size_t[n_leaves];
    for(size_t i = 0; i < n_leaves; ++i)
      ids[i] = i;

    root_node = topdown(ids, ids + n_leaves);
    delete [] ids;
  }
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::balanceIncremental(int iterations)
{
  if(iterations < 0) iterations = n_leaves;
  if((root_node != NULL_NODE) && (iterations > 0))
  {
    for(int i = 0; i < iterations; ++i)
    {
      size_t node = root_node;
      unsigned int bit = 0;
      while(!nodes[node].isLeaf())
      {
        node = nodes[node].children[(opath>>bit)&1];
        bit = (bit+1)&(sizeof(unsigned int) * 8-1);
      }
      update(node);
      ++opath;
    }
  }
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::refit()
{
  if(root_node != NULL_NODE)
    recurseRefit(root_node);
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::extractLeaves(size_t root, NodeType*& leaves) const
{
  if(!nodes[root].isLeaf())
  {
    extractLeaves(nodes[root].children[0], leaves);
    extractLeaves(nodes[root].children[1], leaves);
  }
  else
  {
    *leaves = nodes[root];
    leaves++;
  }
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::size() const
{
  return n_leaves;
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::getRoot() const
{
  return root_node;
}

//==============================================================================
template<typename BV>
typename HierarchyTree<BV>::NodeType* HierarchyTree<BV>::getNodes() const
{
  return nodes;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::print(size_t root, int depth)
{
  for(int i = 0; i < depth; ++i)
    std::cout << " ";
  NodeType* n = nodes + root;
  std::cout << " (" << n->bv.min_[0] << ", " << n->bv.min_[1] << ", " << n->bv.min_[2] << "; " << n->bv.max_[0] << ", " << n->bv.max_[1] << ", " << n->bv.max_[2] << ")" << std::endl;
  if(n->isLeaf())
  {
  }
  else
  {
    print(n->children[0], depth+1);
    print(n->children[1], depth+1);
  }
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::getMaxHeight(size_t node) const
{
  if(!nodes[node].isLeaf())
  {
    size_t height1 = getMaxHeight(nodes[node].children[0]);
    size_t height2 = getMaxHeight(nodes[node].children[1]);
    return std::max(height1, height2) + 1;
  }
  else
    return 0;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::getMaxDepth(size_t node, size_t depth, size_t& max_depth) const
{
  if(!nodes[node].isLeaf())
  {
    getMaxDepth(nodes[node].children[0], depth+1, max_depth);
    getmaxDepth(nodes[node].children[1], depth+1, max_depth);
  }
  else
    max_depth = std::max(max_depth, depth);
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::bottomup(size_t* lbeg, size_t* lend)
{
  size_t* lcur_end = lend;
  while(lbeg < lcur_end - 1)
  {
    size_t* min_it1 = nullptr, *min_it2 = nullptr;
    S min_size = std::numeric_limits<S>::max();
    for(size_t* it1 = lbeg; it1 < lcur_end; ++it1)
    {
      for(size_t* it2 = it1 + 1; it2 < lcur_end; ++it2)
      {
        S cur_size = (nodes[*it1].bv + nodes[*it2].bv).size();
        if(cur_size < min_size)
        {
          min_size = cur_size;
          min_it1 = it1;
          min_it2 = it2;
        }
      }
    }

    size_t p = createNode(NULL_NODE, nodes[*min_it1].bv, nodes[*min_it2].bv, nullptr);
    nodes[p].children[0] = *min_it1;
    nodes[p].children[1] = *min_it2;
    nodes[*min_it1].parent = p;
    nodes[*min_it2].parent = p;
    *min_it1 = p;
    size_t tmp = *min_it2;
    lcur_end--;
    *min_it2 = *lcur_end;
    *lcur_end = tmp;
  }
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::topdown(size_t* lbeg, size_t* lend)
{
  switch(topdown_level)
  {
  case 0:
    return topdown_0(lbeg, lend);
    break;
  case 1:
    return topdown_1(lbeg, lend);
    break;
  default:
    return topdown_0(lbeg, lend);
  }
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::topdown_0(size_t* lbeg, size_t* lend)
{
  int num_leaves = lend - lbeg;
  if(num_leaves > 1)
  {
    if(num_leaves > bu_threshold)
    {
      BV vol = nodes[*lbeg].bv;
      for(size_t* i = lbeg + 1; i < lend; ++i)
        vol += nodes[*i].bv;

      int best_axis = 0;
      S extent[3] = {vol.width(), vol.height(), vol.depth()};
      if(extent[1] > extent[0]) best_axis = 1;
      if(extent[2] > extent[best_axis]) best_axis = 2;

      nodeBaseLess<BV> comp(nodes, best_axis);
      size_t* lcenter = lbeg + num_leaves / 2;
      std::nth_element(lbeg, lcenter, lend, comp);

      size_t node = createNode(NULL_NODE, vol, nullptr);
      nodes[node].children[0] = topdown_0(lbeg, lcenter);
      nodes[node].children[1] = topdown_0(lcenter, lend);
      nodes[nodes[node].children[0]].parent = node;
      nodes[nodes[node].children[1]].parent = node;
      return node;
    }
    else
    {
      bottomup(lbeg, lend);
      return *lbeg;
    }
  }
  return *lbeg;
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::topdown_1(size_t* lbeg, size_t* lend)
{
  int num_leaves = lend - lbeg;
  if(num_leaves > 1)
  {
    if(num_leaves > bu_threshold)
    {
      Vector3<S> split_p = nodes[*lbeg].bv.center();
      BV vol = nodes[*lbeg].bv;
      for(size_t* i = lbeg + 1; i < lend; ++i)
      {
        split_p += nodes[*i].bv.center();
        vol += nodes[*i].bv;
      }
      split_p /= (S)(num_leaves);
      int best_axis = -1;
      int bestmidp = num_leaves;
      int splitcount[3][2] = {{0,0}, {0,0}, {0,0}};
      for(size_t* i = lbeg; i < lend; ++i)
      {
        Vector3<S> x = nodes[*i].bv.center() - split_p;
        for(size_t j = 0; j < 3; ++j)
          ++splitcount[j][x[j] > 0 ? 1 : 0];
      }

      for(size_t i = 0; i < 3; ++i)
      {
        if((splitcount[i][0] > 0) && (splitcount[i][1] > 0))
        {
          int midp = std::abs(splitcount[i][0] - splitcount[i][1]);
          if(midp < bestmidp)
          {
            best_axis = i;
            bestmidp = midp;
          }
        }
      }

      if(best_axis < 0) best_axis = 0;

      S split_value = split_p[best_axis];
      size_t* lcenter = lbeg;
      for(size_t* i = lbeg; i < lend; ++i)
      {
        if(nodes[*i].bv.center()[best_axis] < split_value)
        {
          size_t temp = *i;
          *i = *lcenter;
          *lcenter = temp;
          ++lcenter;
        }
      }

      size_t node = createNode(NULL_NODE, vol, nullptr);
      nodes[node].children[0] = topdown_1(lbeg, lcenter);
      nodes[node].children[1] = topdown_1(lcenter, lend);
      nodes[nodes[node].children[0]].parent = node;
      nodes[nodes[node].children[1]].parent = node;
      return node;
    }
    else
    {
      bottomup(lbeg, lend);
      return *lbeg;
    }
  }
  return *lbeg;
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::mortonRecurse_0(size_t* lbeg, size_t* lend, const uint32& split, int bits)
{
  int num_leaves = lend - lbeg;
  if(num_leaves > 1)
  {
    if(bits > 0)
    {
      SortByMorton comp;
      comp.nodes = nodes;
      comp.split = split;
      size_t* lcenter = std::lower_bound(lbeg, lend, NULL_NODE, comp);

      if(lcenter == lbeg)
      {
        uint32 split2 = split | (1 << (bits - 1));
        return mortonRecurse_0(lbeg, lend, split2, bits - 1);
      }
      else if(lcenter == lend)
      {
        uint32 split1 = (split & (~(1 << bits))) | (1 << (bits - 1));
        return mortonRecurse_0(lbeg, lend, split1, bits - 1);
      }
      else
      {
        uint32 split1 = (split & (~(1 << bits))) | (1 << (bits - 1));
        uint32 split2 = split | (1 << (bits - 1));

        size_t child1 = mortonRecurse_0(lbeg, lcenter, split1, bits - 1);
        size_t child2 = mortonRecurse_0(lcenter, lend, split2, bits - 1);
        size_t node = createNode(NULL_NODE, nullptr);
        nodes[node].children[0] = child1;
        nodes[node].children[1] = child2;
        nodes[child1].parent = node;
        nodes[child2].parent = node;
        return node;
      }
    }
    else
    {
      size_t node = topdown(lbeg, lend);
      return node;
    }
  }
  else
    return *lbeg;
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::mortonRecurse_1(size_t* lbeg, size_t* lend, const uint32& split, int bits)
{
  int num_leaves = lend - lbeg;
  if(num_leaves > 1)
  {
    if(bits > 0)
    {
      SortByMorton comp;
      comp.nodes = nodes;
      comp.split = split;
      size_t* lcenter = std::lower_bound(lbeg, lend, NULL_NODE, comp);

      if(lcenter == lbeg)
      {
        uint32 split2 = split | (1 << (bits - 1));
        return mortonRecurse_1(lbeg, lend, split2, bits - 1);
      }
      else if(lcenter == lend)
      {
        uint32 split1 = (split & (~(1 << bits))) | (1 << (bits - 1));
        return mortonRecurse_1(lbeg, lend, split1, bits - 1);
      }
      else
      {
        uint32 split1 = (split & (~(1 << bits))) | (1 << (bits - 1));
        uint32 split2 = split | (1 << (bits - 1));

        size_t child1 = mortonRecurse_1(lbeg, lcenter, split1, bits - 1);
        size_t child2 = mortonRecurse_1(lcenter, lend, split2, bits - 1);
        size_t node = createNode(NULL_NODE, nullptr);
        nodes[node].children[0] = child1;
        nodes[node].children[1] = child2;
        nodes[child1].parent = node;
        nodes[child2].parent = node;
        return node;
      }
    }
    else
    {
      size_t child1 = mortonRecurse_1(lbeg, lbeg + num_leaves / 2, 0, bits - 1);
      size_t child2 = mortonRecurse_1(lbeg + num_leaves / 2, lend, 0, bits - 1);
      size_t node = createNode(NULL_NODE, nullptr);
      nodes[node].children[0] = child1;
      nodes[node].children[1] = child2;
      nodes[child1].parent = node;
      nodes[child2].parent = node;
      return node;
    }
  }
  else
    return *lbeg;
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::mortonRecurse_2(size_t* lbeg, size_t* lend)
{
  int num_leaves = lend - lbeg;
  if(num_leaves > 1)
  {
    size_t child1 = mortonRecurse_2(lbeg, lbeg + num_leaves / 2);
    size_t child2 = mortonRecurse_2(lbeg + num_leaves / 2, lend);
    size_t node = createNode(NULL_NODE, nullptr);
    nodes[node].children[0] = child1;
    nodes[node].children[1] = child2;
    nodes[child1].parent = node;
    nodes[child2].parent = node;
    return node;
  }
  else
    return *lbeg;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::insertLeaf(size_t root, size_t leaf)
{
  if(root_node == NULL_NODE)
  {
    root_node = leaf;
    nodes[leaf].parent = NULL_NODE;
  }
  else
  {
    if(!nodes[root].isLeaf())
    {
      do
      {
        root = nodes[root].children[select(leaf, nodes[root].children[0], nodes[root].children[1], nodes)];
      }
      while(!nodes[root].isLeaf());
    }

    size_t prev = nodes[root].parent;
    size_t node = createNode(prev, nodes[leaf].bv, nodes[root].bv, nullptr);
    if(prev != NULL_NODE)
    {
      nodes[prev].children[indexOf(root)] = node;
      nodes[node].children[0] = root; nodes[root].parent = node;
      nodes[node].children[1] = leaf; nodes[leaf].parent = node;
      do
      {
        if(!nodes[prev].bv.contain(nodes[node].bv))
          nodes[prev].bv = nodes[nodes[prev].children[0]].bv + nodes[nodes[prev].children[1]].bv;
        else
          break;
        node = prev;
      } while (NULL_NODE != (prev = nodes[node].parent));
    }
    else
    {
      nodes[node].children[0] = root; nodes[root].parent = node;
      nodes[node].children[1] = leaf; nodes[leaf].parent = node;
      root_node = node;
    }
  }
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::removeLeaf(size_t leaf)
{
  if(leaf == root_node)
  {
    root_node = NULL_NODE;
    return NULL_NODE;
  }
  else
  {
    size_t parent = nodes[leaf].parent;
    size_t prev = nodes[parent].parent;
    size_t sibling = nodes[parent].children[1-indexOf(leaf)];

    if(prev != NULL_NODE)
    {
      nodes[prev].children[indexOf(parent)] = sibling;
      nodes[sibling].parent = prev;
      deleteNode(parent);
      while(prev != NULL_NODE)
      {
        BV new_bv = nodes[nodes[prev].children[0]].bv + nodes[nodes[prev].children[1]].bv;
        if(!new_bv.equal(nodes[prev].bv))
        {
          nodes[prev].bv = new_bv;
          prev = nodes[prev].parent;
        }
        else break;
      }

      return (prev != NULL_NODE) ? prev : root_node;
    }
    else
    {
      root_node = sibling;
      nodes[sibling].parent = NULL_NODE;
      deleteNode(parent);
      return root_node;
    }
  }
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::update_(size_t leaf, const BV& bv)
{
  size_t root = removeLeaf(leaf);
  if(root != NULL_NODE)
  {
    if(max_lookahead_level >= 0)
    {
      for(int i = 0; (i < max_lookahead_level) && (nodes[root].parent != NULL_NODE); ++i)
        root = nodes[root].parent;
    }

    nodes[leaf].bv = bv;
    insertLeaf(root, leaf);
  }
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::indexOf(size_t node)
{
  return (nodes[nodes[node].parent].children[1] == node);
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::allocateNode()
{
  if(freelist == NULL_NODE)
  {
    NodeType* old_nodes = nodes;
    n_nodes_alloc *= 2;
    nodes = new NodeType[n_nodes_alloc];
    memcpy(nodes, old_nodes, n_nodes * sizeof(NodeType));
    delete [] old_nodes;

    for(size_t i = n_nodes; i < n_nodes_alloc - 1; ++i)
      nodes[i].next = i + 1;
    nodes[n_nodes_alloc - 1].next = NULL_NODE;
    freelist = n_nodes;
  }

  size_t node_id = freelist;
  freelist = nodes[node_id].next;
  nodes[node_id].parent = NULL_NODE;
  nodes[node_id].children[0] = NULL_NODE;
  nodes[node_id].children[1] = NULL_NODE;
  ++n_nodes;
  return node_id;
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::createNode(size_t parent,
                                     const BV& bv1,
                                     const BV& bv2,
                                     void* data)
{
  size_t node = allocateNode();
  nodes[node].parent = parent;
  nodes[node].data = data;
  nodes[node].bv = bv1 + bv2;
  return node;
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::createNode(size_t parent,
                                     const BV& bv,
                                     void* data)
{
  size_t node = allocateNode();
  nodes[node].parent = parent;
  nodes[node].data = data;
  nodes[node].bv = bv;
  return node;
}

//==============================================================================
template<typename BV>
size_t HierarchyTree<BV>::createNode(size_t parent,
                                     void* data)
{
  size_t node = allocateNode();
  nodes[node].parent = parent;
  nodes[node].data = data;
  return node;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::deleteNode(size_t node)
{
  nodes[node].next = freelist;
  freelist = node;
  --n_nodes;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::recurseRefit(size_t node)
{
  if(!nodes[node].isLeaf())
  {
    recurseRefit(nodes[node].children[0]);
    recurseRefit(nodes[node].children[1]);
    nodes[node].bv = nodes[nodes[node].children[0]].bv + nodes[nodes[node].children[1]].bv;
  }
  else
    return;
}

//==============================================================================
template<typename BV>
void HierarchyTree<BV>::fetchLeaves(size_t root, NodeType*& leaves, int depth)
{
  if((!nodes[root].isLeaf()) && depth)
  {
    fetchLeaves(nodes[root].children[0], leaves, depth-1);
    fetchLeaves(nodes[root].children[1], leaves, depth-1);
    deleteNode(root);
  }
  else
  {
    *leaves = nodes[root];
    leaves++;
  }
}

//==============================================================================
template<typename BV>
nodeBaseLess<BV>::nodeBaseLess(const NodeBase<BV>* nodes_, size_t d_)
  : nodes(nodes_), d(d_)
{
  // Do nothing
}

//==============================================================================
template<typename BV>
bool nodeBaseLess<BV>::operator()(size_t i, size_t j) const
{
  if(nodes[i].bv.center()[d] < nodes[j].bv.center()[d])
    return true;

  return false;
}

//==============================================================================
template <typename S, typename BV>
struct SelectImpl
{
  static bool run(size_t query, size_t node1, size_t node2, NodeBase<BV>* nodes)
  {
    return 0;
  }

  static bool run(const BV& query, size_t node1, size_t node2, NodeBase<BV>* nodes)
  {
    return 0;
  }
};

//==============================================================================
template<typename BV>
size_t select(size_t query, size_t node1, size_t node2, NodeBase<BV>* nodes)
{
  return SelectImpl<typename BV::S, BV>::run(query, node1, node2, nodes);
}

//==============================================================================
template<typename BV>
size_t select(const BV& query, size_t node1, size_t node2, NodeBase<BV>* nodes)
{
  return SelectImpl<typename BV::S, BV>::run(query, node1, node2, nodes);
}

//==============================================================================
template <typename S>
struct SelectImpl<S, AABB<S>>
{
  static bool run(size_t query, size_t node1, size_t node2, NodeBase<AABB<S>>* nodes)
  {
    const AABB<S>& bv = nodes[query].bv;
    const AABB<S>& bv1 = nodes[node1].bv;
    const AABB<S>& bv2 = nodes[node2].bv;
    Vector3<S> v = bv.min_ + bv.max_;
    Vector3<S> v1 = v - (bv1.min_ + bv1.max_);
    Vector3<S> v2 = v - (bv2.min_ + bv2.max_);
    S d1 = fabs(v1[0]) + fabs(v1[1]) + fabs(v1[2]);
    S d2 = fabs(v2[0]) + fabs(v2[1]) + fabs(v2[2]);
    return (d1 < d2) ? 0 : 1;
  }

  static bool run(const AABB<S>& query, size_t node1, size_t node2, NodeBase<AABB<S>>* nodes)
  {
    const AABB<S>& bv = query;
    const AABB<S>& bv1 = nodes[node1].bv;
    const AABB<S>& bv2 = nodes[node2].bv;
    Vector3<S> v = bv.min_ + bv.max_;
    Vector3<S> v1 = v - (bv1.min_ + bv1.max_);
    Vector3<S> v2 = v - (bv2.min_ + bv2.max_);
    S d1 = fabs(v1[0]) + fabs(v1[1]) + fabs(v1[2]);
    S d2 = fabs(v2[0]) + fabs(v2[1]) + fabs(v2[2]);
    return (d1 < d2) ? 0 : 1;
  }
};

} // namespace implementation_array
} // namespace detail
} // namespace fcl

#endif