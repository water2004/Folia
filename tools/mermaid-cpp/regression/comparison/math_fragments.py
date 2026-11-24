"""Exact ordered MathJax SVG drawing trees and fragment metrics."""

from __future__ import annotations

import math
import re

from lxml import etree

from .corpus import require


NUMERIC_ATTRIBUTES = set("x y x1 x2 y1 y2 cx cy r rx ry width height stroke-width font-size opacity".split())
TOKENS = re.compile(r"[A-Za-z]+|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?|[^\s,]")
JS_SPACE = "\u0009\u000a\u000b\u000c\u000d\u0020\u00a0\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a\u2028\u2029\u202f\u205f\u3000\ufeff"


def numeric(value):
    text = value.strip(JS_SPACE)
    try:
        number = float(int(text, 0)) if re.match(r"^0[xob]", text, re.I) else float(text) if text else 0.
    except ValueError:
        return value
    return number if math.isfinite(number) else value


def attribute_name(element, key):
    if not key.startswith("{"):
        return key
    name = etree.QName(key)
    if name.namespace == "http://www.w3.org/XML/1998/namespace":
        return "xml:" + name.localname
    prefix = next((prefix for prefix, uri in element.nsmap.items() if uri == name.namespace and prefix), None)
    require(prefix is not None, "Missing SVG attribute namespace")
    return prefix + ":" + name.localname


def svg_tree(svg):
    root = etree.fromstring(svg.encode("utf-8"), etree.XMLParser(resolve_entities=False, no_network=True))
    require(etree.QName(root).localname == "svg", "Expected an SVG root")
    identified = [node for node in root.iterdescendants() if isinstance(node.tag, str) and "id" in node.attrib]
    ids = {node.get("id"): "resource-" + str(index) for index, node in enumerate(identified)}

    def reference(value):
        for ident, replacement in ids.items():
            value = value.replace("#" + ident, "#" + replacement)
        return value

    def walk(element):
        attrs = {}
        for name, value in sorted((attribute_name(element, key), value) for key, value in element.attrib.items()):
            if name in ("xmlns", "xmlns:xlink", "role", "focusable") or name.startswith("data-"):
                continue
            if element is root and name in ("width", "height"):
                continue
            if element is root and name == "style":
                require(re.fullmatch(r"\s*vertical-align:\s*[-.\d]+(?:ex|px)?;?\s*", value) is not None,
                        "Unsupported MathJax root style")
                continue
            if name == "id":
                attrs[name] = ids.get(value, value)
            elif name in ("d", "transform", "viewBox", "points"):
                attrs[name] = [numeric(token) for token in TOKENS.findall(value)]
            elif name in NUMERIC_ATTRIBUTES:
                attrs[name] = numeric(value)
            else:
                attrs[name] = reference(value)
        children = []
        if element.text and element.text.strip(JS_SPACE):
            children.append(element.text)
        for child in element:
            if isinstance(child.tag, str):
                observed = walk(child)
                if observed is not None:
                    children.append(observed)
            if child.tail and child.tail.strip(JS_SPACE):
                children.append(child.tail)
        tag = etree.QName(element).localname
        if element is not root and tag == "g" and not children and set(attrs) <= {"transform"}:
            return None
        return {"tag": tag, "attrs": attrs, "children": children}

    return walk(root)


def strict_equal(a, b):
    if isinstance(a, dict) and isinstance(b, dict):
        return a.keys() == b.keys() and all(strict_equal(value, b[key]) for key, value in a.items())
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(strict_equal(av, bv) for av, bv in zip(a, b))
    return (type(a) is type(b) or type(a) in (int, float) and type(b) in (int, float)) and a == b


def first_difference(expected, actual, path="$"):
    if strict_equal(expected, actual):
        return None
    if not isinstance(expected, (dict, list)) or not isinstance(actual, (dict, list)):
        return {"path": path, "expected": expected, "actual": actual}
    if type(expected) is not type(actual):
        return {"path": path, "reason": "container type differs"}
    e = expected if isinstance(expected, dict) else {str(i): value for i, value in enumerate(expected)}
    a = actual if isinstance(actual, dict) else {str(i): value for i, value in enumerate(actual)}
    if list(e) != list(a):
        return {"path": path, "expectedKeys": list(e), "actualKeys": list(a)}
    for key, value in e.items():
        difference = first_difference(value, a[key], path + "." + key)
        if difference:
            return difference
    return {"path": path, "reason": "value differs"}


def dimensions(reference, native):
    result = {}
    for key in ("width", "height", "baseline"):
        require(all(type(row[key]) in (int, float) and math.isfinite(row[key]) for row in (reference, native)),
                "Non-finite fragment " + key)
        result[key] = {"expected": reference[key], "actual": native[key], "difference": abs(reference[key] - native[key])}
    return result


def canvas(svg, width, height):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">'
            '<g transform="translate(1,1)">' + svg + '</g></svg>')
