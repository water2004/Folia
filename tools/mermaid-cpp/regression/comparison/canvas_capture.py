"""Run native ZenUML with real Skia sample generation, preserving raw evidence."""

from __future__ import annotations

import json
import math
import re
from pathlib import Path

from .canvas_records import JS_SPACE, official_records, verify_backend
from .corpus import digest, require
from .observation_process import JsonProcess, SampleCapture, sample_file


def capture_native(probe, official_directory, destination, *, node="node"):
    official, corpus_hash = official_records(official_directory)
    probe = probe.resolve()
    binary_hash = digest(probe.read_bytes())
    destination.mkdir(parents=True, exist_ok=True)
    script = Path(__file__).resolve().parents[2] / "oracle/capture-skia-samples.mjs"
    records, total = [], 0
    with SampleCapture(script, destination / "samples", node=node) as oracle:
        profile = oracle.sample({"kind": "profile"})
        verify_backend(profile["backend"])
        require(profile["availableFonts"] == official["backend"]["availableFonts"],
                "The official and native Canvas captures must use the same installed fonts")
        with (destination / "native.stderr.log").open("w", encoding="utf-8") as stderr:
            with JsonProcess([str(probe), "--remote-measurement"], stderr=stderr) as native:
                for reference in official["cases"]:
                    ident, count = reference["id"], 0
                    native.write({"source": reference["source"], "config": {"theme": "default", "look": "classic"}})
                    while True:
                        response = native.read()
                        if response.get("kind") != "measure":
                            break
                        if response["method"] == "family":
                            native.write({"family": response["family"]})
                            continue
                        require(response["method"] == "text" and not response["styles"],
                                "ZenUML must request real unstyled text widths")
                        font = response["font"]
                        description = ("italic " if font["italic"] else "") + f"{font['weight']} {font['size']:g}px {font['family']}"
                        text = response["text"] if response["preserve"] else re.sub(r"[ \t\r\n]+", " ", response["text"]).strip(JS_SPACE)
                        sample = oracle.sample({"kind": "text", "font": description, "text": text}, ident)
                        metrics = sample["metrics"]
                        require(all(type(v) in (int, float) and math.isfinite(v) for v in metrics.values()) and metrics["advance"] >= 0,
                                ident + ": invalid actual Canvas measurements")
                        native.write(metrics)
                        count += 1
                    total += count
                    record = {"id": ident, "native": {key: value for key, value in response.items() if key != "svg"}, "fontRequests": count}
                    if response["ok"]:
                        require(count > 0, ident + ": native renderer did not request Canvas text measurements")
                        source = (official_directory / reference["svg"]).read_text(encoding="utf-8")
                        require(digest(source) == reference["svgSha256"], ident + ": official SVG changed")
                        svg = response["svg"]
                        path = destination / (ident + ".svg")
                        path.write_text(svg, encoding="utf-8", newline="")
                        record.update(svg=path.name, svgSha256=digest(svg))
                        width = math.ceil(max(reference["width"], response["width"]))
                        height = math.ceil(max(reference["height"], response["height"]))
                        for label, value in (("official", source), ("native", svg)):
                            sample = oracle.sample({"kind": "svg", "source": value, "width": width, "height": height}, ident)
                            record[label + "Raster"] = sample
                            (destination / (ident + "." + label + ".png")).write_bytes(sample_file(oracle.directory, sample, "png"))
                    records.append(record)
                    print("CAPTURE " + ident)
    require(digest(probe.read_bytes()) == binary_hash, "Native probe changed during capture")
    report = {"schema": 1, "stage": "zenuml-canvas-observations", "officialSha256": digest((official_directory / "official.json").read_bytes()),
              "corpusSha256": corpus_hash, "nativeProbeSha256": binary_hash, "backend": profile["backend"],
              "nativeFontRequests": total, "samplesSha256": digest((destination / "samples/samples.jsonl").read_bytes()), "records": records}
    path = destination / "native-observations.json"
    path.write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    return path
