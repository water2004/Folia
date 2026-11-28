# Large-document layout architecture

This note records the design constraints for scaling layout and editing across
large block trees. Benchmark sources, reproduction commands, and measurements
belong to [`tools/document-scalability`](../tools/document-scalability/README.md).

## Semantic and physical ownership

The unified block tree remains the sole semantic document model. Editable leaves
own exact Markdown source and a lossless inline CST; layout fragmentation must
not alter source, node identity, selection coordinates, or serialization.

A semantic block may produce multiple physical render fragments. Split only at
safe flow boundaries, preserve inherited container/style context, and keep
semantic boundaries distinct from artificial fragment boundaries. Map editable
owners to fragments directly so local edits can find and update their affected
layout units without expanding a whole large list.

## Geometry and visual payload

Keep measured fragment geometry in a compact index so scrolling and hit-testing
can locate content without retaining every visual object. Materialize visual
payloads for the viewport and interaction targets; release off-screen layouts,
line records, source/display maps, and paint plans while preserving their exact
measured heights. Re-materialization must produce equivalent content and
geometry. Use the real platform measurer for text; character-count estimates are
not valid layout data.

Initial parsing and exact measurement remain proportional to document size.
Changes to width, font, or other metric-affecting style can require broad
remeasurement. Middle insertions may shift navigation ranks, and structural
changes with broad semantic effects may expand invalidation. Do not describe all
edits as constant-time.

## Verification contract

Tests should exercise edits at the beginning, middle, and end of large lists;
undo/redo; fragment creation and release; exact total and per-fragment geometry;
and preservation of source, CST, node IDs, and selection. Work counters must
measure actual traversal, index writes, and fragment rebuilds rather than only
reporting that an incremental API was called.

Portable core results do not validate DirectWrite, WinUI scrolling, native input,
or GPU/resource behavior. Keep those claims tied to Windows application tests.
The test harness and retained benchmark evidence are documented with their
owners in the tools and tests directories.
