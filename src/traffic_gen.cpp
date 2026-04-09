#include "traffic_gen.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>

TrafficGenerator::TrafficGenerator(const TrafficConfig& cfg)
    : cfg_(cfg),
      rng_(cfg.seed),
      inter_arrival_(cfg.arrival_rate_fps),
      src_dist_(0, cfg.num_leaves - 1),
      dst_dist_(0, cfg.num_leaves - 1),
      port_dist_(1024, 65535)
{}

double TrafficGenerator::pareto_sample(double x_min, double alpha) {
    std::uniform_real_distribution<double> u(0.0, 1.0);
    double r = u(rng_);
    // Inverse CDF of Pareto: x_min / (1-U)^(1/alpha)
    return x_min / std::pow(1.0 - r, 1.0 / alpha);
}

uint32_t TrafficGenerator::leaf_to_ip(int leaf, int host) const {
    // 10.leaf.host.1
    return (10u << 24) | ((uint32_t)leaf << 16) | ((uint32_t)host << 8) | 1u;
}

uint64_t TrafficGenerator::sample_flow_size() {
    // Pareto with alpha=1.2 gives heavy tail (few elephant flows, many mice)
    double sz = pareto_sample(1500.0, 1.2);  // min 1 packet
    // Cap at 100 MB to keep simulation manageable
    sz = std::min(sz, 100.0 * 1024.0 * 1024.0);
    return (uint64_t)sz;
}

std::vector<Flow> TrafficGenerator::generate() {
    std::vector<Flow> flows;
    flows.reserve(cfg_.num_flows);

    double t = 0.0;
    std::uniform_int_distribution<int> host_dist(0, cfg_.hosts_per_leaf - 1);

    for (int i = 0; i < cfg_.num_flows; i++) {
        Flow f;
        f.id         = i;
        f.start_time = t;
        f.bytes_sent = 0;
        f.completed  = false;

        // Pick source and destination leaves (must differ)
        int src_leaf, dst_leaf;
        do {
            src_leaf = src_dist_(rng_);
            dst_leaf = dst_dist_(rng_);
        } while (src_leaf == dst_leaf);

        int src_host = host_dist(rng_);
        int dst_host = host_dist(rng_);

        f.src_leaf = src_leaf;
        f.dst_leaf = dst_leaf;

        f.key.src_ip   = leaf_to_ip(src_leaf, src_host);
        f.key.dst_ip   = leaf_to_ip(dst_leaf, dst_host);
        f.key.src_port = port_dist_(rng_);
        f.key.dst_port = port_dist_(rng_);
        f.key.proto    = 6; // TCP

        f.size_bytes   = sample_flow_size();

        flows.push_back(f);

        // Next arrival time (Poisson process)
        t += inter_arrival_(rng_);
        if (t > cfg_.sim_duration) break;
    }

    // Sort by start time
    std::sort(flows.begin(), flows.end(),
              [](const Flow& a, const Flow& b) {
                  return a.start_time < b.start_time;
              });

    return flows;
}
