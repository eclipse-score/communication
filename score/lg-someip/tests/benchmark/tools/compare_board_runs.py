# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************
"""Compare two lg-someip board benchmark logs. Usage: python3 compare_board_runs.py <baseline.log> <candidate.log>"""

import re, sys

LAT = re.compile(
    r"^(\w+)\s+(UDP|TCP)\s+(c\d+)\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*$"
)
RES = re.compile(r"^(bench_sender|someip-daemon|rpc_client)\s+(\d+)\s+([\d.]+)\s+([\d.]+)(?:\s+([\d.]+))?\s*$")
RPC = re.compile(
    r"^(\d+B)\s+(client processing \(app - wire\)|app round trip|wire round trip)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*$"
)
DROP = re.compile(r"^dropped.*?:\s*(\d+)")
KEEP = {"Uint8", "Bytes", "NestedStruct", "FixedArray"}


def parse(path):
    out, target, res_kind = {}, None, None
    for line in open(path):
        line = line.rstrip("\n")
        m = re.search(r"/test_cases/(\w+)\.runfiles", line)
        if line.startswith("rootdir") and m:
            target = m.group(1)
            continue
        if line.startswith("process") and "CPU" in line:
            res_kind = "cpu"
        elif line.startswith("process") and "RSS" in line:
            res_kind = "mem"
        if m := LAT.match(line):
            t, c, size = m.group(1, 3, 4)
            out[(target, f"{t} {size}B {c}")] = dict(
                count=int(m[5]), p50=float(m[9]), p99=float(m[11]), mean=float(m[8])
            )
        elif m := RES.match(line):
            if res_kind == "cpu":
                out[(target, f"{m[1]} %CPU")] = dict(mean=float(m[3]), peak=float(m[4]))
            else:
                out[(target, f"{m[1]} RSS MiB")] = dict(mean=float(m[3]), peak=float(m[4]))
        elif m := RPC.match(line):
            out[(target, f"{m[1]} {m[2]}")] = dict(p50=float(m[6]), p99=float(m[8]), mean=float(m[5]))
        elif m := DROP.match(line):
            out[(target, "dropped")] = dict(n=int(m[1]))
    return out


def pct(a, b):
    return f"{(b - a) / a * 100:+.0f}%" if a else "n/a"


def main():
    base, cand = parse(sys.argv[1]), parse(sys.argv[2])
    show_all = "--all" in sys.argv
    keys = [k for k in base if k in cand] + [k for k in cand if k not in base]
    cur = None
    for k in sorted(keys, key=lambda k: (k[0], keys.index(k))):
        tgt, row = k
        if (
            not show_all
            and row.split()[0] not in KEEP
            and "%CPU" not in row
            and "RSS" not in row
            and "dropped" not in row
            and not row[0].isdigit()
        ):
            continue
        if tgt != cur:
            cur = tgt
            print(f"\n### {tgt}\n")
            print(
                "| row | baseline p50 / mean | candidate p50 / mean | Δ | baseline p99 / peak | candidate p99 / peak | Δ |"
            )
            print("|---|---|---|---|---|---|---|")
        a, b = base.get(k), cand.get(k)
        if a is None or b is None:
            print(f"| {row} | {a} | {b} | | | | |")
        elif "n" in a:
            print(f"| {row} | {a['n']} | {b['n']} | | | | |")
        elif "p50" in a:
            print(
                f"| {row} | {a['p50']:.1f} | {b['p50']:.1f} | {pct(a['p50'], b['p50'])} | {a['p99']:.1f} | {b['p99']:.1f} | {pct(a['p99'], b['p99'])} |"
            )
        else:
            print(
                f"| {row} | {a['mean']:.1f} | {b['mean']:.1f} | {pct(a['mean'], b['mean'])} | {a['peak']:.1f} | {b['peak']:.1f} | {pct(a['peak'], b['peak'])} |"
            )


if __name__ == "__main__":
    main()
