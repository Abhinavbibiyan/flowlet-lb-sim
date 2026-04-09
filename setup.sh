#!/usr/bin/env bash
# setup.sh — one-shot setup for Kali Linux / Ubuntu
# Creates Python venv, installs deps, builds the C++ simulator
set -e

echo "=============================================="
echo " Flowlet Load Balancer Simulator — Setup"
echo "=============================================="

# ── 1. Check g++ ─────────────────────────────────────────────────────────────
if ! command -v g++ &>/dev/null; then
    echo "[!] g++ not found. Installing..."
    sudo apt-get install -y g++ make
fi
echo "[✓] g++ $(g++ --version | head -1)"

# ── 2. Python venv ───────────────────────────────────────────────────────────
if [ ! -d "venv" ]; then
    echo "[*] Creating Python virtual environment..."
    python3 -m venv venv
    echo "[✓] venv created"
else
    echo "[✓] venv already exists"
fi

source venv/bin/activate
echo "[*] Installing Python dependencies..."
pip install --quiet --upgrade pip
pip install --quiet -r requirements.txt
echo "[✓] Python deps installed"

# ── 3. Build C++ simulator ───────────────────────────────────────────────────
echo "[*] Building C++ simulator..."
make clean
make
echo "[✓] Build complete"

echo ""
echo "=============================================="
echo " Setup complete! Next steps:"
echo ""
echo "  1. Activate venv:     source venv/bin/activate"
echo "  2. Run simulation:    make run"
echo "  3. Plot results:      python3 scripts/plot.py"
echo "  4. HTML dashboard:    python3 scripts/dashboard.py"
echo "     Then open:         xdg-open data/dashboard.html"
echo "  5. Custom run:"
echo "     ./bin/flowlet_sim --flows 500 --spines 4 --leaves 4 \\"
echo "       --hosts 8 --load 0.8 --duration 0.1 --gap-us 150"
echo "=============================================="
