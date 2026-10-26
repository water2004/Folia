# Third-party notices

Folia includes and uses third-party software. The complete, version-specific
license notices distributed with the application are generated at:

`src/app-winui/Assets/licenses/THIRD-PARTY-NOTICES.txt`

The generated bundle is authoritative. It includes the repository's Boost.UT
test fork, the SRELL search engine, QuickJS and MathJax, Tree-sitter and its
language grammars, ICU4C charset detection and conversion, the native
SVG/Mermaid Rust dependency graph, Windows App
SDK/NuGet packages, and bundled npm packages. Dependencies that are present
only because the Windows App SDK package graph restores them are listed too;
their presence does not imply that Folia embeds a WebView editor.

Regenerate the file after changing native, NuGet, Cargo, or npm dependencies:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\generate-third-party-notices.ps1
```

Folia itself is licensed under the MIT License in `LICENSE`.

## Fluent UI System Icons

Workspace icons use the Panel Left, Folder, Document, Document Add, Folder Add, Arrow Clockwise and Square Multiple 20 Regular SVG paths from https://github.com/microsoft/fluentui-system-icons at commit 82ecf7cbdb43684403c7125efa160be04a33dca5. The original vector paths are retained in WorkspaceIcons.cpp; the right-panel variant mirrors Panel Left for an identical silhouette. They are rendered by native XAML paths inside their complete SVG viewboxes with theme foreground colors.

MIT License

Copyright (c) 2020 Microsoft Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
