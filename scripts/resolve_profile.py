#!/usr/bin/env python3
"""
Resolve nexus_profile.csv (raw function addresses + ns timings) into a
human-readable, sorted latency report.

Usage:
    python scripts/resolve_profile.py <path-to-exe> [nexus_profile.csv]

Requires the exe to have been built with debug info (-g) so addr2line can
map addresses back to function names.
"""
import csv
import subprocess
import sys


def main():
    if len(sys.argv) < 2:
        print("usage: resolve_profile.py <exe> [csv]")
        sys.exit(1)

    exe = sys.argv[1]
    csv_path = sys.argv[2] if len(sys.argv) > 2 else "nexus_profile.csv"

    rows = []
    with open(csv_path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)

    addrs = [r["address"] for r in rows]
    # Batch through addr2line once for speed.
    result = subprocess.run(
        ["addr2line", "-f", "-C", "-e", exe, *addrs],
        capture_output=True, text=True, check=True,
    )
    lines = result.stdout.splitlines()
    # addr2line -f prints two lines per address: function, file:line
    names = lines[0::2]

    for r, name in zip(rows, names):
        r["function"] = name

    rows.sort(key=lambda r: int(r["total_ns"]), reverse=True)

    print(f"{'function':<40}{'calls':>10}{'total_ns':>15}{'avg_ns':>12}{'min_ns':>12}{'max_ns':>12}")
    for r in rows:
        print(f"{r['function']:<40}{r['calls']:>10}{r['total_ns']:>15}{r['avg_ns']:>12}{r['min_ns']:>12}{r['max_ns']:>12}")


if __name__ == "__main__":
    main()
