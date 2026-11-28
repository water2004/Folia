"""The original Pie DOM and seven stylesheet checks, independent of Node."""

from __future__ import annotations

import json
import math
import re
from pathlib import Path

import cssselect2
import tinycss2
from lxml import etree

from .corpus import REPOSITORY, UPSTREAM, digest, require


FIXTURE = REPOSITORY / "tests/mermaid_cpp/fixtures/pie-svg.tsv"
SOURCE_LOCK = REPOSITORY / "tools/mermaid-cpp/source-lock.json"


def select(root, selector):
    return [item.etree_element for item in cssselect2.ElementWrapper.from_xml_root(root).query_all(selector)
            if item.etree_element is not root]


def first(root, selector):
    nodes = select(root, selector)
    require(bool(nodes), f"Missing Pie element: {selector}")
    return nodes[0]


def inline(node, key):
    values = {decl.lower_name: tinycss2.serialize(decl.value).strip()
              for decl in tinycss2.parse_declaration_list(node.get("style", ""), skip_comments=True)
              if decl.type == "declaration"}
    return values.get(key, "")


def attr(node, key):
    return inline(node, key) or node.get(key) or ""


def number(value):
    text = "" if value is None else str(value).strip()
    try:
        return float(int(text, 0)) if re.match(r"^0[xob]", text, re.I) else float(text) if text else 0.
    except ValueError:
        return math.nan


def parse_float(value):
    match = re.match(r"^[\s]*([+-]?(?:Infinity|(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?))", value or "")
    return float(match[1]) if match else math.nan


def translate(node):
    return [number(value) for value in re.split(r"[, ]+", node.get("transform", "translate(0,0)")[10:-1])]


def parse(source):
    root = etree.fromstring(source.encode("utf-8"), etree.XMLParser(resolve_entities=False, no_network=True))
    require(etree.QName(root).localname == "svg", "Expected a Pie SVG root")
    return root


def snapshot(root):
    group = first(root, "g")
    title = first(root, ".pieTitleText")
    text = lambda node: "".join(node.itertext())
    view = root.get("viewBox")
    require(view is not None, "Missing Pie viewBox")
    return {
        "viewBox": [number(value) for value in view.split(" ")],
        "width": root.get("width"), "height": root.get("height"), "maxWidth": inline(root, "max-width"),
        "outer": translate(group), "pie": translate(first(group, "g")),
        "radius": parse_float(first(root, "circle").get("r")),
        "paths": [[node.get("d"), attr(node, "fill"), node.get("class")] for node in select(root, "path")],
        "slices": [[text(node), *translate(node), attr(node, "text-anchor")] for node in select(root, ".slice")],
        "title": [text(title), title.get("x"), title.get("y")],
        "legends": [[*translate(node), text(first(node, "text")), attr(first(node, "rect"), "fill"),
                     attr(first(node, "rect"), "stroke"), first(node, "rect").get("width"),
                     first(node, "rect").get("height"), first(node, "text").get("x"), first(node, "text").get("y")]
                    for node in select(root, ".legend")],
    }


def equal(expected, actual, path="pie"):
    if type(expected) in (int, float):
        require(type(actual) in (int, float) and math.isfinite(expected) and math.isfinite(actual)
                and abs(expected - actual) < 1e-10, f"{path}: {expected!r} != {actual!r}")
    elif isinstance(expected, dict):
        require(isinstance(actual, dict) and list(expected) == list(actual), path + ": object keys differ")
        for key, value in expected.items():
            equal(value, actual[key], path + "." + key)
    elif isinstance(expected, list):
        require(isinstance(actual, list) and len(expected) == len(actual), path + ": array length differs")
        for index, value in enumerate(expected):
            equal(value, actual[index], f"{path}.{index}")
    else:
        require(type(expected) is type(actual) and expected == actual, f"{path}: {expected!r} != {actual!r}")


def compare_case(item, native):
    name = item["name"]
    if not item["finite"]:
        require(native == "ERROR", name + ": expected exact ERROR result for non-finite upstream SVG")
        return {"name": name, "status": "passed", "expectedError": True}
    actual = parse(native)
    equal(item["snapshot"], snapshot(actual), name)
    rules = item["styles"]
    require(len(rules) == 7, name + ": expected seven official stylesheet rules")
    for rule in rules:
        selector, declarations = rule["selector"], rule["declarations"]
        if "highlighted" in selector:
            normalized = selector + "{" + "".join(key + ":" + value + ";" for key, value in declarations) + "}"
            require(normalized in "".join(first(actual, "style").itertext()), name + " " + selector)
        else:
            for node in select(actual, selector):
                for key, value in declarations:
                    owner = first(actual, "g") if key == "font-family" else node
                    expected = format(parse_float(value), ".15g") if key in ("font-size", "stroke-width") else value
                    require(owner.get(key) == expected, f"{name} {selector} {key}: {owner.get(key)!r} != {expected!r}")
    return {"name": name, "status": "passed", "expectedError": False}


def load_capture(path, fixture=FIXTURE, source_lock=SOURCE_LOCK):
    capture = json.loads(Path(path).read_text(encoding="utf-8"))
    require(type(capture.get("schemaVersion")) is int and capture["schemaVersion"] == 1, "Unsupported Pie capture schema")
    require(capture.get("upstream") == UPSTREAM, "Pie upstream identity differs")
    require(capture.get("sourceLockSha256") == digest(source_lock.read_bytes()), "Pie source lock differs")
    require(capture.get("fixtureSha256") == digest(fixture.read_bytes()), "Pie fixture bytes differ")
    rows = [line.split("\t") for line in fixture.read_text(encoding="utf-8").splitlines()
            if line and not line.startswith("#")]
    require(rows and all(len(row) == 11 for row in rows), "Malformed Pie corpus")
    cases = capture["cases"]
    require([item["name"] for item in cases] == [row[0] for row in rows], "Pie capture must cover every fixture in order")
    for item, row in zip(cases, rows):
        encoded = row[1]
        require(len(encoded) % 4 == 0 and re.fullmatch("[0-9a-f]+", encoded) is not None, "Invalid Pie UTF-16 input")
        source = bytes.fromhex(encoded).decode("utf-16-be")
        require(item["sourceSha256"] == digest(source), item["name"] + ": source differs")
        require(type(item["finite"]) is bool and item["finite"] == (row[10] == "1"), item["name"] + ": finite status differs")
        require(item["svgSha256"] == digest(item["svg"]), item["name"] + ": official SVG hash differs")
        if item["finite"]:
            equal(item["snapshot"], snapshot(parse(item["svg"])), item["name"] + ".official")
    return capture


def compare_capture(reference, native, fixture=FIXTURE, source_lock=SOURCE_LOCK):
    capture = load_capture(reference, fixture, source_lock)
    results = [compare_case(item, (Path(native) / (item["name"] + ".svg")).read_text(encoding="utf-8"))
               for item in capture["cases"]]
    return {"schemaVersion": 1, "referenceSha256": digest(Path(reference).read_bytes()),
            "checked": len(results), "compared": sum(not row["expectedError"] for row in results), "results": results}
