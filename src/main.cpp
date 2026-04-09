#include "simulator.h"
#include "topology.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>

// ── ASCII bar chart helpers ─────────────────────────────────────────────────
static void print_bar(const std::string& label, double value, double max_val,
                      int width = 40, const std::string& color = "") {
    int filled = (max_val > 0) ? (int)(value / max_val * width) : 0;
    filled = std::min(filled, width);
    std::cout << std::left << std::setw(22) << label << " │";
    if (!color.empty()) std::cout << color;
    for (int i = 0; i < filled; i++) std::cout << "█";
    if (!color.empty()) std::cout << "\033[0m";
    for (int i = filled; i < width; i++) std::cout << "░";
    std::cout << "│ " << std::fixed << std::setprecision(4) << value << "\n";
}

static void print_separator(int width = 70) {
    std::cout << std::string(width, '-') << "\n";
}

static void print_header(const std::string& title) {
    std::cout << "\n";
    print_separator();
    std::cout << "  " << title << "\n";
    print_separator();
}

static void print_comparison_row(const std::string& metric,
                                  double ecmp_val, double flowlet_val,
                                  const std::string& unit = "",
                                  bool lower_is_better = false) {
    std::string winner_tag = "";
    std::string ecmp_color = "", flowlet_color = "";
    bool flowlet_wins = lower_is_better
                        ? (flowlet_val < ecmp_val)
                        : (flowlet_val > ecmp_val);
    if (flowlet_wins) {
        flowlet_color = "\033[32m"; // green
        ecmp_color    = "\033[31m"; // red
        winner_tag    = " ← better";
    } else {
        ecmp_color    = "\033[32m";
        flowlet_color = "\033[31m";
        winner_tag    = "";
    }
    std::cout << std::left  << std::setw(24) << metric
              << "  ECMP: "    << ecmp_color
              << std::right << std::setw(10) << std::fixed << std::setprecision(4)
              << ecmp_val << unit << "\033[0m"
              << "   Flowlet: " << flowlet_color
              << std::right << std::setw(10) << flowlet_val << unit << "\033[0m"
              << winner_tag << "\n";
}

// ── Link utilization bar chart ───────────────────────────────────────────────
static void print_link_chart(const SimStats& ecmp, const SimStats& flowlet,
                               const Topology& topo) {
    print_header("PER-LINK UTILIZATION (0.0 = idle, 1.0 = saturated)");
    std::cout << "\n  \033[31m[ECMP]\033[0m\n";
    for (size_t i = 0; i < ecmp.link_utils.size() && i < 16; i++) {
        auto& lnk = topo.links[i];
        std::string label = "L" + std::to_string(lnk.src) +
                            "→S" + std::to_string(lnk.dst);
        print_bar(label, ecmp.link_utils[i], 1.0, 40, "\033[31m");
    }
    std::cout << "\n  \033[32m[Flowlet]\033[0m\n";
    for (size_t i = 0; i < flowlet.link_utils.size() && i < 16; i++) {
        auto& lnk = topo.links[i];
        std::string label = "L" + std::to_string(lnk.src) +
                            "→S" + std::to_string(lnk.dst);
        print_bar(label, flowlet.link_utils[i], 1.0, 40, "\033[32m");
    }
}

// ── FCT distribution mini-histogram ─────────────────────────────────────────
static void print_fct_histogram(const std::vector<double>& fcts,
                                  const std::string& label,
                                  const std::string& color) {
    if (fcts.empty()) return;
    // 8 buckets
    double mn = *std::min_element(fcts.begin(), fcts.end());
    double mx = *std::max_element(fcts.begin(), fcts.end());
    if (mx - mn < 1e-9) mx = mn + 1.0;
    int nb = 8;
    std::vector<int> buckets(nb, 0);
    for (double v : fcts) {
        int b = (int)((v - mn) / (mx - mn) * (nb - 1));
        b = std::max(0, std::min(nb - 1, b));
        buckets[b]++;
    }
    int max_b = *std::max_element(buckets.begin(), buckets.end());
    std::cout << "\n  " << color << label << "\033[0m\n";
    for (int b = 0; b < nb; b++) {
        double lo = mn + (mx - mn) * b / nb;
        double hi = mn + (mx - mn) * (b + 1) / nb;
        std::ostringstream lbl;
        lbl << std::fixed << std::setprecision(2) << lo << "-" << hi << "ms";
        int bars = (max_b > 0) ? (buckets[b] * 20 / max_b) : 0;
        std::cout << "  " << std::left << std::setw(20) << lbl.str()
                  << " " << color;
        for (int k = 0; k < bars; k++) std::cout << "▪";
        std::cout << "\033[0m (" << buckets[b] << ")\n";
    }
}

// ── Main dashboard ───────────────────────────────────────────────────────────
static void print_dashboard(const SimStats& ecmp, const SimStats& flowlet,
                              const Topology& topo) {
    std::cout << "\n\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     FLOWLET LOAD BALANCER SIMULATOR  —  RESULTS DASHBOARD       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";

    // ── Topology info
    print_header("TOPOLOGY");
    std::cout << "  Spines: " << topo.num_spines
              << "  |  Leaves: " << topo.num_leaves
              << "  |  Hosts/Leaf: " << topo.hosts_per_leaf
              << "  |  Link: " << topo.link_capacity_gbps << " Gbps\n";
    std::cout << "  ECMP paths per flow: " << topo.num_spines
              << "  |  Total links: " << topo.links.size() << "\n";

    // ── Summary comparison
    print_header("PERFORMANCE COMPARISON");
    print_comparison_row("Avg Link Util",   ecmp.avg_link_util,   flowlet.avg_link_util,   "", false);
    print_comparison_row("Max Link Util",   ecmp.max_link_util,   flowlet.max_link_util,   "", false);
    print_comparison_row("Imbalance Ratio", ecmp.imbalance_ratio, flowlet.imbalance_ratio, "x", true);
    print_comparison_row("StdDev Util",     ecmp.std_dev_util,    flowlet.std_dev_util,    "", true);
    print_separator();
    print_comparison_row("Avg FCT",         ecmp.avg_fct_ms,      flowlet.avg_fct_ms,      " ms", true);
    print_comparison_row("p50 FCT",         ecmp.p50_fct_ms,      flowlet.p50_fct_ms,      " ms", true);
    print_comparison_row("p95 FCT",         ecmp.p95_fct_ms,      flowlet.p95_fct_ms,      " ms", true);
    print_comparison_row("p99 FCT",         ecmp.p99_fct_ms,      flowlet.p99_fct_ms,      " ms", true);
    print_separator();
    print_comparison_row("Throughput",      ecmp.throughput_gbps, flowlet.throughput_gbps, " Gbps", false);
    print_separator();

    std::cout << "  Hash collisions (ECMP):    " << ecmp.hash_collisions  << "\n";
    std::cout << "  Flowlet switches:          " << flowlet.flowlet_switches << "\n";
    std::cout << "  Completed flows (ECMP):    " << ecmp.completed_flows  << "/" << ecmp.total_flows    << "\n";
    std::cout << "  Completed flows (Flowlet): " << flowlet.completed_flows << "/" << flowlet.total_flows  << "\n";

    // ── Improvement summary
    double imb_improvement = (ecmp.imbalance_ratio > 0)
        ? (ecmp.imbalance_ratio - flowlet.imbalance_ratio) / ecmp.imbalance_ratio * 100.0
        : 0.0;
    double fct_improvement = (ecmp.avg_fct_ms > 0)
        ? (ecmp.avg_fct_ms - flowlet.avg_fct_ms) / ecmp.avg_fct_ms * 100.0
        : 0.0;

    print_header("IMPROVEMENT SUMMARY");
    std::cout << "\033[32m";
    std::cout << "  Load imbalance reduction:  " << std::fixed << std::setprecision(1)
              << imb_improvement << "%\n";
    std::cout << "  Avg FCT improvement:       " << fct_improvement << "%\n";
    std::cout << "\033[0m";

    // ── Link utilization bars
    print_link_chart(ecmp, flowlet, topo);

    // ── FCT histograms
    print_header("FCT DISTRIBUTION");
    print_fct_histogram(ecmp.fct_values,     "[ECMP]",    "\033[31m");
    print_fct_histogram(flowlet.fct_values,  "[Flowlet]", "\033[32m");

    print_separator();
    std::cout << "  CSV data exported to data/  |  Run: python3 scripts/plot.py\n";
    print_separator();
    std::cout << "\n";
}

// ── Argument parsing ─────────────────────────────────────────────────────────
struct CLIArgs {
    int    spines         = 4;
    int    leaves         = 4;
    int    hosts_per_leaf = 4;
    double link_gbps      = 10.0;
    int    num_flows      = 300;
    double load           = 0.6;
    double sim_duration   = 0.05;
    double gap_us         = 150.0;
    uint32_t seed         = 42;
};

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "  --spines N          Number of spine switches (default 4)\n"
              << "  --leaves N          Number of leaf switches  (default 4)\n"
              << "  --hosts N           Hosts per leaf           (default 4)\n"
              << "  --link-gbps F       Link capacity in Gbps    (default 10)\n"
              << "  --flows N           Number of flows          (default 300)\n"
              << "  --load F            Load factor 0..1         (default 0.6)\n"
              << "  --duration F        Sim duration seconds     (default 0.05)\n"
              << "  --gap-us F          Flowlet gap threshold μs (default 150)\n"
              << "  --seed N            RNG seed                 (default 42)\n"
              << "  --help              Show this message\n";
}

static CLIArgs parse_args(int argc, char* argv[]) {
    CLIArgs a;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") { print_usage(argv[0]); exit(0); }
#define NEXT_INT(field)   if (i+1 < argc) { a.field = std::stoi(argv[++i]); }
#define NEXT_FLOAT(field) if (i+1 < argc) { a.field = std::stod(argv[++i]); }
        else if (arg == "--spines")   { NEXT_INT(spines) }
        else if (arg == "--leaves")   { NEXT_INT(leaves) }
        else if (arg == "--hosts")    { NEXT_INT(hosts_per_leaf) }
        else if (arg == "--link-gbps"){ NEXT_FLOAT(link_gbps) }
        else if (arg == "--flows")    { NEXT_INT(num_flows) }
        else if (arg == "--load")     { NEXT_FLOAT(load) }
        else if (arg == "--duration") { NEXT_FLOAT(sim_duration) }
        else if (arg == "--gap-us")   { NEXT_FLOAT(gap_us) }
        else if (arg == "--seed")     { NEXT_INT(seed) }
#undef NEXT_INT
#undef NEXT_FLOAT
    }
    return a;
}

// ── Entry point ──────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    CLIArgs cli = parse_args(argc, argv);

    TrafficConfig tcfg;
    tcfg.num_flows      = cli.num_flows;
    tcfg.load_factor    = cli.load;
    tcfg.link_gbps      = cli.link_gbps;
    tcfg.num_leaves     = cli.leaves;
    tcfg.hosts_per_leaf = cli.hosts_per_leaf;
    tcfg.sim_duration   = cli.sim_duration;
    tcfg.arrival_rate_fps = cli.num_flows / cli.sim_duration;
    tcfg.seed           = cli.seed;

    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║      Flowlet Load Balancer Simulator v1.0            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n\n";
    std::cout << "Config: " << cli.spines << " spines, " << cli.leaves
              << " leaves, " << cli.hosts_per_leaf << " hosts/leaf, "
              << cli.link_gbps << " Gbps links\n";
    std::cout << "Flows: " << cli.num_flows << "  Load: " << cli.load
              << "  Duration: " << cli.sim_duration << "s  Gap: "
              << cli.gap_us << "μs\n";

    Simulator sim(tcfg, cli.spines, cli.leaves, cli.hosts_per_leaf, cli.link_gbps);

    SimStats ecmp_stats    = sim.run_ecmp();
    SimStats flowlet_stats = sim.run_flowlet(cli.gap_us);

    // Print ASCII dashboard
    Topology display_topo(cli.spines, cli.leaves, cli.hosts_per_leaf, cli.link_gbps);
    print_dashboard(ecmp_stats, flowlet_stats, display_topo);

    // Export CSVs
    sim.export_csv(ecmp_stats, flowlet_stats, "data");

    return 0;
}
