# Incomplete native Mermaid candidates — 2026-10-28

The active contribution tree remains the validated 17-family implementation at
`f98c996643719ab7036e397d08b391f9bb73bd8a`. These source-only patches preserve
unfinished work for reassignment. They are not applied by the build or CI and
must not be treated as accepted renderers. No execution instructions or machine
artifacts are included.

| Candidate | Preserved work | Verification still needed |
| --- | --- | --- |
| native-entry.patch | C4 public entry; ELK input model order and Flowchart public entry; Architecture measured SVG; State classic/Dagre native entry; ER and ZenUML original oracles | C4 same-host SVG; corrected Flowchart/Architecture Windows build and SVG; State native build/tests and SVG. ER/ZenUML have no native implementation. |
| class-elk.patch | Five-case measured Class ELK scene, original browser tools and partial label postprocessing | Source extraction fails at placedTerminalLabels; fixture/tests remain seven columns while oracle prepares nine; native build, painter, entry and SVG are incomplete. |
| class-elk-shared.patch | Shared measured ELK scene and terminal-label interface | Apply together with Class candidate, then verify existing Flowchart behavior. |

`native-entry.patch` applies to the exact active baseline above and represents
functional source state `d313c746`; fork scheduling documents are excluded.
Apply it only in a separate checkout with `git apply --check` first. The Class
patches follow it; the shared patch currently needs `--ignore-space-change`.
Do not apply candidates to a newer shared boundary without reviewing overlap.
SHA-256 values refer to raw repository patch bytes; retain their line endings.

The 17-family baseline passed all four Windows configurations in run
[40296270685](https://github.com/zhao0349/Folia/actions/runs/40296270685).
C4 core/platform checks passed in earlier standalone runs, but its browser
acceptance remains separate. The newer Flowchart/Architecture gate stopped on
an MSVC structured-binding test capture; its source correction is preserved
but has not completed a replacement gate. State original oracle passes eight
DB cases and two syntax errors with eleven source pins, and its original
sixteen SVG samples were captured; native parity is unverified. ER has only
eight DB cases and two syntax errors verified, without completed browser
capture. ZenUML has five original parser/layout/SVG cases verified; its pinned
preparation script and MIT notice are included.

Next work should start from these explicit gaps rather than reopening the
accepted baseline or assuming these candidate renderers are ready.
