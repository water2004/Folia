# Built-in math completions

The shipped `src/app-winui/Assets/latex/commands.json` is generated data. Completion
does not construct a math renderer or execute JavaScript at runtime.

- `registry.json` records the active finite command, delimiter and environment
  maps from the locked MathJax 4.1.3 package profile, including texvc aliases. Its
  digest binds it to the existing upstream registry fixture checked against native
  `folia::math` by `tests/math/tex_package_registry.cpp`.
  The source data is derived from MathJax (Apache-2.0); see
  `src/math/LICENSE-MathJax.txt` for the upstream license.
- `templates.json` owns editor conveniences and curated argument templates. Stable
  ids retain usage ranking. Native `sub` and `sup` retain their subset/supremum
  meanings; script conveniences are `subscript` and `superscript`.
- `methods.json` gives the argument/template contract of each parser method.
  Macro expansion arity and optional defaults come from registry metadata.
  Unknown parser methods fail generation instead of silently receiving a bare
  command. `environments.json` owns environment arguments, bodies and established
  ids; all registered environments receive begin/end candidates from one recipe.
- Control-space and the row separator are typed directly. Templates that need an
  enclosing array, proof tree or declaration context retain those TeX semantics.

Regenerate after changing the locked package profile:

```powershell
node tools/math-port/tex-package-conformance.cjs --registry-only
python tools/latex-completion/generate.py
```

For ordinary template edits only the second command is needed. Check the data:

```powershell
python tools/latex-completion/generate.py --check
python -m unittest discover -s tests/query -p test_latex_catalog.py
```

Core tests cover prefix replacement, ranking and limits. Native application tests
load the packaged catalog, parse every snippet with Folia's snippet parser and
exercise acceptance, parameter navigation and exact source/selection history.
