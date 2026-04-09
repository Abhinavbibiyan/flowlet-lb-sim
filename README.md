# Flowlet Load Balancer Simulator

A **C++17 network simulator** that implements and rigorously compares **ECMP** (Equal-Cost Multi-Path, hash-based) versus **Flowlet Switching** (gap-based adaptive load balancing) across a spine-leaf datacenter topology.

Inspired by:
- **[NSDI 2025]** *Unlocking ECMP Programmability* — precise ECMP traffic control
- **[ENLB 2025, Wiley]** — ECMP wastes ~38% bandwidth that flowlet switching recovers
- **[CONGA, SIGCOMM 2014]** — congestion-aware load balancing
- **[Presto, SIGCOMM 2015]** — flowcell-based load balancing

---

## Architecture

```
flowlet_lb/
├── include/
│   ├── topology.h          # Spine-leaf graph (adjacency list)
│   ├── flow.h              # Flow/Packet structs, 5-tuple key
│   ├── ecmp_router.h       # 5-tuple hash → path selection
│   ├── flowlet_switcher.h  # Per-flow timer, gap detection, rebalance
│   ├── traffic_gen.h       # Poisson arrivals + Pareto heavy-tail flows
│   └── simulator.h         # Discrete-event engine + stats
├── src/
│   ├── topology.cpp
│   ├── ecmp_router.cpp
│   ├── flowlet_switcher.cpp
│   ├── traffic_gen.cpp
│   ├── simulator.cpp
│   └── main.cpp            # CLI + ASCII dashboard
├── scripts/
│   ├── plot.py             # Matplotlib publication plots → data/results.png
│   └── dashboard.py        # Self-contained HTML dashboard → data/dashboard.html
├── data/                   # Generated CSVs + dashboard output
├── Makefile
├── requirements.txt
└── setup.sh
```

## Quick Start (Kali Linux / Ubuntu)

```bash
# 1. Clone / enter project
git clone <your-repo>
cd flowlet_lb

# 2. One-shot setup (builds C++, creates Python venv, installs deps)
chmod +x setup.sh && ./setup.sh

# 3. Run simulation (default: 300 flows, 4 spines, 4 leaves, 10 Gbps)
make run

# 4. Generate HTML dashboard
source venv/bin/activate
python3 scripts/dashboard.py
xdg-open data/dashboard.html

# 5. Generate static plots (requires matplotlib)
python3 scripts/plot.py
```

## Build Only (no Python)

```bash
make          # compile
make run      # compile + run with default params
make clean    # remove build artifacts
```

## CLI Options

```
./bin/flowlet_sim [options]

  --spines N          Number of spine switches     (default: 4)
  --leaves N          Number of leaf switches       (default: 4)
  --hosts  N          Hosts per leaf switch         (default: 4)
  --link-gbps F       Link capacity in Gbps         (default: 10.0)
  --flows N           Number of flows to simulate   (default: 300)
  --load F            Traffic load factor 0..1      (default: 0.6)
  --duration F        Simulation duration (seconds) (default: 0.05)
  --gap-us F          Flowlet gap threshold (μs)    (default: 150)
  --seed N            RNG seed for reproducibility  (default: 42)
  --help
```

### Example: Heavy-load stress test

```bash
./bin/flowlet_sim \
  --flows 1000 --spines 8 --leaves 8 \
  --hosts 8 --load 0.85 --duration 0.1 \
  --gap-us 100
```

## Key Concepts Demonstrated

| Concept | ECMP | Flowlet |
|---|---|---|
| Path selection | 5-tuple hash (static) | Least-loaded spine on each flowlet |
| Hash collisions | Many (elephant flow problem) | N/A — adaptive |
| Load imbalance | High (2–3× ratio) | Low (≈1.3–1.6× ratio) |
| FCT tail latency | Elevated (hot-link queuing) | Reduced |
| Bandwidth waste | ~38% (ENLB 2025) | Recovered |

### What is a Flowlet?
A **flowlet** is a burst of packets in the same TCP flow separated by an idle gap ≥ threshold (default: 150 μs). When a new flowlet starts, the switch picks the **least-loaded** uplink instead of re-hashing the 5-tuple. This gives per-flowlet load balancing without reordering within a flowlet.

## Traffic Model

- **Arrival process**: Poisson (inter-arrival rate = `flows / duration`)
- **Flow sizes**: Pareto distribution (α = 1.2, x_min = 1500B) — heavy tail
  - ~80% mice flows (< 100 KB)
  - ~20% elephant flows (100 KB – 100 MB)
- **Burst model**: TCP window = 64 KB bursts with 200 μs inter-burst gaps

## Output Files

| File | Contents |
|---|---|
| `data/summary.csv` | Side-by-side metric comparison |
| `data/link_utils.csv` | Per-link utilization (ECMP and Flowlet) |
| `data/fct_dist.csv` | Full FCT distribution for both modes |
| `data/dashboard.html` | Interactive web dashboard (Chart.js) |
| `data/results.png` | Static publication-quality plots |

## Research Alignment

This simulator directly demonstrates the NVIDIA AI training cluster problem:
- **All-reduce traffic** in AI training creates many concurrent elephant flows
- ECMP hash collisions concentrate flows on the same spine → hot links → FCT blowup
- Flowlet switching recovers the 38% wasted bandwidth (ENLB 2025)
- Gap threshold tuning (50–500 μs) is the key parameter from NSDI 2025

## Dependencies

| Component | Version |
|---|---|
| g++ | ≥ 11 (C++17) |
| make | any |
| Python | ≥ 3.8 |
| matplotlib | ≥ 3.7 (optional, for plots) |
| Chart.js | 4.4.0 (CDN, for dashboard) |

---
 
