"""Gate native WinUI SVG content and viewport before activating the app switch.

Uses built DLLs only; does not build, install, or launch the application.
A missing portable native label is a failure, not a skipped test.
"""
import argparse
import ctypes as c
import math
import sys
import xml.etree.ElementTree as ET

SVG = "{http://www.w3.org/2000/svg}"
MARKER = "MermaidLabelSentinel"
LABEL_CASES = {
    "flowchart-control": f"flowchart TD\n A[{MARKER}] --> B[Finish]",
    "er": f"erDiagram\n{MARKER} ||--o{{ ORDER : places",
    "requirement": f"requirementDiagram\nrequirement R {{\ntext: {MARKER}\n}}",
    "usecase": f"usecase-beta\nactor User\nUser --> {MARKER}",
    "swimlane": f"swimlane-beta LR\nsubgraph Lane\nA[{MARKER}]\nend",
    "eventmodeling": f"eventmodeling\ntf 01 evt Output {{ {MARKER}: 7 }}",
    "venn": f"venn-beta\nset A\n  text first[{MARKER}]\nset B\nunion A,B[Both]",
}
VIEWPORT_CASES = {
    "info": "info",
    "flowchart": "flowchart TD\n A[Start 中文] --> B[Finish]",
    "sequence": "sequenceDiagram\n Alice->>Bob: Hello",
    "class": "classDiagram\n Class01 <|-- Class02",
    "pie": 'pie\n "One": 40\n "Two": 60',
}


class Pipeline:
    def __init__(self, native, normalizer):
        self.native = c.CDLL(str(native))
        self.normalizer = c.CDLL(str(normalizer))
        byte_pointer = c.POINTER(c.c_uint8)
        output = [c.POINTER(byte_pointer), c.POINTER(c.c_size_t),
                  c.POINTER(c.c_float), c.POINTER(c.c_float)]
        self.native.FoliaNativeMermaidRender.argtypes = [c.c_char_p, c.c_size_t, c.c_bool] + output
        self.native.FoliaNativeMermaidFree.argtypes = [byte_pointer]
        self.native.FoliaNativeMermaidFree.restype = None
        self.normalizer.folia_svg_normalizer_create.restype = c.c_void_p
        self.normalizer.folia_svg_normalizer_destroy.argtypes = [c.c_void_p]
        self.normalizer.folia_svg_normalizer_destroy.restype = None
        self.normalizer.folia_svg_normalize.argtypes = [c.c_void_p, c.c_char_p, c.c_size_t, c.c_float] + output
        self.normalizer.folia_svg_buffer_destroy.argtypes = [byte_pointer, c.c_size_t]
        self.normalizer.folia_svg_buffer_destroy.restype = None
        self.context = self.normalizer.folia_svg_normalizer_create()
        if not self.context:
            raise RuntimeError("normalizer context creation failed")

    def close(self):
        self.normalizer.folia_svg_normalizer_destroy(self.context)

    def invoke(self, source, dark=None):
        data = source.encode("utf-8")
        output = c.POINTER(c.c_uint8)()
        length, width, height = c.c_size_t(), c.c_float(), c.c_float()
        refs = [c.byref(output), c.byref(length), c.byref(width), c.byref(height)]
        if dark is None:
            status = self.normalizer.folia_svg_normalize(self.context, data, len(data), 16, *refs)
        else:
            status = self.native.FoliaNativeMermaidRender(data, len(data), dark, *refs)
        try:
            result = c.string_at(output, length.value).decode("utf-8") if output else ""
        finally:
            if dark is None:
                self.normalizer.folia_svg_buffer_destroy(output, length.value)
            else:
                self.native.FoliaNativeMermaidFree(output)
        if status != 0:
            raise AssertionError(f"DLL status {status}: {result}")
        assert all(math.isfinite(v) and v > 0 for v in [width.value, height.value])
        return result, width.value, height.value


def viewport(source, width, height):
    # Same numeric outer viewport as MermaidRenderer; inner viewBox is intact.
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:.6f}" '
            f'height="{height:.6f}" viewBox="0 0 {width:.6f} {height:.6f}">'
            f'{source}</svg>')


def check_viewport(pipeline, source, dark, info=False):
    raw, width, height = pipeline.invoke(source, dark)
    if info:
        assert (width, height) == (400, 100), (width, height)
    normalized, actual_width, actual_height = pipeline.invoke(viewport(raw, width, height))
    assert abs(actual_width - width) <= 0.001, (actual_width, width)
    assert abs(actual_height - height) <= 0.001, (actual_height, height)
    root = ET.fromstring(normalized)
    assert not list(root.iter(SVG + "text")), "text was not converted to paths"
    return raw, width, height, root


def check_label(pipeline, source, dark):
    raw, width, height, normalized = check_viewport(pipeline, source, dark)
    root = ET.fromstring(raw)
    # Checking absence only AFTER normalization misses silently dropped HTML.
    assert not list(root.iter(SVG + "foreignObject")), "native HTML labels would be silently dropped by usvg"
    labels = [node for node in root.iter(SVG + "text") if MARKER in "".join(node.itertext())]
    assert labels, "sentinel label is not present in portable native SVG text"
    # Remove only the sentinel's text; keep identical geometry and all other
    # labels. Normalization must lose actual glyph paths in this counterfactual.
    for parent in root.iter():
        for child in list(parent):
            if child in labels:
                parent.remove(child)
    without, _, _ = pipeline.invoke(viewport(ET.tostring(root, encoding="unicode"), width, height))
    before_paths = list(normalized.iter(SVG + "path"))
    after_paths = list(ET.fromstring(without).iter(SVG + "path"))
    assert len(before_paths) > len(after_paths), "sentinel label contributed no rendered glyph paths"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=str, required=True)
    parser.add_argument("--normalizer", type=str, required=True)
    args = parser.parse_args()
    pipeline = Pipeline(args.native, args.normalizer)
    failures, passed = [], 0
    try:
        for kind, cases in [("viewport", VIEWPORT_CASES), ("label", LABEL_CASES)]:
            for name, source in cases.items():
                for dark in [False, True]:
                    label = f"{kind}/{name}/{'dark' if dark else 'light'}"
                    try:
                        if kind == "label":
                            check_label(pipeline, source, dark)
                        else:
                            check_viewport(pipeline, source, dark, info=name == "info")
                        passed += 1
                        print(f"PASS {label}")
                    except (AssertionError, ET.ParseError) as error:
                        failures.append(label)
                        print(f"FAIL {label}: {error}")
    finally:
        pipeline.close()
    print(f"{passed} passed; {len(failures)} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
