#!/usr/bin/env python3
"""Summarize Build 196 [MEM]/[LAT] serial telemetry without dependencies."""

from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path
import re

PAIR_RE = re.compile(r"([A-Za-z0-9_]+)=([^\s]+)")
MEM_FIELDS = (
    "int_free", "int_min", "int_largest",
    "dma_free", "dma_min", "dma_largest",
    "psram_total", "psram_free", "psram_min", "psram_largest",
)


def parse_line(line: str):
    if "[MEM]" in line:
        kind = "MEM"
    elif "[LAT]" in line:
        kind = "LAT"
    else:
        return None
    values = dict(PAIR_RE.findall(line))
    if "tag" not in values:
        return None
    return kind, values


def as_int(values, key):
    try:
        return int(values[key], 0)
    except (KeyError, ValueError):
        return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path, help="serial log containing Build 196 [MEM]/[LAT] lines")
    args = parser.parse_args()

    mem_by_tag = defaultdict(list)
    lat_by_tag = defaultdict(list)
    for line in args.log.read_text(encoding="utf-8", errors="replace").splitlines():
        parsed = parse_line(line)
        if not parsed:
            continue
        kind, values = parsed
        if kind == "MEM":
            mem_by_tag[values["tag"]].append(values)
        else:
            us = as_int(values, "us")
            if us is not None:
                lat_by_tag[values["tag"]].append(us)

    if not mem_by_tag and not lat_by_tag:
        print("No Build 196 telemetry found.")
        return 1

    print("MEMORY BY TAG")
    print("tag,count,int_free_min,int_largest_min,dma_free_min,dma_largest_min,psram_free_min,psram_largest_min")
    for tag in sorted(mem_by_tag):
        rows = mem_by_tag[tag]
        def minimum(field):
            vals = [v for r in rows if (v := as_int(r, field)) is not None]
            return min(vals) if vals else ""
        print(",".join(map(str, (
            tag, len(rows), minimum("int_free"), minimum("int_largest"),
            minimum("dma_free"), minimum("dma_largest"),
            minimum("psram_free"), minimum("psram_largest"),
        ))))

    if lat_by_tag:
        print("\nLATENCY BY TAG")
        print("tag,count,min_us,avg_us,max_us")
        for tag in sorted(lat_by_tag):
            vals = lat_by_tag[tag]
            print(f"{tag},{len(vals)},{min(vals)},{sum(vals)//len(vals)},{max(vals)}")

    all_mem = [r for rows in mem_by_tag.values() for r in rows]
    if all_mem:
        print("\nGLOBAL LOW-WATER OBSERVATIONS")
        for field in ("int_free", "int_min", "int_largest", "dma_free", "dma_min", "dma_largest", "psram_free", "psram_min", "psram_largest"):
            vals = [v for r in all_mem if (v := as_int(r, field)) is not None]
            if vals:
                print(f"{field}={min(vals)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
