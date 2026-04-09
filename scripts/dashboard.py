#!/usr/bin/env python3
"""
dashboard.py — 100% self-contained HTML. Zero CDN / internet required.
All charts are inline SVG drawn by Python. Works fully offline on Kali Linux.
Run: python3 scripts/dashboard.py   (after make run)
Out: data/dashboard.html
"""

import os, sys, csv, math

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")

def read_summary():
    d = {}
    with open(os.path.join(DATA_DIR, "summary.csv")) as f:
        for row in csv.DictReader(f):
            d[row["metric"]] = {"ecmp": float(row["ECMP"]), "flowlet": float(row["Flowlet"])}
    return d

def read_link_utils():
    ids, ecmp, flowlet = [], [], []
    with open(os.path.join(DATA_DIR, "link_utils.csv")) as f:
        for row in csv.DictReader(f):
            ids.append(int(row["link_id"]))
            ecmp.append(float(row["ECMP"]))
            flowlet.append(float(row["Flowlet"]))
    return ids, ecmp, flowlet

def read_fct():
    ecmp, flowlet = [], []
    with open(os.path.join(DATA_DIR, "fct_dist.csv")) as f:
        for row in csv.DictReader(f):
            if row["mode"] == "ECMP": ecmp.append(float(row["fct_ms"]))
            else:                     flowlet.append(float(row["fct_ms"]))
    return ecmp, flowlet

def pct(vals, p):
    if not vals: return 0
    s = sorted(vals)
    return s[min(int(len(s)*p/100), len(s)-1)]

def imp(a, b):
    return (a - b) / a * 100 if a else 0

def make_cdf(values, n=80):
    if not values: return [], []
    s = sorted(values)
    step = max(1, len(s)//n)
    xs = s[::step]
    ys = [min((i*step+step)/len(s), 1.0) for i in range(len(xs))]
    return xs, ys

# ── SVG chart builders ────────────────────────────────────────────────────────
EC = "#f87171"
FL = "#4ade80"
BG = "#1a1f2e"
GRID = "#252d3d"

def bar_chart(ev, fv, labels, W=680, H=230, title=""):
    PL, PR, PT, PB = 52, 12, 26, 44
    w = W-PL-PR; h = H-PT-PB
    n = len(labels)
    if not n: return ""
    mx = max(max(ev,default=0), max(fv,default=0))*1.18 or 0.001
    gw = w/n; bw = gw*0.3
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
         f'<rect width="{W}" height="{H}" fill="{BG}" rx="6"/>']
    if title:
        o.append(f'<text x="{W//2}" y="17" text-anchor="middle" fill="#64748b" font-size="10" font-family="monospace">{title}</text>')
    for i in range(6):
        y = PT+h-(i/5)*h; v = mx*i/5
        o.append(f'<line x1="{PL}" y1="{y:.1f}" x2="{PL+w}" y2="{y:.1f}" stroke="{GRID}" stroke-width="0.6"/>')
        o.append(f'<text x="{PL-3}" y="{y+4:.1f}" text-anchor="end" fill="#4a5568" font-size="8" font-family="monospace">{v:.4f}</text>')
    for i,(lbl,e,f) in enumerate(zip(labels,ev,fv)):
        cx = PL+i*gw+gw/2
        eh = max((e/mx)*h,1); ey = PT+h-eh
        fh = max((f/mx)*h,1); fy = PT+h-fh
        o.append(f'<rect x="{cx-bw-1:.1f}" y="{ey:.1f}" width="{bw:.1f}" height="{eh:.1f}" fill="{EC}" opacity="0.9" rx="2"/>')
        o.append(f'<rect x="{cx+1:.1f}" y="{fy:.1f}" width="{bw:.1f}" height="{fh:.1f}" fill="{FL}" opacity="0.9" rx="2"/>')
        o.append(f'<text x="{cx:.1f}" y="{PT+h+13}" text-anchor="middle" fill="#4a5568" font-size="7.5" font-family="monospace">{lbl[:5]}</text>')
    lx=PL; ly=H-8
    o.append(f'<rect x="{lx}" y="{ly-7}" width="9" height="7" fill="{EC}" rx="1"/>')
    o.append(f'<text x="{lx+11}" y="{ly}" fill="#94a3b8" font-size="9" font-family="monospace">ECMP</text>')
    o.append(f'<rect x="{lx+52}" y="{ly-7}" width="9" height="7" fill="{FL}" rx="1"/>')
    o.append(f'<text x="{lx+63}" y="{ly}" fill="#94a3b8" font-size="9" font-family="monospace">Flowlet</text>')
    o.append('</svg>')
    return "".join(o)

def cdf_chart(ex, ey, fx, fy, W=460, H=210):
    PL,PR,PT,PB = 44,12,20,38
    w=W-PL-PR; h=H-PT-PB
    all_x=ex+fx
    if not all_x: return ""
    mn=min(all_x); mx=max(all_x) or 1
    def tx(v): return PL+(v-mn)/(mx-mn)*w
    def ty(v): return PT+h-v*h
    o=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
       f'<rect width="{W}" height="{H}" fill="{BG}" rx="6"/>',
       f'<text x="{W//2}" y="14" text-anchor="middle" fill="#64748b" font-size="10" font-family="monospace">FCT CDF</text>']
    for i in range(6):
        y=ty(i/5)
        o.append(f'<line x1="{PL}" y1="{y:.1f}" x2="{PL+w}" y2="{y:.1f}" stroke="{GRID}" stroke-width="0.5"/>')
        o.append(f'<text x="{PL-3}" y="{y+4:.1f}" text-anchor="end" fill="#4a5568" font-size="8" font-family="monospace">{i/5:.1f}</text>')
    for i in range(5):
        x=PL+i/4*w; v=mn+(mx-mn)*i/4
        o.append(f'<line x1="{x:.1f}" y1="{PT}" x2="{x:.1f}" y2="{PT+h}" stroke="{GRID}" stroke-width="0.5"/>')
        o.append(f'<text x="{x:.1f}" y="{PT+h+11}" text-anchor="middle" fill="#4a5568" font-size="7.5" font-family="monospace">{v:.3f}</text>')
    for xs,ys,col in [(ex,ey,EC),(fx,fy,FL)]:
        if not xs: continue
        pts=" ".join(f"{tx(x):.1f},{ty(y):.1f}" for x,y in zip(xs,ys))
        o.append(f'<polyline points="{pts}" fill="none" stroke="{col}" stroke-width="2" stroke-linejoin="round"/>')
    lx=PL; ly=H-8
    o.append(f'<line x1="{lx}" y1="{ly-3}" x2="{lx+12}" y2="{ly-3}" stroke="{EC}" stroke-width="2"/>')
    o.append(f'<text x="{lx+14}" y="{ly}" fill="#94a3b8" font-size="9" font-family="monospace">ECMP</text>')
    o.append(f'<line x1="{lx+56}" y1="{ly-3}" x2="{lx+68}" y2="{ly-3}" stroke="{FL}" stroke-width="2"/>')
    o.append(f'<text x="{lx+70}" y="{ly}" fill="#94a3b8" font-size="9" font-family="monospace">Flowlet</text>')
    o.append('</svg>')
    return "".join(o)

def hbar_chart(labels, values, W=320, H=210):
    PL,PR,PT,PB = 100,55,20,14
    w=W-PL-PR; h=H-PT-PB
    n=len(labels)
    if not n: return ""
    ma=max(abs(v) for v in values)*1.2 or 1
    rh=h/n; bh=rh*0.52
    zx=PL+w/2
    o=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
       f'<rect width="{W}" height="{H}" fill="{BG}" rx="6"/>',
       f'<text x="{W//2}" y="14" text-anchor="middle" fill="#64748b" font-size="10" font-family="monospace">Improvement (%)</text>',
       f'<line x1="{zx:.1f}" y1="{PT}" x2="{zx:.1f}" y2="{PT+h}" stroke="#374151" stroke-width="1"/>']
    for i,(lbl,v) in enumerate(zip(labels,values)):
        cy=PT+i*rh+rh/2
        bl=abs(v)/ma*(w/2)
        col=FL if v>=0 else EC
        bx=zx if v>=0 else zx-bl
        o.append(f'<rect x="{bx:.1f}" y="{cy-bh/2:.1f}" width="{bl:.1f}" height="{bh:.1f}" fill="{col}" opacity="0.88" rx="2"/>')
        o.append(f'<text x="{PL-3}" y="{cy+4:.1f}" text-anchor="end" fill="#94a3b8" font-size="8.5" font-family="monospace">{lbl}</text>')
        vx=zx+bl+3 if v>=0 else zx-bl-3
        anc="start" if v>=0 else "end"
        sign="+" if v>0 else ""
        o.append(f'<text x="{vx:.1f}" y="{cy+4:.1f}" text-anchor="{anc}" fill="{col}" font-size="8.5" font-family="monospace">{sign}{v:.1f}%</text>')
    o.append('</svg>')
    return "".join(o)

def kpi_svg(label, ev, fv, unit="", dec=4, low=True, W=210, H=88):
    i=imp(ev,fv) if low else imp(fv,ev)
    col=FL if i>=0 else EC
    sign="▼" if i>=0 else "▲"
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">'
            f'<rect width="{W}" height="{H}" fill="{BG}" rx="7" stroke="#252d3d" stroke-width="1"/>'
            f'<text x="10" y="19" fill="#475569" font-size="10" font-family="monospace">{label}</text>'
            f'<text x="10" y="48" fill="{col}" font-size="22" font-weight="bold" font-family="monospace">{fv:.{dec}f}{unit}</text>'
            f'<text x="10" y="65" fill="#374151" font-size="10" font-family="monospace">ECMP: {ev:.{dec}f}{unit}</text>'
            f'<text x="10" y="80" fill="{col}" font-size="10" font-family="monospace">{sign} {abs(i):.1f}% vs ECMP</text>'
            f'</svg>')

# ── assemble HTML ─────────────────────────────────────────────────────────────
def build(summary, ids, eu, fu, efct, ffct):
    link_lbl = [f"L{i}" for i in ids]
    bar   = bar_chart(eu, fu, link_lbl, title="Per-Link Utilization — ECMP vs Flowlet")
    ex,ey = make_cdf(efct); fx,fy = make_cdf(ffct)
    cdf_s = cdf_chart(ex,ey,fx,fy)
    imp_k = ["imbalance_ratio","std_dev_util","avg_fct_ms","p95_fct_ms","p99_fct_ms","max_link_util"]
    imp_l = ["Imbalance","StdDev","AvgFCT","p95FCT","p99FCT","MaxUtil"]
    imp_v = [imp(summary[k]["ecmp"],summary[k]["flowlet"]) for k in imp_k]
    hbar  = hbar_chart(imp_l, imp_v)

    kpis = "".join([
        kpi_svg("Imbalance Ratio", summary["imbalance_ratio"]["ecmp"], summary["imbalance_ratio"]["flowlet"], "×", 2),
        kpi_svg("Link StdDev",     summary["std_dev_util"]["ecmp"],     summary["std_dev_util"]["flowlet"],   "", 5),
        kpi_svg("Avg FCT",         summary["avg_fct_ms"]["ecmp"],       summary["avg_fct_ms"]["flowlet"],     " ms", 4),
        kpi_svg("p99 FCT",         summary["p99_fct_ms"]["ecmp"],       summary["p99_fct_ms"]["flowlet"],     " ms", 4),
    ])

    stat_defs = [
        ("avg_link_util","Avg Link Util",4,""),("max_link_util","Max Link Util",4,""),
        ("min_link_util","Min Link Util",4,""),("imbalance_ratio","Imbalance Ratio",3,"×"),
        ("std_dev_util","StdDev Util",5,""),("avg_fct_ms","Avg FCT",4," ms"),
        ("p50_fct_ms","p50 FCT",4," ms"),("p95_fct_ms","p95 FCT",4," ms"),
        ("p99_fct_ms","p99 FCT",4," ms"),("throughput_gbps","Throughput",4," Gbps"),
        ("hash_collisions","Hash Collisions",0,""),("flowlet_switches","Flowlet Switches",0,""),
        ("completed_flows","Completed Flows",0,""),
    ]
    trows=""
    for key,lbl,dec,unit in stat_defs:
        ev=summary[key]["ecmp"]; fv=summary[key]["flowlet"]
        fmt=f"{{:.{dec}f}}"
        trows+=f'<tr><td>{lbl}</td><td class="ec">{fmt.format(ev)}{unit}</td><td class="fl">{fmt.format(fv)}{unit}</td></tr>'

    return f"""<!DOCTYPE html>
<html lang="en"><head>
<meta charset="UTF-8"><title>FlowletLB Dashboard</title>
<style>
*{{box-sizing:border-box;margin:0;padding:0}}
body{{background:#0d1117;color:#e2e8f0;font-family:'Courier New',monospace;font-size:13px;padding:18px;line-height:1.5}}
header{{text-align:center;padding:18px 0 14px;border-bottom:1px solid #1e2d3d;margin-bottom:20px}}
header h1{{font-size:19px;letter-spacing:.08em;color:#60a5fa;font-weight:700}}
header p{{color:#475569;margin-top:5px;font-size:11px}}
.badge{{display:inline-block;padding:2px 7px;border-radius:9px;font-size:10px;font-weight:700;margin:0 2px}}
.be{{background:#7f1d1d;color:#fca5a5}}.bf{{background:#14532d;color:#86efac}}.bi{{background:#1e3a5f;color:#93c5fd}}
.grid{{display:grid;gap:14px;margin-bottom:14px}}
.g4{{grid-template-columns:repeat(4,1fr)}}
.g2{{grid-template-columns:1fr 1fr}}
.g32{{grid-template-columns:2fr 1fr}}
@media(max-width:860px){{.g4,.g2,.g32{{grid-template-columns:1fr 1fr}}}}
@media(max-width:500px){{.g4,.g2,.g32{{grid-template-columns:1fr}}}}
.panel{{background:#161b27;border:1px solid #1e2d3d;border-radius:7px;padding:13px}}
.panel h2{{font-size:10px;text-transform:uppercase;letter-spacing:.08em;color:#475569;margin-bottom:10px;border-bottom:1px solid #1e2d3d;padding-bottom:7px}}
.kpi-row{{display:flex;flex-wrap:wrap;gap:10px;justify-content:center}}
.kpi-row svg{{flex:1;min-width:190px;max-width:230px}}
.scr{{width:100%;overflow-x:auto}}
.scr svg{{min-width:360px;width:100%;height:auto}}
table{{width:100%;border-collapse:collapse;font-size:12px}}
th{{text-align:left;padding:4px 8px;color:#475569;font-size:10px;text-transform:uppercase;border-bottom:1px solid #1e2d3d}}
td{{padding:4px 8px;border-bottom:1px solid #151c28}}
tr:last-child td{{border-bottom:none}}
.ec{{color:#f87171;font-weight:600}}.fl{{color:#4ade80;font-weight:600}}
footer{{text-align:center;color:#334155;font-size:10px;margin-top:20px;padding-top:14px;border-top:1px solid #1e2d3d}}
</style></head><body>
<header>
  <h1>&#9889; FLOWLET LOAD BALANCER SIMULATOR</h1>
  <p><span class="badge be">ECMP</span> vs <span class="badge bf">Flowlet Switching</span> &middot;
     <span class="badge bi">NSDI 2025</span> <span class="badge bi">ENLB 2025</span> <span class="badge bi">CONGA 2014</span></p>
</header>

<div class="panel" style="margin-bottom:14px">
  <h2>Key Metrics &mdash; Flowlet vs ECMP (green = better)</h2>
  <div class="kpi-row">{kpis}</div>
</div>

<div class="panel" style="margin-bottom:14px">
  <h2>Per-Link Utilization</h2>
  <div class="scr">{bar}</div>
</div>

<div class="grid g32">
  <div class="panel">
    <h2>Flow Completion Time &mdash; CDF</h2>
    <div class="scr">{cdf_s}</div>
  </div>
  <div class="panel">
    <h2>Improvement over ECMP</h2>
    <div class="scr">{hbar}</div>
  </div>
</div>

<div class="panel" style="margin-top:14px">
  <h2>Full Stats &mdash; <span style="color:#f87171">ECMP</span> vs <span style="color:#4ade80">Flowlet</span></h2>
  <table><thead><tr><th>Metric</th><th>ECMP</th><th>Flowlet</th></tr></thead>
  <tbody>{trows}</tbody></table>
</div>

<footer>FlowletLB Simulator &middot; C++17 &middot; Spine-Leaf &middot; ECMP vs Flowlet Switching &middot; Research-Grade</footer>
</body></html>"""

def main():
    for f in ["summary.csv","link_utils.csv","fct_dist.csv"]:
        if not os.path.exists(os.path.join(DATA_DIR, f)):
            print(f"ERROR: data/{f} not found. Run 'make run' first."); sys.exit(1)
    s         = read_summary()
    ids,eu,fu = read_link_utils()
    ef,ff     = read_fct()
    html      = build(s, ids, eu, fu, ef, ff)
    out       = os.path.join(DATA_DIR, "dashboard.html")
    with open(out,"w") as f: f.write(html)
    print(f"[Dashboard] Generated -> {out}")
    print(f"            Open with: xdg-open {out}")

if __name__=="__main__": main()
