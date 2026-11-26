# Third-party notices

Folia includes and uses third-party software. The complete, version-specific
license notices distributed with the application are generated at:

`src/app-winui/Assets/licenses/THIRD-PARTY-NOTICES.txt`

The generated bundle is authoritative. It includes the repository's Boost.UT
test fork, the SRELL search engine, MathJax and its fonts, Tree-sitter and its
language grammars, ICU4C charset detection and conversion, the usvg normalizer
dependency graph, and Windows App SDK/NuGet packages. Native Mermaid notices
are in `src/mermaid/THIRD_PARTY_NOTICES.md`. Dependencies that are present
only because the Windows App SDK package graph restores them are listed too;
their presence does not imply that Folia embeds a WebView editor.

Regenerate the file after changing native, NuGet, or Cargo dependencies:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\generate-third-party-notices.ps1
```

Folia itself is licensed under the MIT License in `LICENSE`.

## Inline completion

- Model protocol adapters: MIT, Fairy contributors, vendored at
  `ef60077797ddadc82ee934d2e9d80ce05cf67830`. Source and license are in
  `third_party/model-providers`; Folia has no Fairy installation or runtime dependency.
- nlohmann/json 3.12.0: MIT. Source, license and hash are in
  `third_party/nlohmann-json`.
