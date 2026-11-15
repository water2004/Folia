"""Summarize the checked-in raw core benchmark observations without rounding."""

from __future__ import annotations

import argparse
import collections
import json
from pathlib import Path
import statistics


def summarize(path: Path) -> dict:
    rows = [json.loads(line) for line in path.read_text().splitlines() if line]
    if not rows or rows[0].get("phase") != "metadata":
        raise ValueError(f"Missing workload metadata: {path}")
    measurements = rows[1:]
    groups: dict[tuple[str, str], list[dict]] = collections.defaultdict(list)
    for row in measurements:
        groups[row["phase"], row["position"]].append(row)
    phases = {}
    for (phase, position), values in groups.items():
        phases[f"{phase}/{position}"] = {
            "median_ms": statistics.median(row["milliseconds"] for row in values),
            "max_ms": max(row["milliseconds"] for row in values),
            "rss_kib": values[-1]["rss_kib"],
            "peak_rss_kib": max(row["peak_rss_kib"] for row in values),
        }
    return {
        "metadata": rows[0],
        "phases": phases,
        "max_rss_kib": max(row["rss_kib"] for row in measurements),
        "peak_rss_kib": max(row["peak_rss_kib"] for row in measurements),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, default=Path(__file__).parent)
    parser.add_argument("--series", choices=("initial", "structural", "final"), default="initial")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    series_suffix = "" if args.series == "initial" else f"-{args.series}"
    report = {}
    for size, suffix in ((1, "-1mib"), (10, "")):
        report[str(size)] = {
            label: summarize(args.directory / f"results-{label}{series_suffix}{suffix}.jsonl")
            for label in ("baseline", "current")
        }
    text = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
