#!/usr/bin/env python3
"""Capture the production C++ MathJax fragment API without a Windows app host."""

import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import platform


class SvgMetrics(C.Structure):
    _fields_ = [(name, C.c_double) for name in ("width", "height", "baseline")]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def configure(library):
    pointer = C.c_void_p
    for name in ("RendererCreate", "CancellationCreate"):
        function = getattr(library, "FoliaNativeMath" + name)
        function.argtypes = [C.POINTER(pointer)]
        function.restype = C.c_int
    for name in ("RendererDestroy", "CancellationDestroy", "ResultDestroy"):
        function = getattr(library, "FoliaNativeMath" + name)
        function.argtypes = [pointer]
        function.restype = None
    library.FoliaNativeMathRenderSvgFragment.argtypes = [
        pointer, C.POINTER(C.c_uint16), C.c_size_t, C.c_double,
        C.c_double, pointer, C.c_uint, C.POINTER(pointer),
    ]
    library.FoliaNativeMathRenderSvgFragment.restype = C.c_int
    library.FoliaNativeMathResultFragmentMetrics.argtypes = [pointer, C.POINTER(SvgMetrics)]
    library.FoliaNativeMathResultFragmentMetrics.restype = C.c_int
    for name in ("FoliaNativeMathResultSvg", "FoliaNativeMathResultError"):
        function = getattr(library, name)
        function.argtypes = [pointer, C.POINTER(C.c_size_t)]
        function.restype = pointer


def read_result(library, name, result):
    size = C.c_size_t()
    data = getattr(library, name)(result, C.byref(size))
    return C.string_at(data, size.value).decode("utf-8") if data else ""


def main():
    directory = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--input", type=Path, default=directory / "cases.json")
    parser.add_argument("--output", type=Path,
                        default=Path("build/mermaid-math/native/report.json"))
    args = parser.parse_args()
    corpus = json.loads(args.input.read_text(encoding="utf-8"))
    assert corpus["schema"] == 1
    binary = args.library.resolve()
    binary_hash = digest(binary)
    library = C.CDLL(str(binary))
    configure(library)
    renderer = C.c_void_p()
    status = library.FoliaNativeMathRendererCreate(C.byref(renderer))
    if status:
        raise RuntimeError(f"Native renderer creation failed: {status}")
    records = []
    try:
        for item in corpus["cases"]:
            cancellation = C.c_void_p()
            status = library.FoliaNativeMathCancellationCreate(C.byref(cancellation))
            if status:
                raise RuntimeError(f"Cancellation creation failed: {status}")
            result = C.c_void_p()
            try:
                encoded = item["tex"].encode("utf-16-le")
                text = (C.c_uint16 * (len(encoded) // 2)).from_buffer_copy(encoded)
                status = library.FoliaNativeMathRenderSvgFragment(
                    renderer, text, len(text), item["em"], item["width"],
                    cancellation, 0, C.byref(result),
                )
                metrics = SvgMetrics()
                measured = library.FoliaNativeMathResultFragmentMetrics(result, C.byref(metrics))
                error = read_result(library, "FoliaNativeMathResultError", result)
                records.append({"name": item["name"], "status": status,
                    "svg": read_result(library, "FoliaNativeMathResultSvg", result),
                    "width": metrics.width if measured else None,
                    "height": metrics.height if measured else None,
                    "baseline": metrics.baseline if measured else None,
                    "error": {"status": status, "message": error} if status or error else None})
            finally:
                library.FoliaNativeMathResultDestroy(result)
                library.FoliaNativeMathCancellationDestroy(cancellation)
    finally:
        library.FoliaNativeMathRendererDestroy(renderer)
    assert binary_hash == digest(binary), "Native binary changed during capture"
    report = {"schema": 1, "corpusSha256": digest(args.input),
              "binary": {"path": str(binary), "sha256": binary_hash},
              "python": platform.python_version(), "platform": platform.platform(),
              "records": records}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"destination": str(args.output), "records": len(records),
                      "errors": [item["name"] for item in records if item["error"]]}))


if __name__ == "__main__":
    main()
