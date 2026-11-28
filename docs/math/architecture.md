# Native math architecture and maintenance

Folia renders TeX with the C++23 library in `src/math`. The production renderer
does not run JavaScript. Its public C++ entry point is `import folia.math;`;
Windows and WinUI call the native renderer through the C ABI.

For validation requirements, see [verification.md](verification.md). The
MediaWiki compatibility package has a separate [package boundary](mediawiki-texvc.md).
The repository’s [MathJax profile](../../tools/math-port/profiles/scope.json),
source locks, reference data, and generators are build and verification inputs;
they are not migration notes or disposable generated output. Third-party
licenses are recorded in the repository’s [third-party notices](../../THIRD_PARTY_NOTICES.md).

## Pipeline and ownership

```text
TeX input
  → input/tex: package registration, tokenization, parsing and macro state
  → core/mml_tree: document-owned nodes, attributes and semantic families
  → output/common: font metrics, measurement, mathematical layout and placement
  → output/scene: immutable, fully laid-out values
  → output/svg: serialization of the completed Scene
```

There is one mathematical layout implementation. `output/scene` owns its
completed result; `output/svg` serializes it and does not recompute geometry.
Any future drawing backend must consume that same Scene and must not introduce
a second layout path.

| Area | Current responsibility |
| --- | --- |
| `src/math/math.ixx` and `math.cpp` | Configure the supported profile, organize single or batch requests, and return SVG, Scene, or owned errors. |
| `src/math/input/tex/` | TeX packages, parser stack, token handling, macro expansion and session semantics. |
| `src/math/core/mml_tree/` | MML node families, attributes, tree ownership, cloning and semantic state. |
| `src/math/output/common/` | Mathematical measurement, font selection, stretching, line breaking, layout and placement. |
| `src/math/output/scene/` | Immutable drawing values and the complete geometry needed by consumers. |
| `src/math/output/svg/` | SVG structure, escaping, resource ordering and numeric spelling. |
| `src/math/fonts/` | Immutable font tables and request-scoped glyph data. |
| `src/math/handlers/`, `adaptors/`, `dependencies/` | HTML document behavior, the Lite adaptor, and focused vendored native dependencies. |
| `src/math/native_renderer_bridge.cpp` | C ABI boundary for the native renderer DLL. |
| `src/platform/windows/media/math/` | Windows worker scheduling, cancellation, result caching and DLL sessions. |
| `src/app-winui/editor/rendering/` | Editor viewport demand, formula previews and presentation. |

Core math modules use C++23 modules and `import std;`. They remain independent
of Windows, WinRT, Direct2D and DirectWrite headers. Platform APIs stay in
`src/platform` or the application layer. Keep each implementation in its
existing semantic owner; new files need a real consumer and a distinct
responsibility. Use typed native values and explicit domain callbacks; do not
restore a generic JavaScript object/prototype runtime or a parallel legacy math
implementation.

## Public request and lifetime contract

`folia::math::Renderer` owns one configured TeX session and is thread-affine:
create, call and destroy it on the same thread. `renderBatch` is the shared
request path for one or more formulas. A `RenderInput` carries UTF-16 TeX,
cancellation, options and a timeout. Results own their SVG bytes and immutable
Scene independently of later requests and Renderer destruction. Per-item errors
in a batch remain isolated.

The opaque C ABI exposes renderer, cancellation and result handles; no C++ or
STL object crosses that boundary. Its UTF-16 input length is measured in code
units. Cancellation may be requested from another thread, but the cancellation
handle must remain alive until the render returns. Renderer destruction has a
caller-thread precondition.

MML nodes, Common wrappers, layout temporaries and decoded glyph bodies belong
to a request and unwind on success, error, exception or cancellation. Persistent
session state is limited to TeX semantics and small immutable lookup structures.
Font requirements are deduplicated and loaded for a render batch; decoded glyph
bodies are not retained across requests. Scene values retain only resources
they need to draw.

The result’s `reusableWidthFrom` value describes when its completed geometry is
valid at other container widths. An absent value means the result is valid only
at the width used for that render. Consumers may reuse geometry only according
to this returned contract; they must not infer width dependence from TeX text,
scale numbered equations, or substitute a guessed width.

## Rendering and application boundary

Standalone display fragments and document math use the same parser, Common
layout and SVG writer. The fragment C ABI returns an explicit pixel viewport and
baseline for embedding. The editor requests formula work asynchronously; UI
threads do not perform TeX layout or synchronous font-file reads. Cancellation,
document generations and offscreen demand prevent stale or unnecessary results
from being presented.

SVG output is self-contained. Layout and paint properties come from Scene;
the writer must preserve paint, clipping, resource order and Source-required
transform spelling. It must not fill missing data by recalculating layout or by
post-processing serialized SVG. Unsupported input or output features must be
reported explicitly instead of silently approximated.

Keep the ordered transform sequence in Scene so SVG can serialize Source order
and another backend can compose the same values. A glyph resource is selected
by its semantic font variant and codepoint; a Source cache identifier is not a
substitute for that identity. Preserve the original pinned SVG path bytes for
serialization. If the typed outline decoder encounters malformed path data,
retain the raw SVG data and report the decoder’s typed diagnostic; do not guess
coordinates or silently drop the glyph.

The editor’s text measurement boundary is platform-owned. When CSS font-family
lists are passed to DirectWrite, resolve them to an installed family first and
use measured advances; do not estimate width from character counts. Keep text
coordinates and Unicode conversion consistent at the core/platform boundary.

## Source profile and package scope

The reference behavior is the pinned MathJax 4.1.3 profile in
[`tools/math-port/profiles/scope.json`](../../tools/math-port/profiles/scope.json).
Folia’s default package set contains the 38 non-conflicting packages from that
profile. The separately documented `mediawiki-texvc` package supplies aliases
that MathJax 4 no longer includes. Existing v4 definitions retain precedence.

Generators and witness tools in `tools/math-port/` are offline development
tools. They read pinned upstream files and emit immutable native tables or
independent expected observations. Production does not load the JavaScript
engine. Keep the generator, its inputs, source lock and generated native data in
sync when moving or changing source data.

## Mhchem integration

MathJax’s `mhchem` package uses `mhchemparser` 4.2.1. The configuration module
in `src/math/input/tex/mhchem/` owns MathJax command and character bindings,
replacements, and the `Machine` and `relmo` callbacks. It sends generated TeX
back through the ordinary TeX parser.

| Upstream parser responsibility | File under `src/math/dependencies/mhchem_parser/` |
| --- | --- |
| `toTex`, `createTransitions`, and the parser loop | `parser.cpp` |
| Parsed values, buffers, matches, and transitions | `parser.ixx` |
| Pattern definitions, capture handling, and matching | `patterns.cpp` and `patterns_data.inc` |
| Generic and machine-specific actions | `actions.cpp` |
| Ordered state transitions | `transitions_data.inc` |
| `_mhchemTexify` and the arrow, bond, and operator maps | `texify.cpp` |

The data generator transcribes pattern literals and ordered transition tables
from upstream. C++ owns matching, actions, recursion, errors, and TeX output.
The source and output hashes are recorded in the [mhchem source lock](../../tools/math-port/locks/mhchem-source-lock.json).
