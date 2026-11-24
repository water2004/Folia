"""SVG structure, upstream CSS projection and paint checks; no rendering engine."""

from copy import deepcopy
import math
import re

from lxml import etree


def require(condition, message):
    if not condition:
        raise ValueError(message)


def tag(node):
    return etree.QName(node).localname if node is not None and isinstance(node.tag, str) else None


def serialize(node):
    return etree.tostring(node, encoding="unicode", with_tail=False)


def document_roots(raw):
    parser = etree.XMLParser(resolve_entities=False, no_network=True)
    try:
        host = etree.fromstring(("<paint-document>" + raw + "</paint-document>").encode("utf-8"), parser)
    except etree.XMLSyntaxError as error:
        raise ValueError("Malformed SVG: " + str(error)) from error
    container = next((node for node in host if tag(node) == "mjx-container"), host)
    roots = [node for node in container if tag(node) == "svg"]
    require(bool(roots), "Result has no direct SVG roots")
    return roots


def parse(svg):
    roots = document_roots(svg)
    require(len(roots) == 1, "Expected a single SVG root")
    return roots[0]


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def dimensions(item, root, observation=None):
    if observation is not None:
        box, em = observation["rootBox"], item.get("em", 16)
        require(isinstance(box, dict) and not box.get("pwidth"), "Expected an intrinsic root viewport")
        scale = em * .5 / observation["xHeight"]
        return {"width": max(box["w"], em / 1000) * scale,
                "height": max(box["h"] + box["d"], em / 1000) * scale, "baseline": box["h"] * scale}
    result = {}
    for name in ("width", "height"):
        printed = root.get(name, "")
        require(re.fullmatch(r"\d+(?:\.\d+)?(?:ex|%)", printed) is not None, "Expected a supported document " + name)
        if printed.endswith("%"):
            require(name == "width", "Only document width can be a percentage")
            width = item.get("containerWidth")
            require(finite(width) and width > 0, "Percentage SVG needs the actual Source fixture container width")
            result[name] = float(printed[:-1]) * width / 100
        else:
            result[name] = float(printed[:-2]) * item.get("ex", item.get("em", 16) * .5)
    return result


def target_nodes(root, feature):
    selected = []
    for node in root.iter():
        kind, parent = tag(node), node.getparent()
        grandparent = parent.getparent() if parent is not None else None
        if ((feature == "table-lines" and kind == "line" and "data-line" in node.attrib)
                or (feature == "table-frame" and kind == "rect" and "data-frame" in node.attrib)
                or (feature == "enclosure-lines" and kind == "line" and "data-line" not in node.attrib)
                or (feature == "enclosure-curves" and (kind == "ellipse" or kind == "rect" and "rx" in node.attrib))
                or (feature == "glyphs" and kind in ("path", "use") and "data-c" in node.attrib)
                or (feature == "link" and kind == "a")
                or (feature == "error" and kind == "g" and node.get("data-mml-node") == "merror")
                or (feature == "table-viewport" and kind == "svg" and tag(parent) == tag(grandparent) == "g"
                    and grandparent.get("data-mml-node") == "mtable")):
            selected.append(node)
    return selected


def validate_styles(styles):
    rules = styles["rules"]
    require(isinstance(styles["css"], str) and bool(styles["css"]), "Missing actual MathJax stylesheet")
    expected = {"tableLine": {"stroke-width": "70px", "fill": "none"},
                "tableFrame": {"stroke-width": "70px", "fill": "none"},
                "dashed": {"stroke-dasharray": "140"},
                "dotted": {"stroke-linecap": "round", "stroke-dasharray": "0,140"}, "glyph": {"stroke-width": 3}}
    require(all(rules.get(key) == value for key, value in expected.items()), "Pinned MathJax paint rules changed")
    require(all(key in rules for key in ("root", "link", "errorText", "errorBackground", "tableViewport")), "Incomplete paint rules")


def materialize(root, rules):
    root = deepcopy(root)

    def apply(node, values):
        for name, value in values.items():
            text = str(value)
            if name == "stroke-width":
                text = re.sub(r"px$", "", text)
            if name == "stroke-dasharray":
                text = text.replace(",", " ")
            node.set(name, text)

    def table(node):
        return tag(node) == "g" and node.get("data-mml-node") == "mtable"

    def walk(node, parent=None, grandparent=None, parent_svg=True):
        kind = tag(node)
        if not kind:
            return
        svg = kind == "svg" or parent_svg and tag(parent) != "foreignObject"
        if node is root:
            apply(node, {"overflow": rules["root"]["overflow"]})
        if svg and kind == "a":
            apply(node, rules["link"])
        if svg and tag(parent) == "g" and parent.get("data-mml-node") == "merror":
            if kind == "g":
                apply(node, rules["errorText"])
            if kind == "rect" and "data-background" in node.attrib:
                apply(node, rules["errorBackground"])
        if svg and (table(parent) or tag(parent) == "g" and tag(grandparent) == "svg" and "data-table" in grandparent.attrib):
            if kind == "line" and "data-line" in node.attrib:
                apply(node, rules["tableLine"])
            if kind == "rect" and "data-frame" in node.attrib:
                apply(node, rules["tableFrame"])
            classes = node.get("class", "").split()
            for name in ("dashed", "dotted"):
                if "mjx-" + name in classes:
                    apply(node, rules[name])
        if svg and kind == "svg" and tag(parent) == "g" and table(grandparent):
            apply(node, rules["tableViewport"])
        if svg and kind in ("path", "use") and "data-c" in node.attrib:
            apply(node, rules["glyph"])
        for child in node:
            walk(child, node, parent, svg)

    walk(root)
    return root


def canvas(root, metrics, css=""):
    root = deepcopy(root)
    for key in ("width", "height"):
        root.set(key, str(metrics[key]))
    root.set("x", "4")
    root.set("y", "4")
    width, height = math.ceil(metrics["width"]) + 8, math.ceil(metrics["height"]) + 8
    style = "<style>" + css.replace("mjx-container", "svg[data-mathjax-host]") + "</style>" if css else ""
    return (f'<svg xmlns="http://www.w3.org/2000/svg" data-mathjax-host="" jax="SVG" '
            f'width="{width}" height="{height}" viewBox="0 0 {width} {height}">' + style + serialize(root) + "</svg>")


def paint_checks(root, item):
    group = next((node for node in root if tag(node) == "g"), None)
    checks = [{"name": "glyph-root-stroke-stays-zero", "passed": group is not None and group.get("stroke-width") == "0"}]
    for feature in ("table-lines", "table-frame"):
        nodes = target_nodes(root, feature)
        if feature in item["features"]:
            checks.append({"name": feature + "-present", "passed": bool(nodes)})
        for index, node in enumerate(nodes):
            kind = node.get("class")
            dash = "140" if kind == "mjx-dashed" else "0 140" if kind == "mjx-dotted" else None
            checks.append({"name": f"{feature}-{index}-paint", "passed": node.get("fill") == "none"
                           and node.get("stroke-width") == "70" and "stroke-thickness" not in node.attrib
                           and (not dash or node.get("stroke-dasharray") == dash)
                           and (kind != "mjx-dotted" or node.get("stroke-linecap") == "round"),
                           "attributes": [{"name": name, "value": value} for name, value in node.attrib.items()]})
    checks.extend({"name": "glyph-blacker", "passed": node.get("stroke-width") == "3"} for node in target_nodes(root, "glyphs"))
    checks.extend({"name": "table-overflow", "passed": node.get("overflow") == "visible"} for node in target_nodes(root, "table-viewport"))
    if "undefined-command" in item["features"]:
        nodes = target_nodes(root, "table-lines")
        checks.append({"name": "unknown-command-is-not-hline", "passed": len(nodes) == 1 and nodes[0].get("data-line") == "v"})
    return checks
