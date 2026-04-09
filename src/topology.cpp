#include "topology.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>

Topology::Topology(int spines, int leaves, int hpl, double cap)
    : num_spines(spines), num_leaves(leaves),
      hosts_per_leaf(hpl), link_capacity_gbps(cap)
{
    build();
}

void Topology::build() {
    // Create spine nodes
    for (int s = 0; s < num_spines; s++) {
        Node n;
        n.id = s;
        n.name = "Spine" + std::to_string(s);
        n.is_spine = true;
        nodes.push_back(n);
    }
    // Create leaf nodes
    for (int l = 0; l < num_leaves; l++) {
        Node n;
        n.id = num_spines + l;
        n.name = "Leaf" + std::to_string(l);
        n.is_spine = false;
        nodes.push_back(n);
    }

    // Connect every leaf to every spine (full mesh uplinks)
    for (int l = 0; l < num_leaves; l++) {
        int leaf_node = leaf_id(l);
        adj[leaf_node]; // ensure entry exists
        for (int s = 0; s < num_spines; s++) {
            int spine_node = spine_id(s);
            // Leaf -> Spine
            int idx = (int)links.size();
            links.emplace_back(leaf_node, spine_node, link_capacity_gbps);
            adj[leaf_node].push_back(idx);
            adj[spine_node].push_back(idx);
            link_map[encode(leaf_node, spine_node)] = idx;
            link_map[encode(spine_node, leaf_node)] = idx;
        }
    }

    // Pre-compute paths: paths[src_leaf][dst_leaf] = vector of spine indices
    paths.resize(num_leaves, std::vector<std::vector<int>>(num_leaves));
    for (int s = 0; s < num_leaves; s++) {
        for (int d = 0; d < num_leaves; d++) {
            if (s == d) continue;
            for (int sp = 0; sp < num_spines; sp++) {
                paths[s][d].push_back(sp);
            }
        }
    }
}

const std::vector<int>& Topology::get_spine_paths(int src_leaf, int dst_leaf) const {
    return paths[src_leaf][dst_leaf];
}

int Topology::get_link_id(int leaf, int spine) const {
    int leaf_node  = leaf_id(leaf);
    int spine_node = spine_id(spine);
    auto it = link_map.find(encode(leaf_node, spine_node));
    if (it == link_map.end()) return -1;
    return it->second;
}

void Topology::reset_utilization() {
    for (auto& lnk : links) {
        lnk.utilization   = 0.0;
        lnk.bytes_sent    = 0;
        lnk.packets_dropped = 0;
    }
}

void Topology::print_topology() const {
    std::cout << "\n=== Spine-Leaf Topology ===\n";
    std::cout << "Spines: " << num_spines
              << "  Leaves: " << num_leaves
              << "  Hosts/Leaf: " << hosts_per_leaf
              << "  Link: " << link_capacity_gbps << " Gbps\n";
    std::cout << "Total links: " << links.size() << "\n";
    std::cout << "ECMP paths per flow: " << num_spines << "\n\n";
}
