# P8b normal MML node port evidence

Status: implementation submitted for root source review; no build or oracle run
was performed by this packet. The accepted root fixture is currently reported
as 1,037 assertions across 32 cases. That count is fixture inventory, not a
claim that this implementation passes it.

## Pinned source and ownership

The implementation is based on MathJax 4.1.3 `ts/core/MmlTree/MmlNode.ts`
(SHA-256 `cc0594eb5782928412c4307637866d9f80ac147af577297edd48ee88b63b8656`)
and the root-pinned CJS runtime file (SHA-256
`86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45`). The
read-only member ledger reports 111 members in the file, 85 executable members
across the whole file. This packet implements the `AbstractMmlNode` class and
its module-level MML policy tables only; token, layout, base, empty, text, and
XML branches belong to the sibling packets.

The only implementation file is
[`mml_node.cpp`](../../../src/math/core/mml_tree/mml_node.cpp). It is an
implementation unit of the root-frozen `folia.math.core.mml_tree.mml_node`
module, imports the host binding and `std`, and retains the MathJax Apache-2.0
provenance header. `TEXCLASS`, `TEXCLASSNAMES`, `attributeTuple`,
`unpackAttribute`, and the four normal/token/layout/base defaults reference
functions are supplied by root-owned declarations/bindings and are not
redefined here.

## Assigned-member ledger

Source line numbers below refer to the pinned TypeScript file. Each item maps
the executable member to the native implementation responsibility.

| MathJax member | Source lines | Native mapping |
| --- | ---: | --- |
| `TEXSPACELENGTH`, `TEXSPACE` | 71–95 | Private constexpr 4-entry length table and complete signed 8×8 TeX spacing table. |
| `MATHVARIANTS`, `indentAttributes` | 97–125 | Mutable module-level native vectors; `MATHVARIANTS` membership uses SameValueZero. |
| `AbstractMmlNode` constructor body | 443–460 | `initializeMml`: generic storage initialization, inferred-row creation/parenting, virtual `setChildren`, class defaults lookup, virtual `setAttributes`, then `setList`. |
| `copy` | 467–489 | Current-registry create, shallow property assignment, explicit-attribute copy with root-only `id` filtering, inferred-row unwrapping, live captured-array traversal and null-child preservation. Child copies do not receive `keepIds`. |
| TeX/attribute state declarations and `texClass` accessors | 403–413, 497–508 | Native accessors over nullable TeX-class state; semantic methods dispatch through the virtual getters/setters so concrete nodes may override computed classes. |
| `isToken`, `isEmbellished`, `isSpacelike`, `linebreakContainer`, `linebreakAlign`, `isEmpty`, `arity`, `isInferred`, `notParent`, `Parent` | 511–589 | Defaults, captured-array/live-length empty check, infinite arity, and recursive logical-parent traversal. Recursive methods use the shared tree guard. |
| `setChildren`, `appendChild`, `replaceChild` | 595–646 | Inferred-child routing; infinite-arity inferred-row flatten uses captured `forEach` length; fixed-arity conversion preserves attributes/properties; generic mutation retains parent/error ordering. |
| `core`, `coreMO`, `coreIndex`, `childPosition` | 650–689 | Self-core defaults and raw-parent traversal followed by iteration of a captured sibling array. |
| `setTeXclass`, `updateTeXclass`, `getPrevClass` | 694–724 | Virtual TeX-class accessors, previous scriptlevel transfer, and right-associative clearing (`prevLevel` before `prevClass`). |
| `texSpacing` | 726–744 | Nullable TeX classes, JS `|| ORD`, raw `prevLevel > 0` conversion, invalid-row TypeError versus invalid-column Missing, negative spacing retention, complete table lookup. The invalid-column path still evaluates the scriptlevel comparison first, including its `prevLevel` short-circuit, before returning Missing for `Math.abs(undefined)`. |
| `hasSpacingAttributes` | 746–749 | Short-circuit embellished check, then virtual `coreMO` delegation. |
| `setInheritedAttributes` | 765–843 | Own-default/always-inherit gates, origin-specific `noInherit`, copied `stopInherit` removal, display/script/prime inheritance, finite arity trim/pad, linebreak indentation augmentation, and child dispatch in source order. |
| `setChildInheritedAttributes` | 846–856 | Captured child-array iterator with live length and null-child failure at dispatch. |
| `addInheritedAttributes` | 866–885 | Shallow assignment clone, exclusion of only `displaystyle`, `scriptlevel`, and `style`, origin tuple creation. |
| `inheritAttributesFrom` | 889–897 | Source attribute and prime reads, conditional `mathsize` tuple, call into inherited-attribute path. |
| `verifyTree` | 906–931 | Null-option no-op; attribute verification, finite arity check/error, then child verification. |
| `verifyAttributes` | 934–970 | Ordered explicit names, `data-*` and standard-name exceptions, default lookup, mutable mathvariant membership and ignore-property truthiness. |
| `verifyChildren` | 973–978 | Captured array/live-length child dispatch and null-child failure. |
| `mError` | 987–1012 | Parent-merror suppression, error creation/message first, full/short/wrapping forms, parent replacement and append ordering, short title assignment. The initial attributes handle is captured for the message write; short-error title writing re-reads and checks the attributes handle after virtual parent replacement, preserving a replacement-time TypeError instead of dereferencing null. |
| `noInherit`, `stopInherit`, `alwaysInherit`, `verifyDefaults` | 335–394 | Live mutable `PropertyList` records matching the pinned nested property sets and Boolean defaults. |

## Native host adaptations

The root contract supplies the shared node tree, factory/session ownership,
`Attributes`, `PropertyList`, `Property` (including distinct Missing and null),
tuple records, scalar JS truthiness/number/string operations, and a 512-deep
`TreeCallGuard`. `initializeMml` is a post-adoption constructor phase because
C++ virtual dispatch cannot reproduce the TS constructor's derived virtual
calls. Missing own class defaults map to an empty assignment source, while
missing math defaults and invalid native `std::any` bindings raise the frozen
TypeError. Explicit null children remain a failing `setChildren` input rather
than being treated as omitted.

Native vectors represent the source arrays. Each ported `for...of` captures the
vector handle and checks its current length; the inferred-row flatten path
captures length as `Array.forEach` does. The representation has no sparse-hole
state distinct from an explicit null child, as fixed by the root contract.
Attribute tuples are the root-owned two-slot records, not a second object or
array model. The valid `MATHVARIANTS` names are stored in an ordered mutable
vector because the frozen C++ declaration uses that type; validation preserves
JS Set membership semantics.

## Review items and remaining boundary

Root has not accepted the source or run the frozen differential suite yet.
Remaining review is to compare every mapped member against the actual TS/CJS
ordering, validate the new independent replacement/live-array witnesses, and
run root-owned builds, fixtures, and mutations. This packet does not implement
concrete MathML node classes, token/layout/base branches, parsing, operator
dictionary behavior, rendering, or SVG output.
