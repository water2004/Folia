"""Complete Packet/Info SVG comparisons without synthetic font measurements."""

from __future__ import annotations

import json
import re
import subprocess
from pathlib import Path

from lxml import etree

from .corpus import digest, require
from .geometry import close_object
from .styles import SvgStyles, css_number, number


TEXT_DEFAULTS = {
    "rotate": ("none", "0", "0deg"), "lengthAdjust": ("spacing",),
    "letter-spacing": ("normal", "0", "0px"), "word-spacing": ("normal", "0", "0px"),
    "text-decoration": ("none",), "text-decoration-line": ("none",), "text-shadow": ("none",),
    "baseline-shift": ("baseline", "0", "0px"), "writing-mode": ("horizontal-tb", "lr", "lr-tb"),
    "text-orientation": ("mixed",), "direction": ("ltr",), "unicode-bidi": ("normal",),
}


def snapshot(source: str, family: str) -> dict:
    require(family in ("packet", "info"), "Unsupported measurement-free family")
    styles = SvgStyles(source)
    svg = styles.root
    view = svg.get("viewBox")
    view = None if view is None else [number(x, "viewBox") for x in re.split(r"[ ,]+", view.strip())]
    if family == "packet" or view is not None:
        require(view is not None and len(view) == 4, family + " viewBox")
    sizing = None
    if family == "info":
        sizing = {"width": svg.get("width"), "height": svg.get("height"),
                  "maxWidth": styles.value(svg, "max-width"), "maxHeight": styles.value(svg, "max-height"),
                  "preserveAspectRatio": svg.get("preserveAspectRatio", "xMidYMid meet")}
    elements = []
    for element in svg.iterdescendants():
        if any(etree.QName(n).localname in ("defs", "style", "title", "desc") for n in styles.ancestors(element)):
            continue
        tag = etree.QName(element).localname
        if tag == "g":
            require(not element.get("transform"), f"Unexpected transformed {family} group")
            continue
        require(tag == "text" or (family == "packet" and tag == "rect"), f"Unexpected {family} primitive {tag}")
        require(not element.get("transform"), f"Unexpected transformed {family} primitive")
        value = lambda name, fallback="": styles.value(element, name, fallback, inherited=True)
        numeric = lambda name, fallback=0: number(element.get(name, fallback), name)
        result = {"tag": tag, "x": numeric("x"), "y": numeric("y"),
                  "fill": styles.color(element, value("fill", "black")),
                  "stroke": styles.color(element, value("stroke", "none")),
                  "strokeWidth": css_number(value("stroke-width", "1"), "stroke-width"),
                  "fillOpacity": css_number(value("fill-opacity", "1"), "fill-opacity"),
                  "strokeOpacity": css_number(value("stroke-opacity", "1"), "stroke-opacity"),
                  "strokeDasharray": value("stroke-dasharray", "none"),
                  "strokeLinecap": value("stroke-linecap", "butt"), "strokeLinejoin": value("stroke-linejoin", "miter"),
                  "opacity": 1, "displayed": True, "visibility": styles.value(element, "visibility", "visible")}
        for node in styles.ancestors(element):
            if styles.value(node, "display") == "none":
                result["displayed"] = False
            result["opacity"] *= number(styles.value(node, "opacity", "1"), "opacity")
            for name in ("transform", "filter", "clip-path", "mask"):
                effect = styles.value(node, name)
                require(not effect or effect == "none", f"Unsupported {family} presentation {name}: {effect}")
        if tag == "rect":
            result.update({name: numeric(name) for name in ("width", "height", "rx", "ry")})
        else:
            for node in styles.ancestors(element):
                require("textLength" not in node.attrib, f"Unsupported {family} textLength")
                for name, permitted in TEXT_DEFAULTS.items():
                    for effect in (node.get(name), styles.value(node, name)):
                        require(not effect or effect.strip() in permitted, f"Unsupported {family} text effect {name}: {effect}")
            result.update({"dx": numeric("dx"), "dy": numeric("dy"), "text": "".join(element.itertext()),
                           "fontSize": css_number(value("font-size", "16px"), "font-size"),
                           "fontFamily": re.sub(r"\s*,\s*", ",", re.sub(r"[\"']", "", value("font-family"))).lower(),
                           "fontWeight": value("font-weight", "400").replace("normal", "400").replace("bold", "700"),
                           "fontStyle": value("font-style", "normal"), "anchor": value("text-anchor", "start"),
                           "baseline": value("dominant-baseline", "auto")})
        elements.append(result)
    def direct_text(tag):
        return next(("".join(n.itertext()) for n in svg if etree.QName(n).localname == tag), "")
    return {"view": view, **({"sizing": sizing} if sizing else {}), "elements": elements,
            "title": direct_text("title"), "description": direct_text("desc")}


def fontconfig_resolver():
    cache = {}
    def resolve(element):
        key = element["fontFamily"], element["fontWeight"], element["fontStyle"]
        if key in cache:
            return cache[key]
        weight = number(element["fontWeight"], "font weight")
        require(100 <= weight <= 900, "Unsupported font weight")
        weights = [0, 40, 50, 80, 100, 180, 200, 205, 210]
        index = next(i for i in range(9) if (i + 1) * 100 >= weight)
        resolved_weight = weights[index]
        if index > 0 and (index + 1) * 100 != weight:
            resolved_weight = weights[index - 1] + (weight - index * 100) / 100 * (weights[index] - weights[index - 1])
        slant = {"normal": 0, "italic": 100, "oblique": 110}.get(element["fontStyle"])
        require(slant is not None, "Unsupported font style")
        pattern = element["fontFamily"].replace("\\", "\\\\").replace(":", "\\:") + f":weight={resolved_weight:g}:slant={slant}"
        result = subprocess.run(["fc-match", "--format=%{file}\n%{family[0]}\n%{style[0]}\n%{index}\n", pattern],
                                check=True, capture_output=True, text=True).stdout.rstrip().split("\n")
        require(len(result) == 4, "fontconfig did not resolve an installed font")
        file, family, style, font_index = result
        require(bool(file and family) and Path(file).is_file(), "fontconfig did not resolve an installed font")
        font = {"requestedFamily": element["fontFamily"], "weight": element["fontWeight"],
                "requestedStyle": element["fontStyle"], "family": family, "style": style,
                "file": file, "index": font_index, "sha256": digest(Path(file).read_bytes())}
        cache[key] = font
        return font
    return resolve


def compare_svg(official, native, family, resolve_font=None):
    expected, actual, fonts = snapshot(official, family), snapshot(native, family), {}
    if resolve_font:
        for document in (expected, actual):
            for element in document["elements"]:
                if element["tag"] != "text":
                    continue
                font = resolve_font(element)
                require(bool(font.get("file") and font.get("sha256")), "Missing installed font evidence")
                fonts[json.dumps(font)] = font
                element["fontFamily"] = font["sha256"] + ":" + font["index"]
    close_object(actual, expected, family)
    return {"method": f"complete-{family}-svg", "elements": len(expected["elements"]),
            "textNodes": sum(e["tag"] == "text" for e in expected["elements"]), "rasterCompared": False,
            "fontResolution": "fontconfig" if resolve_font else "literal-css-family", "fonts": list(fonts.values())}
