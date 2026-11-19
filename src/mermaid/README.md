# folia::mermaid

Platform-independent C++23 Mermaid parsing, configuration, layout and SVG.
The implementation is pinned to Mermaid 12.2.0; hosts supply actual font and
math measurements. Windows services live in `src/platform/text/`.

Start with the [architecture and ownership guide](../../docs/mermaid-cpp/architecture.md)
and [build/test guide](../../docs/mermaid-cpp/README.md). The `api/` directory owns
public orchestration, `diagrams/<family>/` owns each diagram implementation,
and `frontend/`, `config/`, `themes/`, `layout/`, `text/`, `rendering/` and
`support/` contain the corresponding shared services.

C++ module names remain independent of these paths. Keep generated configuration
and theme data with their owners, and update the generating script together with
any path change. Run `python3 tools/mermaid-cpp/check-structure.py` from the
repository root; the standalone test project also registers this check in CTest.
