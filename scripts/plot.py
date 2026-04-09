#!/usr/bin/env python3
"""
Flowlet Load Balancer Simulator — Static Plots
Generates publication-quality figures from simulation CSV data.
"""

import os
import sys
import csv
import math

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
OUT_DIR  = os.path.join(os.path.dirname(__file__), "..", "data")

def read_summary():
    path = os.path.join(DATA_DIR, "summary.csv")
    d = {}
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            d[row["metric"]] = (float(row["ECMP"]), float(row["Flowlet"]))
    return d

def read_link_utils():
    path = os.path.join(DATA_DIR, "link_utils.csv")
    ecmp, flowlet, ids = [], [], []
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            ids.append(int(row["link_id"]))
            ecmp.append(float(row["ECMP"]))
            flowlet.append(float(row["Flowlet"]))
    return ids, ecmp, flowlet

def read_fct():
    path = os.path.join(DATA_DIR, "fct_dist.csv")
    ecmp, flowlet = [], []
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row["mode"] == "ECMP":
                ecmp.append(float(row["fct_ms"]))
            else:
                flowlet.append(float(row["fct_ms"]))
    return ecmp, flowlet

def try_import_matplotlib():
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        import matplotlib.patches as mpatches
        return plt, mpatches
    except ImportError:
        return None, None

def ascii_bar(val, max_val, width=30, char="█"):
    filled = int(val / max_val * width) if max_val > 0 else 0
    return char * min(filled, width) + "░" * (width - min(filled, width))

def print_ascii_plots(summary, ids, ecmp_utils, flowlet_utils, ecmp_fct, flowlet_fct):
    print("\n" + "="*65)
    print("  PLOT SUMMARY (matplotlib not available — ASCII fallback)")
    print("="*65)

    # Bar chart: key metrics
    print("\n[1] KEY METRICS COMPARISON")
    print("-"*65)
    metrics = [
        ("Imbalance Ratio", "imbalance_ratio", True),
        ("Max Link Util",   "max_link_util",   True),
        ("StdDev Util",     "std_dev_util",    True),
        ("Avg FCT (ms)",    "avg_fct_ms",      True),
        ("p99 FCT (ms)",    "p99_fct_ms",      True),
    ]
    for label, key, lower_better in metrics:
        ev, fv = summary[key]
        mx = max(ev, fv) or 1
        tag = "(lower=better)" if lower_better else "(higher=better)"
        print(f"\n  {label} {tag}")
        print(f"  ECMP     [{ascii_bar(ev, mx)}] {ev:.4f}")
        print(f"  Flowlet  [{ascii_bar(fv, mx)}] {fv:.4f}")

    # Link utilization
    print("\n\n[2] PER-LINK UTILIZATION")
    print("-"*65)
    mx = max(max(ecmp_utils or [1]), max(flowlet_utils or [1])) or 1
    for i, (eu, fu) in enumerate(zip(ecmp_utils, flowlet_utils)):
        print(f"  Link {i:02d}  ECMP[{ascii_bar(eu,mx,20)}]{eu:.4f}  "
              f"Flowlet[{ascii_bar(fu,mx,20)}]{fu:.4f}")

    # FCT histogram
    print("\n\n[3] FCT DISTRIBUTION")
    print("-"*65)
    for label, vals in [("ECMP", ecmp_fct), ("Flowlet", flowlet_fct)]:
        if not vals: continue
        mn, mx2 = min(vals), max(vals)
        nb = 8
        buckets = [0]*nb
        for v in vals:
            b = int((v-mn)/(mx2-mn+1e-12)*(nb-1))
            buckets[max(0,min(nb-1,b))] += 1
        print(f"\n  {label}:")
        mb = max(buckets) or 1
        for b, cnt in enumerate(buckets):
            lo = mn + (mx2-mn)*b/nb
            hi = mn + (mx2-mn)*(b+1)/nb
            bar = "▪" * int(cnt/mb*20)
            print(f"    {lo:.3f}-{hi:.3f}ms [{bar:<20}] {cnt}")

    imb_imp = (summary["imbalance_ratio"][0] - summary["imbalance_ratio"][1]) / \
              (summary["imbalance_ratio"][0] or 1) * 100
    fct_imp = (summary["avg_fct_ms"][0] - summary["avg_fct_ms"][1]) / \
              (summary["avg_fct_ms"][0] or 1) * 100
    print(f"\n  >> Load imbalance improvement: {imb_imp:.1f}%")
    print(f"  >> Avg FCT improvement:         {fct_imp:.1f}%\n")

def make_plots_matplotlib(plt, mpatches, summary, ids, ecmp_utils, flowlet_utils,
                           ecmp_fct, flowlet_fct):
    ECMP_COL    = "#e74c3c"
    FLOW_COL    = "#2ecc71"
    BG_COL      = "#0d1117"
    PANEL_COL   = "#161b22"
    TEXT_COL    = "#c9d1d9"
    GRID_COL    = "#21262d"

    plt.rcParams.update({
        "figure.facecolor":  BG_COL,
        "axes.facecolor":    PANEL_COL,
        "axes.edgecolor":    GRID_COL,
        "axes.labelcolor":   TEXT_COL,
        "xtick.color":       TEXT_COL,
        "ytick.color":       TEXT_COL,
        "text.color":        TEXT_COL,
        "grid.color":        GRID_COL,
        "grid.linewidth":    0.5,
        "axes.grid":         True,
        "font.family":       "monospace",
        "font.size":         9,
    })

    fig = plt.figure(figsize=(16, 12), facecolor=BG_COL)
    fig.suptitle("Flowlet Load Balancer Simulator — ECMP vs Flowlet Switching",
                 fontsize=14, color=TEXT_COL, fontweight="bold", y=0.98)

    gs = fig.add_gridspec(3, 3, hspace=0.45, wspace=0.35,
                          left=0.07, right=0.97, top=0.93, bottom=0.07)

    # ── Panel 1: Key metrics grouped bar ────────────────────────────────────
    ax1 = fig.add_subplot(gs[0, :2])
    metric_keys = ["imbalance_ratio", "std_dev_util", "p95_fct_ms", "p99_fct_ms"]
    metric_labels = ["Imbalance\nRatio", "StdDev\nUtil", "p95 FCT\n(ms)", "p99 FCT\n(ms)"]
    ecmp_vals    = [summary[k][0] for k in metric_keys]
    flowlet_vals = [summary[k][1] for k in metric_keys]

    # Normalise each metric to ECMP=1.0
    norm_ecmp    = [1.0] * len(metric_keys)
    norm_flowlet = [fv/max(ev,1e-12) for ev, fv in zip(ecmp_vals, flowlet_vals)]

    x = list(range(len(metric_keys)))
    w = 0.35
    bars_e = ax1.bar([i-w/2 for i in x], norm_ecmp,    w, color=ECMP_COL, alpha=0.85, label="ECMP")
    bars_f = ax1.bar([i+w/2 for i in x], norm_flowlet, w, color=FLOW_COL, alpha=0.85, label="Flowlet")
    ax1.axhline(1.0, color=TEXT_COL, linewidth=0.8, linestyle="--", alpha=0.5)
    ax1.set_xticks(x)
    ax1.set_xticklabels(metric_labels, fontsize=8)
    ax1.set_ylabel("Normalised to ECMP")
    ax1.set_title("Key Metrics (normalised, lower = better)", color=TEXT_COL)
    ax1.legend(loc="upper right")
    for i, (ev, fv, nf) in enumerate(zip(ecmp_vals, flowlet_vals, norm_flowlet)):
        ax1.text(i-w/2, 1.02, f"{ev:.3f}", ha="center", va="bottom",
                 fontsize=7, color=ECMP_COL)
        ax1.text(i+w/2, nf+0.01, f"{fv:.3f}", ha="center", va="bottom",
                 fontsize=7, color=FLOW_COL)

    # ── Panel 2: Improvement summary ────────────────────────────────────────
    ax2 = fig.add_subplot(gs[0, 2])
    imprv_keys    = ["imbalance_ratio", "std_dev_util", "avg_fct_ms",
                     "p95_fct_ms", "p99_fct_ms", "max_link_util"]
    imprv_labels  = ["Imbalance", "StdDev\nUtil", "Avg FCT",
                     "p95 FCT", "p99 FCT", "Max Util"]
    improvements  = []
    for k in imprv_keys:
        ev, fv = summary[k]
        if ev > 0:
            improvements.append((ev - fv) / ev * 100)
        else:
            improvements.append(0.0)
    colors = [FLOW_COL if v >= 0 else ECMP_COL for v in improvements]
    bars = ax2.barh(imprv_labels, improvements, color=colors, alpha=0.85)
    ax2.axvline(0, color=TEXT_COL, linewidth=0.8)
    ax2.set_xlabel("Improvement (%)")
    ax2.set_title("Flowlet Improvement\nover ECMP (%)", color=TEXT_COL)
    for bar, val in zip(bars, improvements):
        ax2.text(val + (1 if val >= 0 else -1), bar.get_y() + bar.get_height()/2,
                 f"{val:.1f}%", va="center", ha="left" if val>=0 else "right",
                 fontsize=7, color=TEXT_COL)

    # ── Panel 3: Per-link utilization ────────────────────────────────────────
    ax3 = fig.add_subplot(gs[1, :2])
    x3 = list(range(len(ids)))
    ax3.bar([i-w/2 for i in x3], ecmp_utils,    w, color=ECMP_COL, alpha=0.8, label="ECMP")
    ax3.bar([i+w/2 for i in x3], flowlet_utils, w, color=FLOW_COL, alpha=0.8, label="Flowlet")
    ax3.set_xlabel("Link ID")
    ax3.set_ylabel("Utilization (fraction)")
    ax3.set_title("Per-Link Utilization — ECMP vs Flowlet", color=TEXT_COL)
    ax3.legend()
    ax3.set_xticks(x3)
    ax3.set_xticklabels([str(i) for i in ids], fontsize=7)

    # ── Panel 4: Utilization std dev ─────────────────────────────────────────
    ax4 = fig.add_subplot(gs[1, 2])
    import statistics as stats_mod
    ecmp_mean = sum(ecmp_utils)/len(ecmp_utils) if ecmp_utils else 0
    flow_mean = sum(flowlet_utils)/len(flowlet_utils) if flowlet_utils else 0
    ecmp_std  = stats_mod.stdev(ecmp_utils) if len(ecmp_utils)>1 else 0
    flow_std  = stats_mod.stdev(flowlet_utils) if len(flowlet_utils)>1 else 0
    ax4.bar(["ECMP", "Flowlet"], [ecmp_std, flow_std],
            color=[ECMP_COL, FLOW_COL], alpha=0.85)
    ax4.set_ylabel("Std Dev Utilization")
    ax4.set_title("Link Util StdDev\n(lower = better balance)", color=TEXT_COL)
    for i, (lbl, v) in enumerate(zip(["ECMP","Flowlet"], [ecmp_std, flow_std])):
        ax4.text(i, v*1.03, f"{v:.5f}", ha="center", fontsize=8, color=TEXT_COL)

    # ── Panel 5: FCT CDF ─────────────────────────────────────────────────────
    ax5 = fig.add_subplot(gs[2, :2])
    for fcts, col, lbl in [(ecmp_fct, ECMP_COL, "ECMP"),
                            (flowlet_fct, FLOW_COL, "Flowlet")]:
        if not fcts: continue
        sorted_fcts = sorted(fcts)
        n = len(sorted_fcts)
        cdf = [(i+1)/n for i in range(n)]
        ax5.plot(sorted_fcts, cdf, color=col, linewidth=2, label=lbl)
    ax5.set_xlabel("FCT (ms)")
    ax5.set_ylabel("CDF")
    ax5.set_title("Flow Completion Time CDF", color=TEXT_COL)
    ax5.legend()
    ax5.set_xscale("log")
    # Mark percentiles
    for fcts, col in [(ecmp_fct, ECMP_COL), (flowlet_fct, FLOW_COL)]:
        if not fcts: continue
        s = sorted(fcts)
        for p in [50, 95, 99]:
            v = s[int(len(s)*p/100)]
            ax5.axvline(v, color=col, linewidth=0.7, linestyle=":", alpha=0.6)

    # ── Panel 6: FCT histogram ────────────────────────────────────────────────
    ax6 = fig.add_subplot(gs[2, 2])
    if ecmp_fct and flowlet_fct:
        all_v = ecmp_fct + flowlet_fct
        bins_min, bins_max = min(all_v), max(all_v)
        nb = 30
        bw = (bins_max - bins_min) / nb or 0.001
        bins = [bins_min + i*bw for i in range(nb+1)]
        def make_hist(vals):
            counts = [0]*nb
            for v in vals:
                b = int((v - bins_min) / bw)
                counts[max(0,min(nb-1,b))] += 1
            return counts
        ec = make_hist(ecmp_fct)
        fc = make_hist(flowlet_fct)
        bcs = [(bins[i]+bins[i+1])/2 for i in range(nb)]
        ax6.bar(bcs, ec, bw*0.9, color=ECMP_COL, alpha=0.6, label="ECMP")
        ax6.bar(bcs, fc, bw*0.9, color=FLOW_COL, alpha=0.6, label="Flowlet")
    ax6.set_xlabel("FCT (ms)")
    ax6.set_ylabel("Count")
    ax6.set_title("FCT Histogram", color=TEXT_COL)
    ax6.legend()

    out_path = os.path.join(OUT_DIR, "results.png")
    plt.savefig(out_path, dpi=150, bbox_inches="tight", facecolor=BG_COL)
    print(f"[Plot] Saved → {out_path}")
    plt.close()

def main():
    if not os.path.exists(os.path.join(DATA_DIR, "summary.csv")):
        print("ERROR: data/summary.csv not found. Run 'make run' first.")
        sys.exit(1)

    summary       = read_summary()
    ids, eu, fu   = read_link_utils()
    ecmp_fct, flt = read_fct()

    plt, mpatches = try_import_matplotlib()
    if plt:
        make_plots_matplotlib(plt, mpatches, summary, ids, eu, fu, ecmp_fct, flt)
    else:
        print("matplotlib not available, using ASCII plots.")
        print_ascii_plots(summary, ids, eu, fu, ecmp_fct, flt)

if __name__ == "__main__":
    main()
