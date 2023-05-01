#ifndef VG_GFA_HPP_INCLUDED
#define VG_GFA_HPP_INCLUDED

/**
 * \file gfa.hpp
 *
 * Defines GFA I/O algorithms for PathHandleGraphs graphs.
 *
 * Includes an algorithm for converting from GFA, including non-perfect-match
 * edge overlaps and edges that specify containment of one node in another, to
 * a blunt-ended VG.
 */

#include "handle.hpp"
#include "snarls.hpp"
#include "traversal_finder.hpp"

namespace vg {

using namespace std;

/// Export the given VG graph to the given GFA file.
/// Express paths mentioned in rgfa_paths as rGFA.
/// If rgfa_pline is set, also express them as dedicated lines.
/// If use_w_lines is set, reference and haplotype paths will use W lines instead of P lines.
void graph_to_gfa(const PathHandleGraph* graph, ostream& out,
                  const set<string>& rgfa_paths = {},
                  bool rgfa_pline = false,
                  bool use_w_lines = true);


/// Prototype code to tag paths as rGFA paths. Either needs to be completely scrapped
/// or adapted into libhandlegraph  at some point, ideally.
/// It works by adding :SR:i:<rank> to the end of the name

/// Returns the RGFA rank (SR) of a path. This will be 0 for the reference
/// backbone, and higher the further number of (nested) bubbles away it is.
/// If the path is not an RGFA path, then return -1
int get_rgfa_rank(const string& path_name);

/// Add the RGFA rank tag to a pathname
string set_rgfa_rank(const string& path_name, int rgfa_rank);

/// Compute the rGFA path cover
/// graph: the graph
/// snarl_manager: the snarls (todo: should use distance index)
/// reference_paths: rank-0 paths
/// minimum_length: the minimum length of a path to create (alleles shorter than this can be uncovered)
/// preferred_intervals: set of ranges (ex from minigraph) to use as possible for rGFA paths
void rgfa_graph_cover(MutablePathMutableHandleGraph* graph,
                      SnarlManager* snarl_manager,
                      const unordered_set<path_handle_t>& reference_paths,
                      int64_t minimum_length,
                      const unordered_map<string, vector<pair<int64_t, int64_t>>>& preferred_intervals = {});

void rgfa_snarl_cover(const PathHandleGraph* graph,
                      const Snarl& snarl,
                      PathTraversalFinder& path_trav_finder,
                      const unordered_set<path_handle_t>& reference_paths,
                      int64_t minimum_length,
                      vector<pair<int64_t, vector<step_handle_t>>>& cover_fragments,                         
                      unordered_map<nid_t, int64_t>& cover_node_to_fragment,
                      const unordered_map<string, vector<pair<int64_t, int64_t>>>& preferred_intervals);

/// Get some statistics from a traversal fragment that we use for ranking in the greedy algorithm
/// 1. Coverage : total step length across the traversal for all paths
/// 2. Switches : the number of nodes that would need to be flipped to forwardize the traversal
/// 3. Duplicated bases : the number of duplicated bases in the traversal path
tuple<int64_t, int64_t, int64_t> rgfa_traversal_stats(const PathHandleGraph* graph,
                                                      const vector<step_handle_t>& trav,
                                                      const pair<int64_t, int64_t>& trav_fragment);

/// Comparison of the above stats for the purposes of greedily selecting (highest better) traversals
bool rgfa_traversal_stats_less(const tuple<int64_t, int64_t, int64_t>& s1, const tuple<int64_t, int64_t, int64_t>& s2);
                                                  
/// Extract rGFA tags from minigraph GFA in order to pass in as hints above
unordered_map<const string&, vector<pair<int64_t, int64_t>>>  extract_rgfa_intervals(const string& rgfa_path);


}

#endif
