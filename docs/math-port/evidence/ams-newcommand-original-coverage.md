# AMS/Newcommand original-source coverage ledger

The rows below come from actual pinned MathJax CJS execution in
`tools/math-port/ams-newcommand-original-oracle.cjs`. Its source lock records
the nine TS/CJS hashes and produces the 24 complete input rows plus nine
registration snapshots. The native candidate/comparator must retain this case
set and the full `result`, `error`, `dynamicMaps`, and `packageData` values.

| Source unit/member | Current original witnesses | Direct branch gaps |
| --- | --- | --- |
| `NewcommandMethods.NewCommand` | `newcommand-simple`, `newcommand-redefinition`, `newcommand-optional-default`, `newcommand-optional-explicit`, `newcommand-invalid-count`, `protected-recursive-limit`; supplemental standard-name redefinition, missing-definition, illegal control-sequence name, and explicit-empty-optional cases | `protectedMacros` option rejection, malformed nested braces, and invalid optional-default syntax are not directly covered. |
| `NewcommandMethods.NewEnvironment` / `BeginEnvItem` | `new-environment`, `new-environment-optional`; supplemental omitted/empty-default, explicit-empty-optional, and mismatched-end cases | Global definition behavior remains uncovered. |
| `NewcommandMethods.MacroDef` / `MacroWithTemplate` | `def-parameter-template`, `def-optional-brace`; supplemental delimited-match/mismatch, runaway, and missing-control-sequence cases | Malformed parameter sequence is not directly covered. |
| `NewcommandMethods.Let` | `let-control-sequence`; supplemental undefined control sequence, special character/delimiter, existing-newcommand, and existing-character-token cases | Delimiter replacement remains uncovered. |
| `NewcommandUtil.GetCSname`, `GetCsNameArgument`, `GetArgCount`, `GetTemplate`, `GetParameter`, `MatchParam`, `addMacro`, `addEnvironment`, `undefineMacro` | Reached through the corresponding command/environment cases above | There is no isolated direct-method fixture. `checkProtectedMacros`, `addDelimiter`, and `undefineDelimiter` have no direct input witness in this corpus. |
| `AmsMethods.AmsEqnArray`, `AlignAt`, `HandleTag` | `ams-align`, `ams-align-star`, `ams-tag-reference`; supplemental invalid alignat count, alignedat pair count, duplicate/starred tag, and untaggable split cases | Alignment overflow/limits and malformed tag text remain uncovered. |
| `AmsMethods.Multline` / `AmsItems.MultlineItem` | `ams-multline`; supplemental shove outside/after content, shoveright at row start, and multi-integral with limits | Invalid row count, empty table, and left-side tag placement are not directly covered. |
| `AmsMethods.FlalignArray` / `AmsItems.FlalignItem` | `ams-flalign`; supplemental xalignat row | Padded/centered forms, row overflow, and empty-row paths are not directly covered. |
| `AmsMethods.HandleOperatorName`, `SideSet`, `xArrow`, `Genfrac` | `ams-operatorname`, `ams-sideset`, `ams-arrow`, `ams-binomial`; supplemental operatorname limits, empty/bottom arrow labels, and invalid/text genfrac styles | Starred operatorname parsing, x-arrow sizing variants, and other genfrac argument variants remain uncovered. |
| `AmsMethods.HandleDeclareOp`, `XalignAt`, `operatorLetter`, `MultiIntegral`, `HandleShove`, `CFrac` | Supplemental starred/unstarred DeclareMathOperator, xalignat, MultiIntegral limits, both HandleShove branches, and illegal/right-aligned CFrac | `operatorLetter` direct dispatch and broader CFrac valid-align branches need dedicated witnesses. |
| Newcommand/AMS mapping initializers | All cases initialize the real package configurations; the oracle records complete static map registration metadata and dynamic map state per TeX instance. | The current parse corpus does not exercise every static character, delimiter, command, and environment entry. |

This is an explicit coverage boundary, not an acceptance statement. The 24
cases establish broad real-package execution and strict whole-tree comparison;
they do not independently prove the unobserved branches listed above.

## Supplemental branch corpus

`tools/math-port/ams-newcommand-boundary-oracle.cjs` runs an additional 36
observations directly against the same locked CJS files and rechecks every CJS
hash recorded by the original corpus. It writes
`tools/math-port/ams-newcommand-reference/ams-newcommand-boundary-source.json`;
the 24-row original JSON and its cases are unchanged. A separate native
candidate and strict comparator live at
`tools/math-port/ams-newcommand-reference/ams-newcommand-boundary-native-candidate.cpp`
and `tools/math-port/compare-ams-newcommand-boundary.py`. They compare the
same complete MML tree/error, dynamic map entries, and packageData snapshots.
The candidate deliberately shares the frozen candidate's actual RuntimeGraph
and serializer, rather than inventing a reduced node representation.

The supplemental cases include `newcommand-standard-name-redefinition`,
`newcommand-missing-definition`, `newcommand-illegal-control-sequence-name`,
empty and explicitly empty optional arguments, delimited argument
match/mismatch/runaway and missing control sequence, let-to-undefined/existing
macro and character bindings, special character/delimiter lets, omitted and
explicit-empty environment defaults,
`new-environment-mismatched-end`, `declare-math-operator-unstarred`,
`declare-math-operator-starred`, `ams-arrow-empty-labels`,
`ams-shove-outside-multline`, `ams-shove-after-row-content`,
`ams-cfrac-illegal-alignment`, right-aligned cfrac, duplicate/starred tags,
untaggable split, invalid alignat count, two-pair alignedat, invalid/text
genfrac styles, xalignat, both shove positions, bottom arrow label, operatorname
limits, and multi-integral limits. These add branch witnesses but do not by themselves
close every member gap in the table above; in particular utility methods and
all static map entries still need direct behavior coverage.
