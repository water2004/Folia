#!/usr/bin/env python3
"""Render every built-in theme through the production Mermaid pipeline.

Uses unchanged repository corpus cases, one per family, with host-side themes.
Windows uses DirectWrite; other hosts supply real Pango measurements. This is a
native rendering smoke matrix, not an upstream SVG comparison. --no-raster
needs no Pillow or SVG rasterizer; optional PNGs use librsvg/Cairo and Pillow.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import ctypes
import ctypes.util
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
THEMES = ["default", "dark", "base", "forest", "neutral", "neo", "neo-dark",
          "redux", "redux-dark", "redux-color", "redux-dark-color"]
PREFERRED_CASES = {
    "flowchart": "flowchart-official-service-workflow",
    "sequence": "sequence-lifecycle",
    "class": "class-official-inheritance",
    "state": "state-nested-composites",
    "wardley": "wardley-official-01",
    "ishikawa": "ishikawa-official-01",
    "mindmap": "mindmap-official-overview",
}
CONTACT_FAMILIES = {"flowchart", "sequence", "class", "er", "state", "pie", "gantt", "packet", "radar", "xy"}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def cases(selected):
    result = []
    for path in sorted((ROOT / "tests/mermaid_regression/cases").glob("*.json")):
        document = json.loads(path.read_text())
        family = document["family"]
        if selected and family not in selected:
            continue
        candidates = [case for case in document["cases"]
                      if "$$" not in case["source"] and
                      not re.search(r"(?i)\b(theme|themeVariables)[\"']?\s*:", case["source"])]
        if not candidates:
            raise ValueError(f"No formula-free corpus case without embedded theme settings: {family}")
        if family in PREFERRED_CASES:
            chosen = next(c for c in candidates if c["id"] == PREFERRED_CASES[family])
        else:
            chosen = min(candidates,
                         key=lambda c: {"basic": 0, "intermediate": 1, "complex": 2}[c["level"]])
        result.append({"family": family, "sourceFile": str(path.relative_to(ROOT)),
                       "corpusSha256": sha(path.read_bytes()), **chosen})
    return result


def theme_backgrounds():
    result = {}
    for line in (ROOT / "tests/mermaid_cpp/fixtures/themes.tsv").read_text().splitlines():
        if line.startswith("#"):
            continue
        fields = line.split("\t")
        if fields[1] == "baseline":
            result[fields[0]] = json.loads(bytes.fromhex(fields[3]).decode("utf-16-be"))["background"]
    return result


class SvgRasterizer:
    """Optional native SVG rasterization; no source/CSS rewriting."""

    class Rectangle(ctypes.Structure):
        _fields_ = [(name, ctypes.c_double) for name in ("x", "y", "width", "height")]

    class Error(ctypes.Structure):
        _fields_ = [("domain", ctypes.c_uint32), ("code", ctypes.c_int), ("message", ctypes.c_char_p)]

    def __init__(self):
        try:
            from PIL import ImageColor
        except ImportError as error:
            raise RuntimeError("PNG/contact-sheet output requires Pillow; use --no-raster for SVG only") from error
        self.color = ImageColor.getrgb
        libraries = [ctypes.util.find_library(name) for name in ("rsvg-2", "cairo", "gobject-2.0", "glib-2.0")]
        if not all(libraries):
            raise RuntimeError("PNG output requires librsvg, Cairo, GLib and Pillow; use --no-raster for SVG only")
        self.rsvg, self.cairo, self.gobject, self.glib = [ctypes.CDLL(name) for name in libraries]
        pointer, double, integer = ctypes.c_void_p, ctypes.c_double, ctypes.c_int
        self.rsvg.rsvg_handle_new_from_file.argtypes = [ctypes.c_char_p, ctypes.POINTER(pointer)]
        self.rsvg.rsvg_handle_new_from_file.restype = pointer
        self.rsvg.rsvg_handle_render_document.argtypes = [pointer, pointer, ctypes.POINTER(self.Rectangle), ctypes.POINTER(pointer)]
        self.rsvg.rsvg_handle_render_document.restype = integer
        self.cairo.cairo_image_surface_create.argtypes = [integer, integer, integer]
        self.cairo.cairo_image_surface_create.restype = pointer
        self.cairo.cairo_create.argtypes = [pointer]
        self.cairo.cairo_create.restype = pointer
        self.cairo.cairo_set_source_rgb.argtypes = [pointer, double, double, double]
        self.cairo.cairo_set_source_rgb.restype = None
        self.cairo.cairo_paint.argtypes = [pointer]
        self.cairo.cairo_paint.restype = None
        self.cairo.cairo_scale.argtypes = [pointer, double, double]
        self.cairo.cairo_scale.restype = None
        self.cairo.cairo_surface_write_to_png.argtypes = [pointer, ctypes.c_char_p]
        self.cairo.cairo_surface_write_to_png.restype = integer
        self.cairo.cairo_destroy.argtypes = [pointer]
        self.cairo.cairo_destroy.restype = None
        self.cairo.cairo_surface_destroy.argtypes = [pointer]
        self.cairo.cairo_surface_destroy.restype = None
        self.cairo.cairo_version_string.restype = ctypes.c_char_p
        self.gobject.g_object_unref.argtypes = [pointer]
        self.gobject.g_object_unref.restype = None
        self.glib.g_error_free.argtypes = [pointer]
        self.glib.g_error_free.restype = None
        self.version = {"name": "librsvg", "version": ".".join(
            str(ctypes.c_uint.in_dll(self.rsvg, f"rsvg_{part}_version").value)
            for part in ("major", "minor", "micro")),
            "cairoVersion": self.cairo.cairo_version_string().decode()}

    def error_message(self, error):
        if not error:
            return "SVG rasterizer returned no diagnostic"
        message = ctypes.cast(error, ctypes.POINTER(self.Error)).contents.message.decode()
        self.glib.g_error_free(error)
        return message

    def render(self, arguments):
        row, output, background = arguments
        result = {"family": row["family"], "theme": row["theme"], "ok": False}
        svg = output / row["svgFile"]
        target = output / "png" / f'{row["family"]}--{row["theme"]}.png'
        scale = min(2.0, 2400 / row["width"], 2400 / row["height"])
        width, height = [max(1, round(value * scale)) for value in (row["width"], row["height"])]
        handle = surface = context = None
        try:
            error = ctypes.c_void_p()
            handle = self.rsvg.rsvg_handle_new_from_file(str(svg).encode(), ctypes.byref(error))
            if not handle:
                raise RuntimeError(self.error_message(error))
            surface = self.cairo.cairo_image_surface_create(0, width, height)  # CAIRO_FORMAT_ARGB32
            context = self.cairo.cairo_create(surface)
            self.cairo.cairo_set_source_rgb(context, *(value / 255 for value in self.color(background)))
            self.cairo.cairo_paint(context)
            self.cairo.cairo_scale(context, width / row["width"], height / row["height"])
            viewport = self.Rectangle(0, 0, row["width"], row["height"])
            if not self.rsvg.rsvg_handle_render_document(handle, context, ctypes.byref(viewport), ctypes.byref(error)):
                raise RuntimeError(self.error_message(error))
            status = self.cairo.cairo_surface_write_to_png(surface, str(target).encode())
            if status:
                raise RuntimeError(f"Cairo PNG write failed with status {status}")
            return {**result, "ok": True, "pngFile": str(target.relative_to(output)),
                    "pngSha256": sha(target.read_bytes()), "pixelSize": [width, height],
                    "canvasBackground": background}
        except Exception as error:
            return {**result, "error": str(error)}
        finally:
            if context:
                self.cairo.cairo_destroy(context)
            if surface:
                self.cairo.cairo_surface_destroy(surface)
            if handle:
                self.gobject.g_object_unref(handle)


def contacts(report, output):
    from PIL import Image, ImageDraw, ImageFont
    face = subprocess.check_output(["fc-match", "--format=%{file}", "DejaVu Sans"], text=True).strip()
    font = ImageFont.truetype(face, 25)
    sheets = []
    for family in sorted(CONTACT_FAMILIES.intersection(c["family"] for c in report["cases"])):
        rows = [r for r in report["rasters"] if r["family"] == family and r["ok"]]
        if not rows:
            continue
        rows.sort(key=lambda r: THEMES.index(r["theme"]))
        cell_w, cell_h, margin, heading = 640, 460, 20, 55
        sheet = Image.new("RGB", (cell_w * 4, cell_h * math.ceil(len(rows) / 4) + heading), "#e7e9ec")
        draw = ImageDraw.Draw(sheet)
        draw.text((margin, 13), f"Folia native Mermaid / {family} / {len(rows)} themes", font=font, fill="#17212d")
        for index, row in enumerate(rows):
            x, y = (index % 4) * cell_w, heading + (index // 4) * cell_h
            draw.text((x + margin, y + 8), row["theme"], font=font, fill="#17212d")
            with Image.open(output / row["pngFile"]) as image:
                image = image.convert("RGB")
                image.thumbnail((cell_w - margin * 2, cell_h - 70), Image.Resampling.LANCZOS)
                box_x = x + (cell_w - image.width) // 2
                box_y = y + 50 + (cell_h - 70 - image.height) // 2
                draw.rectangle((x + margin, y + 49, x + cell_w - margin, y + cell_h - 12),
                               fill=row["canvasBackground"])
                sheet.paste(image, (box_x, box_y))
        file = output / f"contact-{family}.png"
        sheet.save(file)
        sheets.append(str(file.relative_to(output)))
    return sheets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True, help="Built FoliaMermaidRegressionProbe executable")
    parser.add_argument("--output", type=Path, default=ROOT / "build/mermaid-themes")
    parser.add_argument("--family", action="append", default=[])
    parser.add_argument("--no-raster", action="store_true", help="Only render/validate SVG, without librsvg or Pillow")
    parser.add_argument("--raster-only", action="store_true")
    args = parser.parse_args()
    if args.raster_only and args.no_raster:
        parser.error("--raster-only and --no-raster cannot be combined")
    output = args.output.resolve()
    for directory in (output, output / "svg"):
        directory.mkdir(parents=True, exist_ok=True)
    report_path = output / "report.json"
    if args.raster_only:
        report = json.loads(report_path.read_text())
    else:
        spec = importlib.util.spec_from_file_location("native_measurement",
            ROOT / "tools/mermaid-cpp/regression/run-native.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        provider = None
        inputs = cases(args.family)
        if not inputs:
            parser.error("No corpus cases selected; check --family names")
        missing = set(args.family) - {case["family"] for case in inputs}
        if missing:
            parser.error("Unknown families: " + ", ".join(sorted(missing)))
        binary = args.probe.resolve()
        report = {"schema": 1, "stage": "native-theme-render", "measurement": None,
                  "measurementVersion": None,
                  "probe": str(binary), "probeSha256": sha(binary.read_bytes()),
                  "themes": THEMES, "cases": inputs, "results": [],
                  "passed": 0, "total": len(inputs) * len(THEMES),
                  "officialSvgCompared": False, "winUiTested": False}
        with subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=open(output / "probe-stderr.log", "w"),
                              text=True, encoding="utf-8", bufsize=1) as probe:
            for case in inputs:
                for theme in THEMES:
                    start = time.monotonic()
                    config = {**case.get("config", {}), "theme": theme, "look": "classic"}
                    request = {"source": case["source"], "config": config, "availableWidth": 1600}
                    probe.stdin.write(json.dumps(request, ensure_ascii=True) + "\n")
                    probe.stdin.flush()
                    while True:
                        line = probe.stdout.readline()
                        if not line:
                            report_path.write_text(json.dumps(report, indent=2) + "\n")
                            raise RuntimeError(f"Probe exited unexpectedly at {case['id']} / {theme}")
                        record = json.loads(line)
                        if record["kind"] != "measure":
                            break
                        try:
                            if provider is None:
                                provider = module.PangoMeasurer()
                                report["measurement"] = "Pango"
                                report["measurementVersion"] = provider.Pango.version_string()
                            reply = provider.respond(json.dumps(record, sort_keys=True))
                        except Exception as error:
                            reply = {"error": str(error)}
                        probe.stdin.write(json.dumps(reply, ensure_ascii=True) + "\n")
                        probe.stdin.flush()
                    svg = record.pop("svg", None)
                    if record["ok"]:
                        try:
                            if not all(type(record.get(key)) in (int, float) and
                                       math.isfinite(record[key]) and record[key] > 0
                                       for key in ("width", "height")):
                                raise ValueError("Native output has nonpositive or nonfinite dimensions")
                            if not svg:
                                raise ValueError("Native output has no SVG")
                            tree = ET.fromstring(svg)
                            if tree.tag != "{http://www.w3.org/2000/svg}svg":
                                raise ValueError("Native output has no SVG root")
                            # SVG does not require a viewBox. Some renderers
                            # supply intrinsic dimensions through DiagramSvg.
                            if "viewBox" in tree.attrib:
                                viewbox = [float(value) for value in re.split(r"[\s,]+", tree.get("viewBox").strip())]
                                if len(viewbox) != 4 or not all(math.isfinite(value) for value in viewbox) or min(viewbox[2:]) <= 0:
                                    raise ValueError("Native SVG has an invalid viewBox")
                        except (ValueError, TypeError, ET.ParseError) as error:
                            record["ok"], record["error"] = False, str(error)
                    if record["ok"]:
                        file = output / "svg" / f"{case['family']}--{theme}.svg"
                        svg_bytes = svg.encode("utf-8")
                        file.write_bytes(svg_bytes)
                        record["svgFile"] = str(file.relative_to(output))
                        record["svgSha256"] = sha(svg_bytes)
                        record["textElements"] = len(tree.findall(".//{http://www.w3.org/2000/svg}text"))
                        record["elementCount"] = sum(1 for _ in tree.iter())
                    row = {"family": case["family"], "caseId": case["id"], "theme": theme,
                           "seconds": time.monotonic() - start, "sourceSha256": sha(case["source"].encode()),
                           "config": config, **record}
                    report["results"].append(row)
                    report["passed"] += bool(row["ok"])
                    print(f"{'PASS' if row['ok'] else 'FAIL'} {case['family']} / {theme}" +
                          (f": {row.get('error')}" if not row["ok"] else ""), flush=True)
                report_path.write_text(json.dumps(report, indent=2) + "\n")
            probe.stdin.close()
            if probe.wait(timeout=10):
                raise RuntimeError("Probe exited abnormally")
        if report["measurement"] is None:
            report["measurement"] = "DirectWrite or no text measurement requested"
        report_path.write_text(json.dumps(report, indent=2) + "\n")

    if args.no_raster:
        report["rasterCompared"] = False
        report_path.write_text(json.dumps(report, indent=2) + "\n")
        print(f"Native {report['passed']}/{report['total']}; raster not requested")
        print(report_path)
        return 0 if report["passed"] == report["total"] else 1

    (output / "png").mkdir(exist_ok=True)
    rasterizer = SvgRasterizer()
    backgrounds = theme_backgrounds()
    jobs = [(r, output, backgrounds[r["theme"]]) for r in report["results"] if r["ok"]]
    with ThreadPoolExecutor(max_workers=3) as pool:
        report["rasters"] = list(pool.map(rasterizer.render, jobs))
    report["rasterTool"] = rasterizer.version
    report["canvasBackgroundSource"] = "tests/mermaid_cpp/fixtures/themes.tsv: pinned official baseline background"
    report["rasterCompared"] = False  # PNG generation/inspection is not an upstream raster comparison.
    report["rasterPassed"] = sum(r["ok"] for r in report["rasters"])
    report["rasterTotal"] = len(report["rasters"])
    report["contactSheets"] = contacts(report, output)
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Native {report['passed']}/{report['total']}; raster {report['rasterPassed']}/{report['rasterTotal']}")
    print(report_path)
    return 0 if report["passed"] == report["total"] and report["rasterPassed"] == report["rasterTotal"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Theme rendering failed: {error}", file=sys.stderr)
        sys.exit(1)
