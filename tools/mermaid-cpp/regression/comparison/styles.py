"""CSS cascade for measurement-free SVGs; never estimates text geometry."""

from __future__ import annotations

import math
import re

import cssselect2
import tinycss2
from lxml import etree
from tinycss2.color3 import parse_color

from .corpus import require


INHERITED = set((
    "visibility color fill stroke stroke-width fill-opacity stroke-opacity stroke-dasharray "
    "stroke-linecap stroke-linejoin font-size font-family font-weight font-style text-anchor "
    "dominant-baseline letter-spacing word-spacing text-decoration text-decoration-line "
    "text-shadow writing-mode text-orientation direction unicode-bidi baseline-shift"
).split())


def declarations(source):
    result = []
    for declaration in tinycss2.parse_declaration_list(source, skip_comments=True, skip_whitespace=True):
        require(declaration.type == "declaration", "Unsupported or malformed CSS declaration")
        result.append((declaration.lower_name, tinycss2.serialize(declaration.value).strip(), declaration.important))
    return result


class SvgStyles:
    def __init__(self, source):
        parser = etree.XMLParser(resolve_entities=False, no_network=True)
        self.root = etree.fromstring(source.encode("utf-8"), parser)
        require(etree.QName(self.root).localname == "svg", "Missing SVG root")
        self.matcher = cssselect2.Matcher()
        for element in self.root.iter():
            if etree.QName(element).localname != "style":
                continue
            for rule in tinycss2.parse_stylesheet("".join(element.itertext()), skip_comments=True, skip_whitespace=True):
                require(rule.type == "qualified-rule", "Unsupported CSS at-rule in measurement-free SVG")
                body = declarations(rule.content)
                selectors = cssselect2.compile_selector_list(rule.prelude)
                for selector in selectors:
                    require(selector.pseudo_element is None, "Unsupported CSS pseudo-element")
                    self.matcher.add_selector(selector, body)
        wrapper = cssselect2.ElementWrapper.from_xml_root(self.root)
        self.computed = {}
        for element in wrapper.iter_subtree():
            node = element.etree_element
            values = {name: ((0, (0, 0, 0), -1), value) for name, value in node.attrib.items()}
            for specificity, order, pseudo, body in self.matcher.match(element):
                for name, value, important in body:
                    key = int(important), specificity, order
                    if name not in values or key >= values[name][0]:
                        values[name] = key, value
            for name, value, important in declarations(node.get("style", "")):
                key = int(important), (1, 0, 0), math.inf
                if name not in values or key >= values[name][0]:
                    values[name] = key, value
            self.computed[node] = {name: value for name, (_, value) in values.items()}

    def value(self, node, name, fallback="", inherited=None):
        inherited = name in INHERITED if inherited is None else inherited
        current = node
        while current is not None:
            value = self.computed[current].get(name)
            if value not in (None, "", "inherit", "unset"):
                return fallback if value == "initial" else value.strip()
            if not inherited and value != "inherit":
                break
            current = current.getparent()
        return fallback

    def ancestors(self, node):
        while node is not None:
            yield node
            if node is self.root:
                break
            node = node.getparent()

    def color(self, node, value):
        if value == "none":
            return value
        if value.lower() == "currentcolor":
            value = self.value(node, "color", "black")
        color = parse_color(value)
        require(color is not None and color != "currentColor", f"Unsupported SVG paint {value}")
        r, g, b, alpha = color
        channels = ", ".join(str(math.floor(channel * 255 + .5)) for channel in (r, g, b))
        return f"rgb({channels})" if alpha == 1 else f"rgba({channels}, {alpha:g})"


def number(value, location):
    try:
        result = float(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f"Invalid SVG {location}: {value}") from error
    require(math.isfinite(result), f"Non-finite SVG {location}")
    return result


def css_number(value, location):
    match = re.match(r"^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?", value)
    require(match is not None, f"Invalid SVG {location}: {value}")
    return number(match.group(), location)
