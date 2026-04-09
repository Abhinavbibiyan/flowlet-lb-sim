#pragma once
#include "flow.h"
#include <vector>
#include <random>
#include <cstdint>

struct TrafficConfig {
    int    num_flows       = 200;
    double load_factor     = 0.6;      // fraction of total bisection BW
    double link_gbps       = 10.0;
    int    num_leaves      = 4;
    int    hosts_per_leaf  = 4;
    double sim_duration    = 0.1;      // seconds
    double mean_flow_size  = 100000;   // bytes (heavy-tail base)
    uint32_t seed          = 42;

    // Poisson arrival rate (flows/sec) — derived from load
    double arrival_rate_fps = 1000.0;
};

class TrafficGenerator {
public:
    explicit TrafficGenerator(const TrafficConfig& cfg);

    // Generate all flows for the simulation
    std::vector<Flow> generate();

    // Generate a single flow size using Pareto heavy-tail (realistic DC)
    uint64_t sample_flow_size();

private:
    TrafficConfig cfg_;
    std::mt19937_64 rng_;
    std::exponential_distribution<double> inter_arrival_;
    std::uniform_int_distribution<int>    src_dist_;
    std::uniform_int_distribution<int>    dst_dist_;
    std::uniform_int_distribution<uint16_t> port_dist_;

    // Pareto distribution for heavy-tail flow sizes
    double pareto_sample(double x_min, double alpha);

    uint32_t leaf_to_ip(int leaf, int host) const;
};
