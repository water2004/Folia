# Historical Rust Mermaid backend checkpoint

## Current direction (2026-10-21)

The current request is to build Folia's C++ Mermaid dependency from the official
mermaid-js/mermaid JS/TS sources, not to port mermaid-rs-renderer or resume this
Rust Direct2D experiment. See `../mermaid-cpp/README.md` for the new source pin,
dependency boundary and verification. The historical work below remains
preserved; its local checks do not establish acceptance of the C++ replacement.

## Historical pause (2026-10-12)

Paused by the user on 2026-10-12: record progress, leave Mermaid aside, finish
native MathJax first. No Mermaid agent is assigned further work. Future Mermaid
delegation uses Luna, with Root-owned architecture, responsibilities and gates.

Useful completed pieces:

- Original parser/layout retained; upstream version pinned and 23 diagram
  families inventoried. Original observations exist as comparison references.
- Immutable DiagramScene/SceneBuilder, ordered portable geometry/paint/group/
  clip/text/image/resource types and partial typed compiler leaf helpers.
- Native Direct2D scene consumer with real primitive/resource handling and
  eleven local WARP tests. These do not accept complete Mermaid diagrams.
- Last relevant accepted extraction: 8b7a0a3, shared dimensions/viewBox logic
  moved unchanged into scene/compile/dimensions.rs. Root verified the entire
  extraction/surrounding render source, portable build and twelve existing
  renderer unit tests. See evidence/m5a-dimensions-acceptance.md.

Actual remaining main route:

1. Complete Layout -> DiagramScene family lowering and explicit shared text
   resolution, preserving original painter order, placement, styles and options.
2. Add the SVG consumer of that same finished scene. Public SVG entry points
   currently still call the original Layout -> render_svg path.
3. Independently accept complete Mermaid input through SVG and native D2D,
   then integrate the accepted native path in Folia. Folia is not wired yet.

Root previously expanded the work into unnecessary tiny-skia CPU raster,
sampling/pipeline/shader/blitter ports. Those packets are not prerequisites
for D2D. Do not resume that frontier. See work-scope-review.md and the scope
correction at the top of architecture.md; historical later prerequisites are
superseded. Existing files remain preserved pending cleanup.

Pending working-tree artifacts at pause: m3j3-constructor-source-contract.md
schema correction, m3j3-constructor-source-freeze.md, the blitter-constructor
fixture/observer/oracle and its Rust reference project. They are Source-only
candidate observations, not accepted Native blitter work or integration gates.
Preserve them; they are outside the prioritized MathJax edits.
