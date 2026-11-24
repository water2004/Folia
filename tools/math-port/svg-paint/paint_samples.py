"""Capture real raster samples, or verify/replay those samples without Node."""

import json
import math
from pathlib import Path

from comparison.corpus import digest
from comparison.exact_pixels import exact_pixels
from comparison.observation_process import SampleCapture, encoded, sample_file
from paint_dom import parse, require


METADATA = {"schema", "kind", "inputSha256", "backend", "sourceSha256", "rgba", "rgbaSha256", "png", "pngSha256"}


def difference(a, b):
    require(a["info"] == b["info"], "Raster dimensions or format differ")
    result = exact_pixels(a["data"], b["data"])
    return {key: result[key] for key in ("differentPixels", "maximumChannelDifference")}


def ink(image):
    return sum(value != 0 for value in image["data"][3::4])


class RasterSamples:
    def __init__(self, destination, inputs, script, *, observations=None, node="node"):
        self.destination, self.inputs = Path(destination), inputs
        self.script, self.node = script, node
        self.observation_path = Path(observations) if observations else self.destination / "raster-observations.json"
        self.offline = observations is not None
        self.entries, self.used, self.backend = {}, set(), None

    def __enter__(self):
        self.destination.mkdir(parents=True, exist_ok=True)
        if self.offline:
            self.samples = self.observation_path.parent / "samples"
            report = json.loads(self.observation_path.read_bytes())
            require(type(report.get("schema")) is int and report["schema"] == 1, "Wrong paint observation schema")
            require(report["inputs"] == self.inputs, "Paint observation input provenance changed")
            log = (self.samples / "samples.jsonl").read_bytes()
            require(digest(log) == report["samplesSha256"], "Paint raster observation log changed")
            for line in log.splitlines():
                entry = json.loads(line)
                request, sample = entry["request"], entry["sample"]
                require(request["kind"] == "svg" and sample["inputSha256"] == digest(encoded(request)), "Paint raster request changed")
                key = (entry["case"], sample["inputSha256"])
                require(key not in self.entries or self.entries[key] == entry, "Conflicting paint raster observations")
                self.entries[key] = entry
        else:
            self.samples = self.destination / "samples"
            self.capture = SampleCapture(self.script, self.samples, node=self.node).__enter__()
        return self

    def raster(self, svg, case, file=None):
        request = {"kind": "svg", "source": svg}
        if self.offline:
            key = (case, digest(encoded(request)))
            entry = self.entries.get(key)
            require(entry is not None and entry["request"] == request, "Missing exact paint raster observation for " + case)
            sample = entry["sample"]
            self.used.add(key)
        else:
            sample = self.capture.sample(request, case)
        require(type(sample.get("schema")) is int and sample["schema"] == 1 and sample["kind"] == "svg", "Wrong paint raster sample")
        require(sample["sourceSha256"] == digest(svg), "Paint raster source changed")
        backend = sample["backend"]
        require(backend["name"] == "sharp" and backend["versions"]["sharp"] == "0.35.5", "Unexpected paint raster backend")
        require(self.backend is None or self.backend == backend["versions"], "Mixed paint raster backends")
        self.backend = backend["versions"]
        root = parse(svg)
        width, height = (float(root.get(key)) * 2 for key in ("width", "height"))
        require(all(math.isfinite(value) and value > 0 and value.is_integer() for value in (width, height)), "Invalid raster dimensions")
        require(sample["width"] == width and sample["height"] == height and sample["channels"] == 4,
                "Expected exact 2x RGBA paint canvas")
        require(sample["format"] == "raw" and sample["depth"] == "uchar" and sample["premultiplied"] is False
                and sample["hasAlpha"] is True, "Unsupported paint raster format")
        data, png = sample_file(self.samples, sample, "rgba"), sample_file(self.samples, sample, "png")
        require(len(data) == width * height * 4 == sample["size"], "Incomplete paint raster pixels")
        if file is not None:
            Path(file).write_bytes(png)
        return {"data": data, "pngSha256": digest(png),
                "info": {key: value for key, value in sample.items() if key not in METADATA}}

    def __exit__(self, kind, value, traceback):
        if self.offline:
            if kind is None:
                require(self.used == self.entries.keys(), "Unexpected unused paint raster observations")
            return
        self.capture.__exit__(kind, value, traceback)
        if kind is None:
            report = {"schema": 1, "inputs": self.inputs,
                      "samplesSha256": digest((self.samples / "samples.jsonl").read_bytes())}
            self.observation_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
