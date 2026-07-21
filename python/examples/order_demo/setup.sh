#!/usr/bin/env bash
# One-time setup for order_demo: creates a virtualenv here and installs
# requirements.txt into it. That's all this does now — schema creation
# and user seeding live in exactly one place, scripts/setup.sh, so run
# that (once, from the repo's scripts/ dir) before this if you haven't.
# start_client.sh runs this automatically every time it starts a demo
# (cheap/fast once packages are already installed), so you normally
# don't need to run it directly at all.
#
# Self-healing: if .venv exists but its pip is broken (e.g. from an
# interrupted self-upgrade — a common Windows flake that leaves
# "ModuleNotFoundError: No module named 'pip._internal'" behind forever
# after), wipe and recreate it instead of failing the same way every
# time start_client.sh runs this. Fast path (healthy venv) stays fast;
# only a broken one pays the few-seconds recreate cost.
#
# You still need, separately (not done by this script):
#   nexus_server built and running (see repo root README / CMake, and
#   scripts/run.sh). nexus_server prints the absolute DB path it opened
#   on startup — if that doesn't match what scripts/setup.sh applied
#   schema to, set NEXUS_DB_PATH the same way on both sides.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

if [ -d .venv ]; then
    PYTHON_BIN=.venv/Scripts/python.exe
    [ -f "$PYTHON_BIN" ] || PYTHON_BIN=.venv/bin/python
    if [ ! -x "$PYTHON_BIN" ] || ! "$PYTHON_BIN" -m pip --version >/dev/null 2>&1; then
        echo "setup.sh: existing .venv has a broken pip — recreating it..."
        rm -rf .venv
    fi
fi

if [ ! -d .venv ]; then
    python3 -m venv .venv
fi

# POSIX venvs put activate under bin/; Windows (incl. Git Bash/MSYS) puts
# it under Scripts/ instead — support both.
if [ -f .venv/bin/activate ]; then
    source .venv/bin/activate
elif [ -f .venv/Scripts/activate ]; then
    source .venv/Scripts/activate
else
    echo "setup.sh: could not find venv activate script under .venv/" >&2
    exit 1
fi

# Not upgrading pip here on purpose — venv's bundled pip is plenty for
# installing one package (flask), and pip's own self-upgrade is the
# exact flaky step (interruptible, Windows file-lock-prone) that
# produces a broken "pip._internal" venv in the first place.
# -q + --disable-pip-version-check: this runs on every start_client.sh,
# so on the (common) already-installed path it should print nothing,
# not a wall of "Requirement already satisfied" lines and an unrelated
# "new pip version available" notice.
python -m pip install -q --disable-pip-version-check -r requirements.txt
