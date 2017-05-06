#include "position.hpp"

namespace vg {

pos_t make_pos_t(const Position& pos) {
    return make_tuple(pos.node_id(), pos.is_reverse(), pos.offset());
}

pos_t make_pos_t(id_t id, bool is_rev, off_t off) {
    return make_tuple(id, is_rev, off);
}
    
pos_t make_pos_t(gcsa::node_type node) {
    return make_tuple(gcsa::Node::id(node), gcsa::Node::rc(node), gcsa::Node::offset(node));
}
    
Position make_position(const pos_t& pos) {
    Position p;
    p.set_node_id(id(pos));
    p.set_is_reverse(is_rev(pos));
    p.set_offset(offset(pos));
    return p;
}

Position make_position(id_t id, bool is_rev, off_t off) {
    Position p;
    p.set_node_id(id);
    p.set_is_reverse(is_rev);
    p.set_offset(off);
    return p;
}
    
Position make_position(gcsa::node_type node) {
    Position p;
    p.set_node_id(gcsa::Node::id(node));
    p.set_is_reverse(gcsa::Node::rc(node));
    p.set_offset(gcsa::Node::offset(node));
    return p;
}

bool is_empty(const pos_t& pos) {
    return id(pos) == 0;
}

id_t id(const pos_t& pos) {
    return get<0>(pos);
}

bool is_rev(const pos_t& pos) {
    return get<1>(pos);
}

off_t offset(const pos_t& pos) {
    return get<2>(pos);
}

id_t& get_id(pos_t& pos) {
    return get<0>(pos);
}

bool& get_is_rev(pos_t& pos) {
    return get<1>(pos);
}

off_t& get_offset(pos_t& pos) {
    return get<2>(pos);
}

pos_t reverse(const pos_t& pos, size_t node_length) {
    pos_t rev = pos;
    // swap the offset onto the other strand
    get_offset(rev) = node_length - offset(rev);
    // invert the position
    get_is_rev(rev) = !is_rev(rev);
    return rev;
}

Position reverse(const Position& pos, size_t node_length) {
    auto p = pos;
    p.set_offset(node_length - pos.offset());
    p.set_is_reverse(!pos.is_reverse());
    return p;
}

ostream& operator<<(ostream& out, const pos_t& pos) {
    return out << id(pos) << (is_rev(pos) ? "-" : "+") << offset(pos);
}

Node xg_cached_node(id_t id, xg::XG* xgidx, LRUCache<id_t, Node>& node_cache) {
    pair<Node, bool> cached = node_cache.retrieve(id);
    if(!cached.second) {
        cached.first = xgidx->node(id);
        node_cache.put(id, cached.first);
    }
    Node& node = cached.first;
    return node;
}

vector<Edge> xg_cached_edges_of(id_t id, xg::XG* xgidx, LRUCache<id_t, vector<Edge> >& edge_cache) {
    pair<vector<Edge>, bool> cached = edge_cache.retrieve(id);
    if(!cached.second) {
        for (auto& edge : xgidx->edges_of(id)) {
            cached.first.push_back(edge);
        }
        edge_cache.put(id, cached.first);
    }
    return cached.first;
}

string xg_cached_node_sequence(id_t id, xg::XG* xgidx, LRUCache<id_t, Node>& node_cache) {
    pair<Node, bool> cached = node_cache.retrieve(id);
    if(!cached.second) {
        cached.first = xgidx->node(id);
        node_cache.put(id, cached.first);
    }
    Node& node = cached.first;
    return node.sequence();
}

size_t xg_cached_node_length(id_t id, xg::XG* xgidx, LRUCache<id_t, Node>& node_cache) {
    pair<Node, bool> cached = node_cache.retrieve(id);
    if(!cached.second) {
        cached.first = xgidx->node(id);
        node_cache.put(id, cached.first);
    }
    Node& node = cached.first;
    return node.sequence().size();
}

size_t xg_cached_node_start(id_t id, xg::XG* xgidx, LRUCache<id_t, size_t>& node_start_cache) {
    pair<size_t, bool> cached = node_start_cache.retrieve(id);
    if(!cached.second) {
        cached.first = xgidx->node_start(id);
        node_start_cache.put(id, cached.first);
    }
    return cached.first;
}

char xg_cached_pos_char(pos_t pos, xg::XG* xgidx, LRUCache<id_t, Node>& node_cache) {
    pair<Node, bool> cached = node_cache.retrieve(id(pos));
    if(!cached.second) {
        // If it's not in the cache, put it in
        cached.first = xgidx->node(id(pos));
        node_cache.put(id(pos), cached.first);
    }
    Node& node = cached.first;
    if (is_rev(pos)) {
        /*
        cerr << "reversed... " << endl;
        cerr << "rev pos " << offset(reverse(pos, node.sequence().size())) << endl;
        cerr << "seq is " << node.sequence() << " and got " <<
            reverse_complement(node.sequence()[offset(reverse(pos, node.sequence().size()))-1]) << endl;
        */
        return reverse_complement(node.sequence()[offset(reverse(pos, node.sequence().size()))-1]);
    } else {
        /*
        cerr << "forward... " << endl;
        cerr << "seq is " << node.sequence() << " and got " << node.sequence().at(offset(pos)) << endl;
        */
        return node.sequence().at(offset(pos));
    }
}

map<pos_t, char> xg_cached_next_pos_chars(pos_t pos, xg::XG* xgidx, LRUCache<id_t, Node>& node_cache, LRUCache<id_t, vector<Edge> >& edge_cache) {

    map<pos_t, char> nexts;
    // See if the node is cached (did we just visit it?)
    pair<Node, bool> cached = node_cache.retrieve(id(pos));
    if(!cached.second) {
        // If it's not in the cache, put it in
        cached.first = xgidx->node(id(pos));
        node_cache.put(id(pos), cached.first);
    }
    Node& node = cached.first;
    // if we are still in the node, return the next position and character
    if (offset(pos) < node.sequence().size()-1) {
        ++get_offset(pos);
        nexts[pos] = xg_cached_pos_char(pos, xgidx, node_cache);
    } else {
        // helper
        auto is_inverting = [](const Edge& e) {
            return !(e.from_start() == e.to_end())
            && (e.from_start() || e.to_end());
        };
        // check our cache
        pair<vector<Edge>, bool> cached = edge_cache.retrieve(id(pos));
        if(!cached.second) {
            // If it's not in the cache, put it in
            for (auto& edge : xgidx->edges_of(id(pos))) {
                cached.first.push_back(edge);
            }
            edge_cache.put(id(pos), cached.first);
        }
        auto& edges = cached.first;
        // look at the next positions we could reach
        if (!is_rev(pos)) {
            // we are on the forward strand, the next things from this node come off the end
            for (auto& edge : edges) {
                if((edge.to() == id(pos) && edge.to_end()) || (edge.from() == id(pos) && !edge.from_start())) {
                    id_t nid = (edge.from() == id(pos) ?
                                edge.to()
                                : edge.from());
                    pos_t p = make_pos_t(nid, is_inverting(edge), 0);
                    nexts[p] = xg_cached_pos_char(p, xgidx, node_cache);
                }
            }
        } else {
            // we are on the reverse strand, the next things from this node come off the start
            for (auto& edge : edges) {
                if((edge.to() == id(pos) && !edge.to_end()) || (edge.from() == id(pos) && edge.from_start())) {
                    id_t nid = (edge.to() == id(pos) ?
                                edge.from()
                                : edge.to());
                    pos_t p = make_pos_t(nid, !is_inverting(edge), 0);
                    nexts[p] = xg_cached_pos_char(p, xgidx, node_cache);
                }
            }
        }
    }
    return nexts;
}

set<pos_t> xg_cached_next_pos(pos_t pos, bool whole_node, xg::XG* xgidx, LRUCache<id_t, Node>& node_cache, LRUCache<id_t, vector<Edge> >& edge_cache) {
    set<pos_t> nexts;
    // See if the node is cached (did we just visit it?)
    pair<Node, bool> cached = node_cache.retrieve(id(pos));
    if(!cached.second) {
        // If it's not in the cache, put it in
        cached.first = xgidx->node(id(pos));
        node_cache.put(id(pos), cached.first);
    }
    Node& node = cached.first;
    // if we are still in the node, return the next position and character
    if (!whole_node && offset(pos) < node.sequence().size()-1) {
        ++get_offset(pos);
        nexts.insert(pos);
    } else {
        // helper
        auto is_inverting = [](const Edge& e) {
            return !(e.from_start() == e.to_end())
            && (e.from_start() || e.to_end());
        };
        // check our cache
        pair<vector<Edge>, bool> cached = edge_cache.retrieve(id(pos));
        if(!cached.second) {
            // If it's not in the cache, put it in
            for (auto& edge : xgidx->edges_of(id(pos))) {
                cached.first.push_back(edge);
            }
            edge_cache.put(id(pos), cached.first);
        }
        auto& edges = cached.first;
        // look at the next positions we could reach
        if (!is_rev(pos)) {
            // we are on the forward strand, the next things from this node come off the end
            for (auto& edge : edges) {
                if((edge.to() == id(pos) && edge.to_end()) || (edge.from() == id(pos) && !edge.from_start())) {
                    id_t nid = (edge.from() == id(pos) ?
                                edge.to()
                                : edge.from());
                    nexts.insert(make_pos_t(nid, is_inverting(edge), 0));
                }
            }
        } else {
            // we are on the reverse strand, the next things from this node come off the start
            for (auto& edge : edges) {
                if((edge.to() == id(pos) && !edge.to_end()) || (edge.from() == id(pos) && edge.from_start())) {
                    id_t nid = (edge.to() == id(pos) ?
                                edge.from()
                                : edge.to());
                    nexts.insert(make_pos_t(nid, !is_inverting(edge), 0));
                }
            }
        }
    }
    return nexts;
}

int xg_cached_distance(pos_t pos1, pos_t pos2, int maximum, xg::XG* xgidx, LRUCache<id_t, Node>& node_cache, LRUCache<id_t, vector<Edge> >& edge_cache) {
    //cerr << "distance from " << pos1 << " to " << pos2 << endl;
    if (pos1 == pos2) return 0;
    int adj = (offset(pos1) == xg_cached_node_length(id(pos1), xgidx, node_cache) ? 0 : 1);
    set<pos_t> seen;
    set<pos_t> nexts = xg_cached_next_pos(pos1, false, xgidx, node_cache, edge_cache);
    int distance = 0;
    while (!nexts.empty()) {
        set<pos_t> todo;
        for (auto& next : nexts) {
            if (!seen.count(next)) {
                seen.insert(next);
                if (next == pos2) {
                    return distance+adj;
                }
                // handle the edge case that we are looking for the position after the end of this node
                if (make_pos_t(id(next), is_rev(next), offset(next)+1) == pos2) {
                    return distance+adj+1;
                }
                for (auto& x : xg_cached_next_pos(next, false, xgidx, node_cache, edge_cache)) {
                    todo.insert(x);
                }
            }
        }
        if (distance == maximum) {
            break;
        }
        nexts = todo;
        ++distance;
    }
    return maximum;
}

set<pos_t> xg_cached_positions_bp_from(pos_t pos, int distance, bool rev, xg::XG* xgidx, LRUCache<id_t, Node>& node_cache, LRUCache<id_t, vector<Edge> >& edge_cache) {
    // handle base case
    //size_t xg_cached_node_length(id_t id, xg::XG* xgidx, LRUCache<id_t, Node>& node_cache);
    if (rev) {
        pos = reverse(pos, xg_cached_node_length(id(pos), xgidx, node_cache));
    }
    set<pos_t> positions;
    if (distance == 0) {
        positions.insert(pos);
        //return positions;
    } else {
        set<pos_t> seen;
        set<pos_t> nexts = xg_cached_next_pos(pos, false, xgidx, node_cache, edge_cache);
        int walked = 0;
        while (!nexts.empty()) {
            if (walked+1 == distance) {
                for (auto& next : nexts) {
                    positions.insert(next);
                }
                break;
            }
            set<pos_t> todo;
            for (auto& next : nexts) {
                if (!seen.count(next)) {
                    seen.insert(next);
                    for (auto& x : xg_cached_next_pos(next, false, xgidx, node_cache, edge_cache)) {
                        todo.insert(x);
                    }
                }
            }
            nexts = todo;
            ++walked;
        }
    }
    if (rev) {
        set<pos_t> rev_pos;
        for (auto& p : positions) {
            rev_pos.insert(reverse(p, xg_cached_node_length(id(p), xgidx, node_cache)));
        }
        return rev_pos;
    } else {
        return positions;
    }
}


}
