#include "ecmp_router.h"
#include <functional>

ECMPRouter::ECMPRouter(const Topology& topo) : topo_(topo) {}

int ECMPRouter::select_path(const FlowKey& key, int src_leaf, int dst_leaf) const {
    const auto& spines = topo_.get_spine_paths(src_leaf, dst_leaf);
    if (spines.empty()) return -1;

    // Hash 5-tuple deterministically
    FlowKeyHash hasher;
    size_t h = hasher(key);
    return spines[h % spines.size()];
}

int ECMPRouter::route_packet(const Packet& pkt, Topology& topo) {
    int spine = select_path(pkt.key, pkt.src_leaf, pkt.dst_leaf);
    if (spine < 0) return -1;

    // Update uplink (src_leaf -> spine)
    int up_link = topo.get_link_id(pkt.src_leaf, spine);
    if (up_link >= 0) {
        topo.links[up_link].bytes_sent += pkt.size_bytes;
        topo.links[up_link].utilization += pkt.size_bytes;
    }
    // Update downlink (spine -> dst_leaf) — same link object in our model
    int dn_link = topo.get_link_id(pkt.dst_leaf, spine);
    if (dn_link >= 0) {
        topo.links[dn_link].bytes_sent += pkt.size_bytes;
        topo.links[dn_link].utilization += pkt.size_bytes;
    }

    total_packets_routed++;

    // Track collisions (multiple flows on same path between same src/dst)
    uint64_t path_key = ((uint64_t)pkt.src_leaf << 24) |
                        ((uint64_t)pkt.dst_leaf << 8) | spine;
    auto& flows_on_path = path_flows_[path_key];
    bool found = false;
    for (int fid : flows_on_path) {
        if (fid == pkt.flow_id) { found = true; break; }
    }
    if (!found) {
        if (!flows_on_path.empty()) hash_collisions++;
        flows_on_path.push_back(pkt.flow_id);
    }

    return spine;
}

void ECMPRouter::reset() {
    total_packets_routed = 0;
    hash_collisions = 0;
    path_flows_.clear();
}
