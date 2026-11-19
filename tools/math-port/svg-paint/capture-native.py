#!/usr/bin/env python3
"""Capture the production Math C ABI in document and standalone SVG modes."""

import argparse
import ctypes as C
import importlib.util
import json
from pathlib import Path
import platform
import sys

DIRECTORY = Path(__file__).resolve().parent
ROOT = DIRECTORY.parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--input", type=Path, default=DIRECTORY / "cases.json")
    parser.add_argument("--output", type=Path, default=ROOT / "build/math-svg-paint/native")
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location("math_fragment_capture",
        ROOT / "tools/mermaid-cpp/math-oracle/capture-native.py")
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    corpus = json.loads(args.input.read_text(encoding="utf-8"))
    if corpus["schema"] != 1 or not corpus["cases"]:
        raise ValueError("Expected a nonempty schema-1 paint corpus")
    if any(mode not in ("inline", "display", "fragment") for mode in corpus["modes"]):
        raise ValueError("Unknown native render mode")
    binary = args.library.resolve()
    before = capture.digest(binary)
    library = C.CDLL(str(binary))
    capture.configure(library)
    library.FoliaNativeMathRenderSvg.argtypes = [
        C.c_void_p, C.POINTER(C.c_uint16), C.c_size_t, C.c_int,
        C.c_double, C.c_double, C.c_void_p, C.c_uint, C.POINTER(C.c_void_p)]
    library.FoliaNativeMathRenderSvg.restype = C.c_int
    args.output.mkdir(parents=True, exist_ok=True)
    renderer = C.c_void_p()
    status = library.FoliaNativeMathRendererCreate(C.byref(renderer))
    if status:
        raise RuntimeError(f"Renderer creation failed: {status}")
    records = []
    try:
        for item in corpus["cases"]:
            for mode in corpus["modes"]:
                cancellation, result = C.c_void_p(), C.c_void_p()
                status = library.FoliaNativeMathCancellationCreate(C.byref(cancellation))
                if status:
                    raise RuntimeError(f"Cancellation creation failed: {status}")
                try:
                    encoded = item["tex"].encode("utf-16-le")
                    units = (C.c_uint16 * (len(encoded) // 2)).from_buffer_copy(encoded)
                    parameters = [renderer, units, len(units)]
                    if mode == "fragment":
                        function = library.FoliaNativeMathRenderSvgFragment
                    else:
                        function = library.FoliaNativeMathRenderSvg
                        parameters.append(int(mode == "display"))
                    status = function(*parameters, item.get("em", 16), item.get("width", 1280),
                                      cancellation, 0, C.byref(result))
                    svg = capture.read_result(library, "FoliaNativeMathResultSvg", result)
                    error = capture.read_result(library, "FoliaNativeMathResultError", result)
                    name = item["name"] + "--" + mode
                    file = args.output / (name + ".svg")
                    file.write_bytes(svg.encode("utf-8"))
                    row = {"name": name, "case": item["name"], "mode": mode,
                           "status": status, "error": error or None, "svg": svg,
                           "svgFile": file.name, "svgSha256": capture.digest(file)}
                    metrics = capture.SvgMetrics()
                    if library.FoliaNativeMathResultFragmentMetrics(result, C.byref(metrics)):
                        row["metrics"] = {key: getattr(metrics, key)
                                          for key in ("width", "height", "baseline")}
                    records.append(row)
                finally:
                    library.FoliaNativeMathResultDestroy(result)
                    library.FoliaNativeMathCancellationDestroy(cancellation)
    finally:
        library.FoliaNativeMathRendererDestroy(renderer)
    if capture.digest(binary) != before:
        raise RuntimeError("Native library changed during capture")
    report = {"schema": 1, "corpusSha256": capture.digest(args.input),
              "binary": {"path": str(binary), "sha256": before},
              "python": platform.python_version(), "platform": platform.platform(),
              "records": records}
    destination = args.output / "report.json"
    destination.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    failures = [row["name"] for row in records if row["status"] or row["error"] or not row["svg"]]
    print(json.dumps({"destination": str(destination), "records": len(records), "failures": failures}))
    return int(bool(failures))


if __name__ == "__main__":
    sys.exit(main())
