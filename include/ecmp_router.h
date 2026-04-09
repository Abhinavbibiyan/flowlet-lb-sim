#pragma once
#include "flow.h"
#include "topology.h"
#include <unordered_map>
#include <vector>

class ECMPRouter {
public:
    explicit ECMPRouter(const Topology& topo);

    // Select a spine for this packet using 5-tuple hash
    int select_path(const FlowKey& key, int src_leaf, int dst_leaf) const;

    // Route a packet: updates link utilization, returns chosen spine
    int route_packet(const Packet& pkt, Topology& topo);

    // Stats
    uint64_t total_packets_routed = 0;
    uint64_t hash_collisions = 0;     // flows sharing same path

    void reset();

private:
    const Topology& topo_;

    // For collision detection: path -> list of flow ids using it
    std::unordered_map<uint64_t, std::vector<int>> path_flows_;
};
