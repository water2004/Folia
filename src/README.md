# Source layout

The source tree is divided by ownership rather than by file type:

- `markdown/` owns the standalone block tree, lossless Markdown parsing, and serialization.
- `core/` owns document transactions, editing, queries, and render descriptions.
- `completion/`, `encoding/`, and `highlight/` own completion edits and prompts,
  document byte conversion, and source syntax analysis.
- `math/` owns native TeX rendering and a separate editing syntax session.
- `mermaid/` owns the standalone C++ Mermaid parser, layout, and SVG library (`folia::mermaid`).
- `platform/` adapts native graphics, text layout, media, storage, printing,
  and network services.
- `app-winui/` is the WinUI 3 composition root, interaction layer, and native renderer.

Dependencies point toward `core`. Core code must never include or import WinUI, WinRT, Direct2D, DirectWrite, DXGI, TSF, or other Windows APIs. Platform and application code may consume core modules, but Markdown mutations remain core commands and transactions.

```text
core (portable document semantics and render descriptions)
  ↑
platform (Windows adapters and deterministic display and interaction code)
  ↑
app-winui (window, resources, asynchronous orchestration, and drawing)
```

The platform layer is intentionally not a second presentation framework. It
is the home of services that work without WinUI. XAML controls, swap-chain
and frame ownership, document lifetimes, and user-facing workflows belong in
`app-winui`. See each directory's README and [the test layout](../tests/README.md)
for how those boundaries are exercised.

Folders are physical ownership boundaries; C++ module names remain the API boundary. A file should move or split when its responsibilities cross ownership boundaries, not merely because it exceeds a line-count threshold.
