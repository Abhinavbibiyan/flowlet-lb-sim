#pragma once
#include "topology.h"
#include "flow.h"
#include "ecmp_router.h"
#include "flowlet_switcher.h"
#include "traffic_gen.h"
#include <vector>
#include <string>

struct SimStats {
    std::string mode;          // "ECMP" or "Flowlet"

    // Link utilization
    double avg_link_util;      // 0..1
    double max_link_util;
    double min_link_util;
    double imbalance_ratio;    // max/min util ratio (1.0 = perfect balance)
    double std_dev_util;

    // FCT
    double avg_fct_ms;
    double p50_fct_ms;
    double p95_fct_ms;
    double p99_fct_ms;

    // Flows
    int    total_flows;
    int    completed_flows;
    double throughput_gbps;

    // Routing
    uint64_t hash_collisions;  // ECMP only
    uint64_t flowlet_switches; // Flowlet only

    // Per-link data for export
    std::vector<double> link_utils;
    std::vector<double> fct_values;
};

class Simulator {
public:
    Simulator(const TrafficConfig& tcfg,
              int spines, int leaves, int hosts_per_leaf,
              double link_gbps);

    // Run both ECMP and Flowlet simulations
    SimStats run_ecmp();
    SimStats run_flowlet(double gap_us = 150.0);

    // Export results to CSV files in data/
    void export_csv(const SimStats& ecmp_stats,
                    const SimStats& flowlet_stats,
                    const std::string& out_dir = "data");

private:
    TrafficConfig    tcfg_;
    Topology         topo_;
    std::vector<Flow> flows_;

    SimStats simulate(std::vector<Flow> flows_copy, bool use_flowlet, double gap_us);
    SimStats compute_stats(const std::string& mode,
                           const std::vector<Flow>& flows,
                           const Topology& topo_snap,
                           uint64_t collisions,
                           uint64_t switches);
};
