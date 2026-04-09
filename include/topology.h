#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

struct Link {
    int src;
    int dst;
    double capacity_gbps;
    double utilization;   // bytes in flight this window
    uint64_t bytes_sent;
    uint64_t packets_dropped;

    Link(int s, int d, double cap)
        : src(s), dst(d), capacity_gbps(cap),
          utilization(0.0), bytes_sent(0), packets_dropped(0) {}
};

struct Node {
    int id;
    std::string name;
    bool is_spine;   // true = spine, false = leaf/ToR
};

class Topology {
public:
    int num_spines;
    int num_leaves;
    int hosts_per_leaf;
    double link_capacity_gbps;

    std::vector<Node> nodes;
    std::vector<Link> links;

    // adjacency: node_id -> list of link indices
    std::unordered_map<int, std::vector<int>> adj;

    // paths[src_leaf][dst_leaf] = list of spine indices usable
    std::vector<std::vector<std::vector<int>>> paths;

    Topology(int spines, int leaves, int hosts_per_leaf, double cap_gbps);

    // Returns all ECMP paths (via different spines) between two leaf nodes
    const std::vector<int>& get_spine_paths(int src_leaf, int dst_leaf) const;

    // Get link index between leaf and spine
    int get_link_id(int leaf, int spine) const;

    int total_nodes() const { return num_spines + num_leaves; }
    int spine_id(int s) const { return s; }                    // 0..num_spines-1
    int leaf_id(int l)  const { return num_spines + l; }       // num_spines..

    void reset_utilization();
    void print_topology() const;

private:
    // link_map[(a,b)] = link index
    std::unordered_map<uint64_t, int> link_map;
    void build();
    uint64_t encode(int a, int b) const { return ((uint64_t)a << 32) | (uint32_t)b; }
};
