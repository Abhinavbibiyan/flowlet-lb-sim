#include "flowlet_switcher.h"
#include <limits>
#include <cstdlib>

FlowletSwitcher::FlowletSwitcher(const Topology& topo, double gap_us)
    : topo_(topo), gap_threshold_sec_(gap_us * 1e-6)
{}

int FlowletSwitcher::least_loaded_spine(int src_leaf, int dst_leaf,
                                         const Topology& topo) const {
    const auto& spines = topo.get_spine_paths(src_leaf, dst_leaf);
    int    best_spine = spines[0];
    double min_load   = std::numeric_limits<double>::max();

    for (int sp : spines) {
        // Use uplink load as proxy
        int lnk = topo.get_link_id(src_leaf, sp);
        double load = (lnk >= 0) ? topo.links[lnk].utilization : 0.0;
        if (load < min_load) {
            min_load   = load;
            best_spine = sp;
        }
    }
    return best_spine;
}

int FlowletSwitcher::route_packet(const Packet& pkt, Topology& topo) {
    int spine = -1;
    auto it = state_.find(pkt.key);

    if (it == state_.end()) {
        // New flow — pick least loaded spine
        spine = least_loaded_spine(pkt.src_leaf, pkt.dst_leaf, topo);
        FlowletState fs;
        fs.current_spine    = spine;
        fs.last_packet_time = pkt.timestamp;
        fs.flowlet_id       = 0;
        state_[pkt.key]     = fs;
        new_flowlets++;
    } else {
        FlowletState& fs = it->second;
        double gap = pkt.timestamp - fs.last_packet_time;

        if (gap > gap_threshold_sec_) {
            // New flowlet — rebalance to least loaded path
            int new_spine = least_loaded_spine(pkt.src_leaf, pkt.dst_leaf, topo);
            if (new_spine != fs.current_spine) {
                flowlet_switches++;
            }
            fs.current_spine    = new_spine;
            fs.flowlet_id++;
            new_flowlets++;
        }
        fs.last_packet_time = pkt.timestamp;
        spine = fs.current_spine;
    }

    // Update link utilization
    int up_link = topo.get_link_id(pkt.src_leaf, spine);
    if (up_link >= 0) {
        topo.links[up_link].bytes_sent += pkt.size_bytes;
        topo.links[up_link].utilization += pkt.size_bytes;
    }
    int dn_link = topo.get_link_id(pkt.dst_leaf, spine);
    if (dn_link >= 0) {
        topo.links[dn_link].bytes_sent += pkt.size_bytes;
        topo.links[dn_link].utilization += pkt.size_bytes;
    }

    total_packets_routed++;
    return spine;
}

void FlowletSwitcher::reset() {
    total_packets_routed = 0;
    flowlet_switches     = 0;
    new_flowlets         = 0;
    state_.clear();
}
