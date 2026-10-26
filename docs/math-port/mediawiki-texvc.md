# MediaWiki TeX aliases

Folia eagerly loads the 38 non-conflicting MathJax 4.1.3 TeX packages in the
original profile, plus `mediawiki-texvc`. MathJax's current extension list says
that the latter has not been ported to v3/v4:
https://docs.mathjax.org/en/latest/input/tex/extensions/mediawiki-texvc.html

The aliases come from the complete 101-entry MathJax 2.7.9 definition table:
https://github.com/mathjax/MathJax/blob/2.7.9/unpacked/extensions/TeX/mediawiki-texvc.js
The pristine Apache-2.0 file is retained in
`tools/math-port/reference/mediawiki-texvc-2.7.9.js`; its SHA256 is checked by the
generator. This adds 95 missing names, including `\lang`, `\rang`, `\R`, `\N`,
`\Z`, `\infin`, `\isin`, and arrow/suit/Greek aliases.

The native package has priority 6. Existing v4 packages have higher priority,
so their six existing names (`AA`, `Bbb`, `oiint`, `oiiint`, `real`,
`textvisiblespace`) keep their v4 meanings. User definitions retain their
ordinary TeX precedence and grouping behavior. This deliberately differs from
loading the old v2 extension over v4 and overwriting current definitions.

`\lang` and `\rang` are also registered as delimiter aliases so that
`\left\lang ... \right\rang`, `\middle\lang`, and `\bigl\lang` work. Macro
expansion alone cannot provide this: MathJax has a separate delimiter handler.

## Responsibilities

- `src/math/input/tex/default_packages.*`: production package composition,
  extracted from the former renderer constructor without changing the order
  or options of the original 38 packages. Tests use this same composition.
- `src/math/input/tex/mediawiki_texvc/configuration.*`: only the compatibility
  package's command/delimiter maps and priority.
- `alias_data.inc`: generated immutable definitions, no parser or renderer logic.
- `tools/math-port/tex-package-conformance.cjs`: offline reference generation
  using the locked MathJax 4.1.3 distribution and pristine v2 alias table;
  also adds missing aliases to the bundled completion catalog, preserving
  existing entries/snippets. No JS is executed by the application.
- `tests/math/tex_package_registry.cpp` and generated `*_source.hpp` fixtures:
  production registry and actual individual/batch renderer conformance.

The generator checks the installed version, dependency lock, profile hash,
and the existing locked runtime/font source hashes. Run it with Node, then build/run `FoliaMathTests`.
It compares 1,763 entries across 75 finite maps, including effective handler
precedence and character values. There are 216 byte-exact reference SVG cases
covering all legacy names in inline/display modes, explicit definition groups,
delimiter sizing, and continued visible handling of unknown commands. The
existing full-profile tests continue to cover v4 package behavior. This is a
registration and regression check, not a claim that every possible TeX input
has been exhaustively tested.

## Other v4 packages

The only package directories outside the original profile are `autoload`,
`require`, `colorv2`, and `noerrors`. The first two are dynamic package loaders;
Folia registers the syntax packages eagerly. `colorv2` is the incompatible old
alternative to active `color`; `noerrors` replaces error presentation. These
are intentional exclusions, not missing symbol packages. The original locked
`scope.json` and reference profile are retained unchanged; the compatibility
extension and audit results are recorded separately in
`evidence/tex-package-conformance.json`.
