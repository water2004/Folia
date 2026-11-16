#!/usr/bin/env python3
"""Run every corpus case through the production C++ measurement/layout pipeline.

Windows probes use DirectWrite themselves. On Linux this process supplies actual
Pango font shaping. Pango runs are native render regressions, not a substitute for
the separate official-browser SVG comparison gate.
"""
import argparse
import ctypes
import functools
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import time


class PangoMeasurer:
    def __init__(self):
        import gi
        gi.require_version("Pango", "1.0")
        gi.require_version("PangoCairo", "1.0")
        from gi.repository import Pango, PangoCairo
        self.Pango = Pango
        self.context = PangoCairo.FontMap.get_default().create_context()
        self.language = Pango.Language.from_string("en-us")
        # GI cannot expose PangoLogAttr's C bitfields reliably. Read the public
        # C ABI for Unicode line-break opportunities, without estimating widths.
        self.library = ctypes.CDLL("libpango-1.0.so.0")
        self.library.pango_language_from_string.argtypes = [ctypes.c_char_p]
        self.library.pango_language_from_string.restype = ctypes.c_void_p
        self.library.pango_get_log_attrs.argtypes = [ctypes.c_char_p, ctypes.c_int,
            ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32), ctypes.c_int]
        self.native_language = self.library.pango_language_from_string(b"en-us")

    def description(self, font):
        p = self.Pango
        description = p.FontDescription()
        description.set_family(font["family"])
        description.set_absolute_size(font["size"] * p.SCALE)
        description.set_weight(p.Weight(font["weight"]))
        description.set_style(p.Style.ITALIC if font["italic"] else p.Style.NORMAL)
        return description

    @staticmethod
    def unit_offsets(text):
        units = [0]
        for char in text:
            units.append(units[-1] + (2 if ord(char) > 0xFFFF else 1))
        return units

    def layout(self, text, font, styles=()):
        p = self.Pango
        layout = p.Layout.new(self.context)
        layout.set_font_description(self.description(font))
        layout.set_single_paragraph_mode(True)
        layout.set_text(text, -1)
        attributes = p.AttrList()
        attributes.insert(p.attr_font_features_new("kern=1" if font["pair_kerning"] else "kern=0"))
        offsets = self.unit_offsets(text)
        for offset, length, weight, italic in styles:
            first, last = offsets.index(offset), offsets.index(offset + length)
            for attribute in [p.attr_weight_new(p.Weight(weight)),
                    p.attr_style_new(p.Style.ITALIC if italic else p.Style.NORMAL)]:
                attribute.start_index = len(text[:first].encode("utf-8"))
                attribute.end_index = len(text[:last].encode("utf-8"))
                attributes.insert(attribute)
        layout.set_attributes(attributes)
        return layout

    def text(self, request):
        text = request["text"]
        if not request["preserve"]:
            text = re.sub(r"[ \t\r\n]+", " ", text).strip(" ")
        if not text:
            return dict.fromkeys(["advance", "ink_width", "bounds_width", "line_height", "baseline"], 0)
        layout = self.layout(text, request["font"], request["styles"])
        ink, logical = layout.get_extents()
        scale = self.Pango.SCALE
        advance = logical.width / scale
        left, right = ink.x / scale, (ink.x + ink.width) / scale
        return {"advance": advance, "ink_width": ink.width / scale,
            "bounds_width": max(advance, right) - min(0, left),
            "line_height": logical.height / scale, "baseline": layout.get_baseline() / scale}

    def font(self, request):
        metrics = self.context.get_metrics(self.description(request["font"]), self.language)
        scale = self.Pango.SCALE
        ascent = math.floor(metrics.get_ascent() / scale + .5)
        descent = math.floor(metrics.get_descent() / scale + .5)
        ink, _ = self.layout("x", request["font"]).get_extents()
        return {"height": ascent + descent, "baseline": ascent,
            "middle_offset": ink.height / scale / 2}

    def wrap(self, request):
        text, width = request["text"], request["width"]
        if not text or width is None:
            return {"lines": [text]}
        if width <= 0:
            raise ValueError("Invalid wrapping width")
        encoded = text.encode("utf-8")
        attributes = (ctypes.c_uint32 * (len(text) + 1))()
        self.library.pango_get_log_attrs(encoded, len(encoded), -1, self.native_language,
            attributes, len(attributes))
        breaks = [i for i in range(1, len(text) + 1) if attributes[i] & 1]
        if not breaks or breaks[-1] != len(text):
            breaks.append(len(text))
        offsets = self.unit_offsets(text)

        def measured(first, last):
            source = text[first:last]
            if not request["break_spaces"]:
                source = source.rstrip(" \t")
                last = first + len(source)
            styles = []
            for begin, length, weight, italic in request["styles"]:
                left, right = max(begin, offsets[first]), min(begin + length, offsets[last])
                if right > left:
                    styles.append([left - offsets[first], right - left, weight, italic])
            _, logical = self.layout(source, request["font"], styles).get_extents()
            return logical.width / self.Pango.SCALE

        lines, start, previous = [], 0, 0
        for end in breaks:
            if previous > start and measured(start, end) > width:
                lines.append(text[start:previous])
                start = previous
            previous = end
        lines.append(text[start:])
        return {"lines": lines}

    @functools.lru_cache(maxsize=2048)
    def respond(self, encoded):
        request = json.loads(encoded)
        method = request["method"]
        if method == "family":
            family = subprocess.check_output(["fc-match", "--format=%{family[0]}", request["family"]],
                text=True).strip()
            if not family:
                raise ValueError("No configured font is installed")
            return {"family": family}
        return getattr(self, method)(request)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("build/mermaid-regression/native"))
    parser.add_argument("--family", action="append", default=[])
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--prepare", action="store_true", help="Only parse/configure; never counted as SVG success")
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[3]
    corpus_dir = repository / "tests/mermaid_regression/cases"
    probe_path = args.probe.resolve()
    with probe_path.open("rb") as binary:
        probe_hash = hashlib.file_digest(binary, "sha256").hexdigest()
    args.output.mkdir(parents=True, exist_ok=True)
    cases, hashes = [], {}
    for path in sorted(corpus_dir.glob("*.json")):
        document = json.loads(path.read_text())
        if args.family and document["family"] not in args.family:
            continue
        hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        for case in document["cases"]:
            if not args.case or case["id"] in args.case:
                cases.append({"family": document["family"], **case})
    if not cases:
        parser.error("No corpus cases selected")
    results = []
    provider = None
    with subprocess.Popen([str(probe_path)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1) as probe:
        for case in cases:
            start = time.monotonic()
            # Match the application's request initialization. Frontmatter and
            # case configuration can still select another supported appearance.
            config = {"theme": "default", "look": "classic", **case.get("config", {})}
            request = {"source": case["source"], "config": config, "prepare": args.prepare,
                "availableWidth": 1600}
            probe.stdin.write(json.dumps(request, ensure_ascii=True) + "\n")
            probe.stdin.flush()
            while True:
                line = probe.stdout.readline()
                if not line:
                    raise RuntimeError(f"Native probe exited unexpectedly at {case['id']}; stopping the broader run")
                record = json.loads(line)
                if record["kind"] != "measure":
                    break
                if provider is None:
                    provider = PangoMeasurer()
                try:
                    reply = provider.respond(json.dumps(record, sort_keys=True))
                except Exception as error:
                    reply = {"error": str(error)}
                probe.stdin.write(json.dumps(reply, ensure_ascii=True) + "\n")
                probe.stdin.flush()
            svg = record.pop("svg", None)
            if svg is not None:
                (args.output / (case["id"] + ".svg")).write_text(svg, encoding="utf-8")
                record["svgFile"] = case["id"] + ".svg"
                record["svgSha256"] = hashlib.sha256(svg.encode()).hexdigest()
            results.append({"id": case["id"], "family": case["family"],
                "seconds": time.monotonic() - start, **record})
            print(f"{'PASS' if record['ok'] else 'FAIL'} {case['id']}" +
                (f": {record['error']}" if not record["ok"] else ""), flush=True)
        probe.stdin.close()
        if probe.wait() != 0:
            raise RuntimeError("Native probe did not exit normally")
    report = {"schema": 1, "stage": "prepare" if args.prepare else "render",
        "probe": {"path": str(probe_path), "sha256": probe_hash},
        "measurementProvider": "Pango" if provider else "DirectWrite or no text measurement requested",
        "measurementVersion": provider.Pango.version_string() if provider else None,
        "officialSvgCompared": False, "corpusHashes": hashes, "results": results,
        "passed": sum(row["ok"] for row in results), "total": len(results)}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{report['passed']}/{report['total']} {report['stage']} cases passed")
    return 0 if report["passed"] == report["total"] else 1


if __name__ == "__main__":
    sys.exit(main())
