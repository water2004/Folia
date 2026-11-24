"""Capture actual browser evidence, then compare it with portable Python code."""

from __future__ import annotations

import io
from pathlib import Path

from PIL import Image

from .corpus import require
from .geometry import GEOMETRY_TOLERANCE, capture_viewport, close_object, compare_text, near
from .pixels import CHANNEL_TOLERANCE, RASTER_SCALE, compare_pixels


def capture(page, source, family):
    page.set_content('<!doctype html><html><head><meta charset="utf-8">'
                     '<style>html,body{margin:0;padding:0;background:transparent}'
                     'body>svg{vertical-align:top}</style></head><body>' + source + '</body></html>', wait_until="load")
    dimensions = """() => {
      const svg=document.querySelector('svg'); if(!svg)throw new Error('Missing SVG root');
      const b=svg.getBoundingClientRect(),v=svg.viewBox.baseVal,s=getComputedStyle(svg);
      return {viewBoxPresent:svg.hasAttribute('viewBox'),view:{width:v.width,height:v.height},
        bounds:{width:b.width,height:b.height},sizing:{width:svg.getAttribute('width'),height:svg.getAttribute('height'),
        maxWidth:s.maxWidth,maxHeight:s.maxHeight,preserveAspectRatio:svg.getAttribute('preserveAspectRatio')??'xMidYMid meet'}};
    }"""
    measured = page.evaluate(dimensions)
    sizing = measured.pop("sizing")
    viewport = capture_viewport(family=family, **measured)
    page.evaluate("""async v => {
      const svg=document.querySelector('svg');
      if(!v.intrinsic){svg.style.width=v.width+'px';svg.style.height=v.height+'px';
        svg.style.maxWidth='none';svg.style.maxHeight='none';
        document.body.style.width=Math.ceil(v.width)+'px';document.body.style.height=Math.ceil(v.height)+'px';}
      await document.fonts.ready;
      await new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)));
    }""", viewport)
    measured = page.evaluate(dimensions)
    measured.pop("sizing")
    viewport = capture_viewport(family=family, **measured)
    observe = Path(__file__).with_name("observe-svg.js").read_text(encoding="utf-8")
    snapshot = page.evaluate("(" + observe + ")", {"viewport": viewport})
    import math
    png = page.screenshot(omit_background=True, clip={"x": 0, "y": 0,
        "width": math.ceil(viewport["width"]), "height": math.ceil(viewport["height"])})
    image = Image.open(io.BytesIO(png)).convert("RGBA")
    return snapshot, {"width": image.width, "height": image.height, "data": image.tobytes()}, sizing


def compare_browser(page, official, native, family):
    expected, expected_pixels, expected_sizing = capture(page, official, family)
    actual, actual_pixels, actual_sizing = capture(page, native, family)
    if family == "info":
        close_object(actual_sizing, expected_sizing, "Info sizing contract")
    near(actual["width"], expected["width"], "Viewport width")
    near(actual["height"], expected["height"], "Viewport height")
    text = compare_text(actual["texts"], expected["texts"])
    require(actual["paints"] == expected["paints"], "Visible paint palette")
    require(actual["accessibility"] == expected["accessibility"], "SVG accessibility")
    pixels = compare_pixels(actual=actual_pixels, expected=expected_pixels, scale=RASTER_SCALE,
                            tolerance=GEOMETRY_TOLERANCE, colorTolerance=CHANNEL_TOLERANCE)
    require(pixels["ok"], "Raster differs: " + str(pixels))
    return {"method": "browser-graphemes-and-local-raster", "rasterCompared": True, "text": text, "pixels": pixels}
