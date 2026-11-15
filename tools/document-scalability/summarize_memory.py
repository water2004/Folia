"""Summarize the recorded phased-memory runs without discarding noisy samples."""

import argparse
import json
import statistics
from collections import defaultdict
from pathlib import Path


def summarize(directory: Path) -> dict:
    environment = json.loads((directory / "environment.json").read_text())
    groups = defaultdict(list)
    for run in environment["runs"]:
        records = [json.loads(line) for line in (directory / run["file"]).read_text().splitlines()]
        if records[-1]["phase"] != "close":
            raise ValueError(f"Incomplete run: {run['file']}")
        groups[(run["case"], run["revision"])].append((run, records))

    cases = defaultdict(dict)
    for (case, revision), samples in groups.items():
        phases = defaultdict(list)
        peaks = []
        for _, records in samples:
            edit_index = 0
            peaks.append(max(row.get("peak_rss_kib", 0) for row in records))
            for row in records:
                if "milliseconds" not in row:
                    continue
                phase = row["phase"]
                if phase == "edit_undo_projection":
                    phase += "_" + ("first", "middle", "last")[edit_index]
                    edit_index += 1
                phases[phase].append(row)

        result = {
            "samples": len(samples),
            "input_bytes": samples[0][1][0]["input_bytes"],
            "census": next(row for row in samples[0][1] if row["phase"] == "census"),
            "process_peak_rss_kib_median": statistics.median(peaks),
            "process_seconds_median": statistics.median(run["process_seconds"] for run, _ in samples),
            "phases": {},
        }
        for phase, rows in phases.items():
            times = [row["milliseconds"] for row in rows]
            values = {
                "milliseconds": {"median": statistics.median(times), "min": min(times), "max": max(times)}
            }
            for field in ("allocated_bytes", "rss_kib", "peak_rss_kib"):
                values[field + "_median"] = statistics.median(row[field] for row in rows)
            for field in ("full_parses", "full_serializations", "full_index_scans"):
                values[field + "_max"] = max(row[field] for row in rows)
            if "render_units" in rows[0]:
                values["render_units"] = rows[0]["render_units"]
            result["phases"][phase] = values
        cases[case][revision] = result
    return {"memory_units": "bytes and KiB as named; divide bytes by 1048576 for MiB", "cases": dict(cases)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", type=Path, default=Path(__file__).with_name("memory-results"))
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    target = args.output or args.results / "summary.json"
    target.write_text(json.dumps(summarize(args.results), indent=2) + "\n")
    print(target)


if __name__ == "__main__":
    main()
