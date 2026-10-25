# Inline syntax icons

These original SVG assets preserve the spelling of emphasis, strong, strike,
code and math delimiters. A shared 24-unit square, rounded 1.7-unit strokes,
six-unit tile radius and quiet translucent surface keep the set consistent.
No font, text element, external reference or runtime file lookup is used.
Opening and closing symmetric delimiters share the exact same asset.

Theme JSON files embed the SVG strings under `syntaxMarkers.icons`, together
with `sizeEm` and `gapEm`. These source assets also supply the executable's
complete fail-safe theme through `markers.rc2`. Keep both copies consistent
when changing the shipped artwork. See [theme format](../../../../docs/THEMES.md).
`EditorSyntaxIcons` owns only the finite device resource cache, invalidated on
theme/device changes. The core render builder supplies explicit icon identities
from complete CST nodes and their delimiter ranges; there is no UI spelling
lookup. Incomplete syntax, arbitrary text, link destinations and HTML are not
substituted. Display math consumes its block CST delimiter tokens in the same way.
DirectWrite reserves one indivisible square plus clearance for the whole marker.
The painter uses those same measured bounds. The interaction map projects source
offsets onto that object for caret placement without splitting its layout.
There is no additional source,
selection or editing model. Longer/unmapped spellings remain editable text.

The display replacement pattern is informed by CodeMirror's widget/replacement
decorations: <https://codemirror.net/examples/decoration/>. This is a native
DirectWrite/Direct2D implementation, not a claim that CodeMirror ships this icon set.

`test_syntax_icons.cpp` checks layout and actual rendered pixels. Set
`FOLIA_SYNTAX_ICON_PREVIEW` to an ignored PNG path when running WinUI tests to
export the light/dark 16/20/32 DIP review sheet from the production SVG renderer.
