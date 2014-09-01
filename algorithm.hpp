#pragma once
#ifndef _ALGORITHM_HPP
#define _ALGORITHM_HPP

#include <iostream>

#include <sdsl/bit_vectors.hpp>
#include "debruijn_graph.hpp"

using namespace std;
using namespace sdsl;

// Calling it with a vector of first minus positions is sliiiightly faster
// But would mean we have to save another vector with our graph if we ever want to traverse
// it again.
// If a vector isnt provided, another method is used which is *almost as fast*
template <bool has_branch = 1, class V=sd_vector<>, class G>
V make_branch_vector(const G& g, const vector<size_t> * v = nullptr) {
  bit_vector branching_flags(g.num_edges(),!has_branch);
  // OUTDEGREE
  for (size_t edge = 0; edge < g.num_edges()-1; edge++) {
    // This will only unset the bits that are 0,0 (which means no 11, 10, or 01, which is a sibling edge)
    // 0,0 -> !has_branch, 0,1 -> has_branch,
    // 1,0 ->  has_branch, 1,1 -> has_branch,
    branching_flags[edge] = ((g.m_node_flags[edge] || g.m_node_flags[edge+1]) == has_branch);
  }
  // The last edge is whatever it is in the graph
  // 0 -> !has_branch, 1-> has_branch
  branching_flags[g.num_edges()-1] = (g.m_node_flags[g.num_edges()-1] == has_branch);

  // INDEGREE
  // Seems like we should iterate over all possible edges, but these will have been captured
  // by the outdegree loop above
  if (v) {
    for (auto edge : *v) {
      ssize_t next = g._forward(edge);
      branching_flags[next] = has_branch;
    }
  }
  else {
    // This is close to the same speed as loading in vector created in the constructor
    // But easier for people to use
    typedef typename G::symbol_type symbol_type;
    // Ignore $ edges, which shouldn't be there anyway, but follow every other symbol
    for (symbol_type sym = 1; sym < g.sigma + 1; sym++) {
      symbol_type x = g._with_edge_flag(sym, false);      // edges without flag
      symbol_type x_minus = g._with_edge_flag(sym, true); // edges with flag
      size_t non_minus_bound = g.m_edges.rank(g.num_edges(), x); // how many edges
      size_t minus_bound = g.m_edges.rank(g.num_edges(), x_minus); // how many minus flags do we have?
      for(size_t next_sel = 1; next_sel <= minus_bound; ) { // bound is inclusive
        size_t edge = g.m_edges.select(next_sel, x_minus); // locate the appropriate pre-edge (must have minus otherwise outdegree <= 1)
        branching_flags[g._forward(edge)] = has_branch; // follow edge and set the flag
        size_t prev_non_flags = g.m_edges.rank(edge, x); // edge is a minus
        size_t next_non_flag = prev_non_flags + 1; // Check that this isn't out of bounds
        size_t next_non_flag_edge = (next_non_flag > non_minus_bound)? g.num_edges() - 1 : g.m_edges.select(next_non_flag, x);
        // Count flags before and select to next one
        if (next_sel == minus_bound) break;
        next_sel = 1 + g.m_edges.rank(next_non_flag_edge, x_minus);
      }
    }
  }
  return V(branching_flags);
}

template <bool has_branch, class V>
struct _type_getter{ };

template <class V> struct _type_getter<0, V> {
  typedef typename V::rank_0_type rank_type;
  typedef typename V::select_0_type select_type;
};
template <class V> struct _type_getter<1, V> {
  typedef typename V::rank_1_type rank_type;
  typedef typename V::select_1_type select_type;
};

// Should make iterator instead?
template <const bool has_branch = 1, class G, class V, class F>
void visit_unipaths(const G& g, const V& v, const F & f) {
  typedef typename _type_getter<has_branch, V>::rank_type rank_type;
  typedef typename _type_getter<has_branch, V>::select_type select_type;

  rank_type   rank(&v);
  select_type select(&v);

  // Could reverse one step to use F to look up instead of edges...
  for (size_t sel_idx = 1; sel_idx <= rank(v.size()); sel_idx++) {
    // If we didn't have the guarantee that EVERYTHING has at least one incoming edge (thanks dummy edges)
    // then we might have to backtrack here to print the node
    // if we didnt have all the k-1 dummies per edge, we would be missing some symbols
    // but as it is now, all we have to do is traverse forwards
    size_t edge = select(sel_idx);
    assert(v[edge] == has_branch);
    // For each edge... each one should be a has_branch
    // Due to the definition of our branch vector
    // so it will be selected to... so we just follow _forward
    do {
      typename G::symbol_type x = g._strip_edge_flag(g.m_edges[edge]);
      f(g._map_symbol(x));
      if (x == 0) break;
      edge = g._forward(edge);
      // for pairs: check here and at the if x == 0, and return node index of this + first edge
    } while (v[edge] != has_branch);
  }
}

#endif