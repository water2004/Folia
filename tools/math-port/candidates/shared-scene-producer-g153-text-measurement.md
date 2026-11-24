# Shared Scene producer and native text measurement candidate

This unified candidate applies on top of `tools/math-port/drafts/scene-attribute-parity-unified-g145.patch`. The G145 Scene owner and Lite mapping are the base. This packet adds the Scene producer boundary, the native Lite measurement provider, the paired-session probe, and its verification tools. Its `output_jax.*` changes are incremental against that base and retain the `ChildList::snapshot()` and index-based `ChildList::insert()` API.

`SvgOutputJax::produceScene(CommonWrapper&, DOMNodeRef)` owns the existing CommonWrapper/BBox, placement, inline-break, font-cache, and `Builder::finish()` work. `processMath()` passes that same owned result to the configured consumer. The default SVG consumer materializes it as before; `Renderer::renderScene()` captures it without SVG materialization or XML serialization. Both routes use one actual layout result.

Unknown-text measurement uses `CommonOutputJax::measureTextWithCache` and its existing cache key/result. The NativeRenderSession binds a weakly leased provider to its real LiteAdaptor and reuses a detached Lite text probe pair. Each cache miss updates the probe under a restore scope and calls the actual `LiteAdaptor::nodeSize`; Source text-content and option callbacks remain in the call path. An expired adaptor lease fails with TypeError. Other adaptors keep their SVG temporary-DOM path.

The paired probe runs the same 75 profile inputs and 38 packages in continuous and fresh-session groups. It compares the full frozen Scene tree and its glyph-outline resources, including ordered attributes, geometry, semantic payloads, paint, transforms, child order, and resource command data. Its materialization counter observes every public `Serializer::materialize(Scene)` entry on the current thread; its XML counter observes each Lite `serializeXML` entry. Counts are per conversion and remain available when a conversion throws. Scene-only conversions must enter neither route.

The focused native test covers the public `Renderer::renderScene` call, unknown-text scene-only/SVG output, the pinned Source cache capture/write order and reset behavior, recovery after the real Lite option callback throws, and nested re-entry through the same measurement provider with restoration of the outer probe text. It compares the reentrant outer Scene with a fresh-session Scene. The pinned cache witness runs against the actual locked MathJax CJS implementation.

The packet includes an independent paired probe target, preserving the existing public Renderer target. It also includes the renderer-boundary annotator and verifier. The public Renderer capture is compared separately with clean G150 Source for all 150 records; the paired NativeRenderSession capture checks zero scene-only serializer calls and complete Scene equality. These are separate evidence layers.

Historical G153 patch provenance (the runner and annotation tools are now maintained as Python; the MathJax source witness remains JavaScript). The original patch recorded these filenames and SHA-256 values: `tools/math-port/candidates/shared-scene-producer-g153/measure-text-cache-source-witness.cjs` = `A1A9899A91FBB0DF076810B0AAFC68FDC87432DC935F36828A39759598C823FA`; `tools/math-port/candidates/profile75-full-svg-g147/verify-scene-measurement-native.cjs` = `1F121C6FBA5D438C922EBA3F063D9C026A2D9881009CBE1F8B95C134FB67240C`; `tools/math-port/candidates/profile75-full-svg-g147/annotate-scene-measurement-renderer-boundary.cjs` = `785492ABCC7D33865E4D8AB2AE513E4639CC2739658C57DA971FA1F7428C019C`.

After the Root build gate has captured its manifest and binary audit, build both probe targets:

```powershell
cmake --build build/core --target FoliaMathProfile75FullSvgDump FoliaMathProfile75SceneMeasurementDump
python tools/math-port/candidates/profile75-full-svg-g147/verify-scene-measurement-native.py build/core/FoliaMathProfile75FullSvgDump.exe build/core/FoliaMathProfile75SceneMeasurementDump.exe <gate-number>
node tools/math-port/candidates/shared-scene-producer-g153/measure-text-cache-source-witness.cjs
```

The verifier requires `build/math-port/profile75-full-svg-source-clean-g150-root.json` and the matching Root gate artifacts. Standalone builds without those audited files do not establish the profile comparison. The candidate has not been built; only the pinned Source witness and JavaScript syntax checks have run.

`src/math/math.cpp/.ixx` also overlap the independent G147 cancellation candidate and must be reconciled during integration. This packet's Renderer methods provide the Scene API used by its public verification. Its CMake change adds only `FoliaMathProfile75SceneMeasurementDump` and keeps `FoliaMathProfile75FullSvgDump` unchanged.
