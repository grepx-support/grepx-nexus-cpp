#!/usr/bin/env python3
# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""
Parse the flat text output of:
    perf report --stdio -g none --sort=overhead,symbol > report.txt

into a simple, sorted, readable latency/hotspot table.

Usage:
    python3 parse_perf_report.py report.txt
    python3 parse_perf_report.py report.txt --top 20
    python3 parse_perf_report.py report.txt --debug   # show unmatched lines
"""
import argparse
import re
import sys

# Loose match: a leading percentage, then whatever else, then try to find
# a [.] or [k] marker (user/kernel) followed by the symbol name. perf's
# exact column set varies by version/args, so we don't hardcode column
# count -- we just anchor on the two things that are always there: the
# leading "NN.NN%" and the "[.]"/"[k]" annotation right before the symbol.
PCT_RE = re.compile(r"^\s*(?P<pct>\d+\.\d+)%")
KIND_SYMBOL_RE = re.compile(r"\[(?P<kind>[.k])\]\s+(?P<symbol>.+?)\s*$")


def parse(path, debug=False):
    rows = []
    unmatched = []
    with open(path, "r", errors="replace") as f:
        for line in f:
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            pct_m = PCT_RE.match(line)
            if not pct_m:
                continue
            ks_m = KIND_SYMBOL_RE.search(line)
            if not ks_m:
                # Percentage line but no [.]/[k]  marker -- likely a
                # different column layout (e.g. --sort=overhead only,
                # no dso). Fall back to: everything after the last
                # whitespace run of non-symbol columns.
                unmatched.append(line.rstrip())
                continue
            rows.append({
                "pct": float(pct_m.group("pct")),
                "kind": "kernel" if ks_m.group("kind") == "k" else "user",
                "symbol": ks_m.group("symbol").strip(),
            })

    if debug and unmatched:
        print(f"--- {len(unmatched)} percentage-lines did not match "
              f"(no [.]/[k] marker found) ---", file=sys.stderr)
        for u in unmatched[:10]:
            print(repr(u), file=sys.stderr)

    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("report_file")
    ap.add_argument("--top", type=int, default=25, help="how many rows to show")
    ap.add_argument("--debug", action="store_true", help="print unmatched lines to stderr")
    args = ap.parse_args()

    rows = parse(args.report_file, debug=args.debug)
    if not rows:
        print("No rows parsed.")
        print("Run with --debug to see raw lines that didn't match, "
              "or just paste the first 15-20 lines of the report file back "
              "and we'll fix the parser to match the exact format.")
        sys.exit(1)

    rows.sort(key=lambda r: r["pct"], reverse=True)

    print(f"{'% CPU time':>10}  {'where':<8}  function")
    print("-" * 90)
    for r in rows[: args.top]:
        print(f"{r['pct']:>9.2f}%  {r['kind']:<8}  {r['symbol']}")

    total_shown = sum(r["pct"] for r in rows[: args.top])
    print("-" * 90)
    print(f"Top {min(args.top, len(rows))} functions account for {total_shown:.1f}% of sampled CPU time")
    print(f"({len(rows)} total functions appeared in samples)")


if __name__ == "__main__":
    main()
