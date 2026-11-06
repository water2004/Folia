# P16a root acceptance — five complete concrete MML semantic sources

Status: locally accepted source semantics. Full TeX, Common layout, SVG and
Folia native replacement remain unaccepted; G0–G7 stay closed.

Root froze five interfaces, generated defaults/accessors/field bindings,
22-member AST inventory, source hashes and actual pinned-CJS expectations in
2d323d2 before constrained Luna implementation. Root reviewed all source members
and each submitted method against source order. Implementation/report batch:
dd3f3c7. No prototype parser/layout/renderer code was reused.

menclose calls current child0 with original predecessor, retains its result,
rereads the child after callbacks, then invokes virtual update on that current
nullable value and returns the retained result. mpadded uses raw vbox truthiness,
qualified layout superclass on the false branch, and virtual prev/class writes
before current child0 receives null on the true branch. maligngroup captures
attributes once, merges the live attribute record with original input through
the virtual helper, and propagates the returned handle with raw inheritance
arguments through the qualified layout superclass. mglyph captures attributes,
gets src/family/index and performs strict empty-string checks, then either the
exact short error with original options or qualified token verification.
malignmark contains only its complete generated defaults/accessors.

The accepted typed child-vector/nullish and error bool adaptations are explicit
in simple-mml-contracts.md. This is not arbitrary JS object/proxy reflection.
Native recursive dispatch retains existing call guards; no duplicate tree,
attribute merge algorithm, parser or second coordinate is introduced.

## Independent observations and rejected interim evidence

Final actual CJS oracle: 7,667 assertions / 106 cases / 1,605 raw matrix rows /
296 source value records. It covers every default/getter/field initialization,
fresh defaults identity, inherited mglyph arity, inferred rows, raw vbox and
strict glyph matrices, default/inherited/explicit lookup, options identity and
mutation, virtual callback order/rebindings/throws/partial state, nullable arrays,
returned/current child distinctions, merged-record aliases, raw inherited values,
real outer trees and real short/full errors. Expectations are sourced from the
verified pinned executable package, not from native output.

The read-only oracle review's glyph default/inherited lookup gap was filled by
eight actual-source cases. The prerequisite audit's "token no-child arity"
description was incorrect: actual mglyph inherits Infinity arity. Final source
observations and interface preserve Infinity; this correction is not an omitted
feature. Source default spread identity is separately compared for all five.

p16a-focused.log had an unbound generated Infinity spelling. p16a-focused2.log
failed 1,310 trace comparisons: a test mError override lacking the base short=false
default observed undefined for omitted superclass arguments, whereas the frozen
C++ bool boundary folds omission to false. Root independently observed two-
argument source calls before adding the same base default to the test observer.
No production algorithm or source result was modified to make these pass.
d9fe744 records the observer/Infinity corrections and expanded lookup cases.
Neither rejected interim log is acceptance evidence.

## Validation

Focused final build/test: build/p16a-focused3.log, 106 tests / 7,667 assertions
passed. Complete core build/test: build/p16a-core-final.log, successful terminal
exit 0 and complete suite passed. Generated bindings and oracle reproduce with
--check. Complete pinned AST ledger has all 22 reviewed members with targets.

42 mutants compile and fail unchanged assertions with exit 1. They cover
predecessor/update/reread/return, raw truthiness, superclass/prev/class order,
strict glyph comparisons/message/short flag/options identity/superclass boundary,
merge arguments/result/raw inherited values, each added default, mark arity/
spacelike, ORD initialization and each fresh-default alias. First mutation run's
options-clone mutant lacked its helper module import and is rejected compile
evidence. Root added the import to that mutant only. Final results are
build/math-port-simple-mml-mutations/results.json and build/p16a-mutations2.log;
all 42 are assertion failures, not crashes or failed compilations. Harness byte
checks verify working implementations and fixtures unchanged throughout.

Local source acceptance is 41/246, with 205 sources still required, including
maction/MathChoice/mfenced/factory, TeX packages, fonts, Common layout and SVG.
These five semantic classes do not establish output geometry or production
readiness. Math and the newly requested Mermaid native backend objectives both
remain active at this checkpoint. The earlier Mermaid work is recorded in Git history.
