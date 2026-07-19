#!/usr/bin/env bash
# One-time setup for order_demo: creates a virtualenv here and installs
# requirements.txt into it. Run once before ./run.sh.
#
# You still need, separately (not done by this script):
#   1. nexus_server built and running (see repo root README / CMake)
#   2. python ../../scripts/create_schema.py   (creates the SQLite schema)
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt

echo
echo "order_demo: venv ready in $(pwd)/.venv"
echo "Next: make sure nexus_server is running, then ./run.sh"
