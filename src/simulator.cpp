#include "simulator.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

Simulator::Simulator(const TrafficConfig& tcfg,
                     int spines, int leaves, int hpl, double link_gbps)
    : tcfg_(tcfg),
      topo_(spines, leaves, hpl, link_gbps)
{
    TrafficGenerator gen(tcfg_);
    flows_ = gen.generate();
    std::cout << "Generated " << flows_.size() << " flows\n";
}

// ── Packet-level discrete-event simulation ──────────────────────────────────
SimStats Simulator::simulate(std::vector<Flow> flows, bool use_flowlet, double gap_us) {
    Topology topo_copy = topo_;
    topo_copy.reset_utilization();

    ECMPRouter      ecmp(topo_copy);
    FlowletSwitcher fls(topo_copy, gap_us);

    const uint32_t MTU      = 1500;
    const double   link_bps = topo_copy.link_capacity_gbps * 1e9;
    double ip_gap = (MTU * 8.0) / link_bps;

    std::sort(flows.begin(), flows.end(),
              [](const Flow& a, const Flow& b){
                  return a.start_time < b.start_time;
              });

    const double window_sec = 1e-3;

    for (auto& flow : flows) {
        uint64_t remaining      = flow.size_bytes;
        double   t              = flow.start_time;
        bool     first          = true;
        const uint64_t burst_bytes = 65536;
        uint64_t burst_remaining   = burst_bytes;

        while (remaining > 0) {
            uint32_t pkt_size = (uint32_t)std::min((uint64_t)MTU, remaining);

            Packet pkt;
            pkt.flow_id    = flow.id;
            pkt.key        = flow.key;
            pkt.size_bytes = pkt_size;
            pkt.timestamp  = t;
            pkt.src_leaf   = flow.src_leaf;
            pkt.dst_leaf   = flow.dst_leaf;

            int spine;
            if (use_flowlet) {
                spine = fls.route_packet(pkt, topo_copy);
            } else {
                spine = ecmp.route_packet(pkt, topo_copy);
            }
            pkt.spine = spine;

            if (first) { flow.first_packet = t; first = false; }
            flow.last_packet = t;

            remaining       -= pkt_size;
            flow.bytes_sent += pkt_size;
            burst_remaining  = (burst_remaining >= pkt_size)
                               ? burst_remaining - pkt_size : 0;

            t += ip_gap;

            if (burst_remaining == 0 && remaining > 0) {
                std::mt19937 rng2(flow.id * 1000003 + (uint32_t)(t * 1e9));
                std::exponential_distribution<double> ibg(1.0 / 200e-6);
                double gap = ibg(rng2);
                t += gap;
                burst_remaining = burst_bytes;

                int up_lnk = topo_copy.get_link_id(flow.src_leaf, spine);
                if (up_lnk >= 0) {
                    double cap_bytes_win = (link_bps / 8.0) * window_sec;
                    double util = topo_copy.links[up_lnk].utilization /
                                  (cap_bytes_win + 1.0);
                    if (util > 0.7) {
                        double queue_delay = (util - 0.7) * 5e-3;
                        t += queue_delay;
                    }
                }
            }
        }

        flow.end_time  = t + 4e-6;
        flow.completed = true;
    }

    return compute_stats(use_flowlet ? "Flowlet" : "ECMP",
                         flows, topo_copy,
                         ecmp.hash_collisions,
                         fls.flowlet_switches);
}

SimStats Simulator::compute_stats(const std::string& mode,
                                   const std::vector<Flow>& flows,
                                   const Topology& topo_snap,
                                   uint64_t collisions,
                                   uint64_t switches) {
    SimStats s;
    s.mode = mode;
    s.hash_collisions  = collisions;
    s.flowlet_switches = switches;

    // --- Link utilization ---
    // Normalise bytes to utilization fraction over sim_duration
    double win = tcfg_.sim_duration;
    double cap_bytes = topo_snap.link_capacity_gbps * 1e9 / 8.0 * win;

    std::vector<double> utils;
    for (const auto& lnk : topo_snap.links) {
        double u = lnk.bytes_sent / cap_bytes;
        utils.push_back(std::min(u, 1.0));
    }
    s.link_utils = utils;

    double sum = std::accumulate(utils.begin(), utils.end(), 0.0);
    s.avg_link_util = sum / utils.size();
    s.max_link_util = *std::max_element(utils.begin(), utils.end());
    s.min_link_util = *std::min_element(utils.begin(), utils.end());
    s.imbalance_ratio = (s.min_link_util > 1e-9)
                        ? s.max_link_util / s.min_link_util
                        : s.max_link_util * 100.0;

    double sq_sum = 0;
    for (double u : utils) sq_sum += (u - s.avg_link_util) * (u - s.avg_link_util);
    s.std_dev_util = std::sqrt(sq_sum / utils.size());

    // --- FCT ---
    std::vector<double> fcts;
    uint64_t total_bytes = 0;
    for (const auto& f : flows) {
        if (f.completed && f.fct() > 0) {
            fcts.push_back(f.fct() * 1000.0); // ms
            total_bytes += f.bytes_sent;
        }
    }

    s.total_flows     = (int)flows.size();
    s.completed_flows = (int)fcts.size();
    s.fct_values      = fcts;

    if (!fcts.empty()) {
        std::sort(fcts.begin(), fcts.end());
        s.avg_fct_ms = std::accumulate(fcts.begin(), fcts.end(), 0.0) / fcts.size();
        s.p50_fct_ms = fcts[fcts.size() * 50 / 100];
        s.p95_fct_ms = fcts[fcts.size() * 95 / 100];
        s.p99_fct_ms = fcts[fcts.size() * 99 / 100];
    }

    s.throughput_gbps = (total_bytes * 8.0) / (win * 1e9);
    return s;
}

SimStats Simulator::run_ecmp() {
    std::cout << "\n[ECMP] Running simulation...\n";
    auto stats = simulate(flows_, false, 0);
    return stats;
}

SimStats Simulator::run_flowlet(double gap_us) {
    std::cout << "[Flowlet] Running simulation (gap=" << gap_us << "μs)...\n";
    auto stats = simulate(flows_, true, gap_us);
    return stats;
}

void Simulator::export_csv(const SimStats& ecmp,
                            const SimStats& flowlet,
                            const std::string& out_dir) {
    // 1. Summary CSV
    {
        std::ofstream f(out_dir + "/summary.csv");
        f << "metric,ECMP,Flowlet\n";
        auto fmt = [](double v) {
            std::ostringstream s; s << std::fixed << std::setprecision(4) << v;
            return s.str();
        };
        f << "avg_link_util,"     << fmt(ecmp.avg_link_util)    << "," << fmt(flowlet.avg_link_util)    << "\n";
        f << "max_link_util,"     << fmt(ecmp.max_link_util)    << "," << fmt(flowlet.max_link_util)    << "\n";
        f << "min_link_util,"     << fmt(ecmp.min_link_util)    << "," << fmt(flowlet.min_link_util)    << "\n";
        f << "imbalance_ratio,"   << fmt(ecmp.imbalance_ratio)  << "," << fmt(flowlet.imbalance_ratio)  << "\n";
        f << "std_dev_util,"      << fmt(ecmp.std_dev_util)     << "," << fmt(flowlet.std_dev_util)     << "\n";
        f << "avg_fct_ms,"        << fmt(ecmp.avg_fct_ms)       << "," << fmt(flowlet.avg_fct_ms)       << "\n";
        f << "p50_fct_ms,"        << fmt(ecmp.p50_fct_ms)       << "," << fmt(flowlet.p50_fct_ms)       << "\n";
        f << "p95_fct_ms,"        << fmt(ecmp.p95_fct_ms)       << "," << fmt(flowlet.p95_fct_ms)       << "\n";
        f << "p99_fct_ms,"        << fmt(ecmp.p99_fct_ms)       << "," << fmt(flowlet.p99_fct_ms)       << "\n";
        f << "throughput_gbps,"   << fmt(ecmp.throughput_gbps)  << "," << fmt(flowlet.throughput_gbps)  << "\n";
        f << "hash_collisions,"   << ecmp.hash_collisions       << "," << flowlet.hash_collisions       << "\n";
        f << "flowlet_switches,"  << ecmp.flowlet_switches      << "," << flowlet.flowlet_switches      << "\n";
        f << "completed_flows,"   << ecmp.completed_flows       << "," << flowlet.completed_flows       << "\n";
    }

    // 2. Per-link utilization CSV
    {
        std::ofstream f(out_dir + "/link_utils.csv");
        f << "link_id,ECMP,Flowlet\n";
        size_t n = std::max(ecmp.link_utils.size(), flowlet.link_utils.size());
        for (size_t i = 0; i < n; i++) {
            double eu = (i < ecmp.link_utils.size())    ? ecmp.link_utils[i]    : 0.0;
            double fu = (i < flowlet.link_utils.size()) ? flowlet.link_utils[i] : 0.0;
            f << i << "," << eu << "," << fu << "\n";
        }
    }

    // 3. FCT distribution CSV
    {
        std::ofstream f(out_dir + "/fct_dist.csv");
        f << "mode,fct_ms\n";
        for (double v : ecmp.fct_values)    f << "ECMP,"    << v << "\n";
        for (double v : flowlet.fct_values) f << "Flowlet," << v << "\n";
    }

    std::cout << "\n[Export] CSVs written to " << out_dir << "/\n";
}
