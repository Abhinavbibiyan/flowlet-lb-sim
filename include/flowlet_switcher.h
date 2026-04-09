#pragma once
#include "flow.h"
#include "topology.h"
#include <unordered_map>

struct FlowletState {
    int    current_spine;
    double last_packet_time;  // seconds
    int    flowlet_id;
};

class FlowletSwitcher {
public:
    // gap_threshold_us: idle gap (microseconds) that triggers a new flowlet
    FlowletSwitcher(const Topology& topo, double gap_threshold_us = 150.0);

    // Route a packet with flowlet-aware path selection
    // Returns chosen spine index
    int route_packet(const Packet& pkt, Topology& topo);

    // Stats
    uint64_t total_packets_routed = 0;
    uint64_t flowlet_switches      = 0;  // how many times path was changed
    uint64_t new_flowlets          = 0;

    void reset();
    void set_gap_threshold(double us) { gap_threshold_sec_ = us * 1e-6; }

private:
    const Topology& topo_;
    double gap_threshold_sec_;

    // Per-flow state
    std::unordered_map<FlowKey, FlowletState, FlowKeyHash> state_;

    // Choose least-loaded spine among available paths
    int least_loaded_spine(int src_leaf, int dst_leaf, const Topology& topo) const;
};
