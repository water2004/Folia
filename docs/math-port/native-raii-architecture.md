# Native C++ math architecture

This is the detailed ownership and mathematical-layout reference for the current
native implementation. The public entry point and module map are in
[architecture.md](architecture.md). The pinned MathJax profile and fonts are in
`scope.json`; the additional alias package is documented in `mediawiki-texvc.md`.
Source algorithms remain the reference. Context/ObjectRef and JS object-semantic
simulation are not part of the native architecture.

## Request-scoped font data (implemented 2026-10-22)

The user defines demand as one request, which may contain a bounded batch of
formulas. There is one batch entry point; a single formula is a batch of one.
Deduplicate glyph requirements within that request and read/decode them in
groups. Do not preserve glyph bodies or decoded outlines between requests,
including through an LRU, a persistent provider cache, or a renderer session.
An idle native renderer retains the TeX session and small lookup indexes.
`NewcmFontSource` shares immutable metadata and compiled read-only tables;
`NewcmFontRequest` alone owns the loaded glyph map for one batch. Common and
SVG project values from that same request. The existing single-item helpers
delegate to the batch implementation rather than retaining a separate path.

The font source is file-backed, read-only data, without SQLite. The reader's
batch contract must return request-owned values or explicit lifetime leases,
never pointers into a temporary read buffer. Platform mapping/handle APIs stay
outside core. The existing compiled read-only font tables are source data;
there is no separate font-pack file. Engine construction does not expand every
row into heap objects. Storage lookup preserves pinned variant inheritance, shallow
link overlays, SMP remaps, extension precedence, SVG aliases, present-empty
paths, compositions, and outline diagnostics. Missing or invalid data is a
reported error, not a switch to the eager implementation.

| Owner | Responsibility |
|---|---|
| `fonts/newcm_font_data.*` and its existing generator | Pinned read-only records/indexes and exact source metadata; no eager outline construction |
| `fonts/newcm_font_module_provider.*` | One request's deduplicated batch reads and typed font values; release all loaded bodies at request completion/cancellation/failure |
| `output/common/font_data.*` | Mathematical metrics, variant/remap resolution and a borrow from the active request; no Windows APIs, disk queue, or persistent glyph-body cache |
| `output/svg/font_data.*` | Exact SVG path/content access from the same request; no second font selection implementation |
| `math.*` / native C ABI | One batch request lifecycle shared by startup computation and editing; persistent lightweight parser, independent per-formula MML ownership/results |
| Folia math worker | Bounded startup batches, asynchronous preview, request cancellation; file access never on the UI thread |

Completed Scene results still own only the glyph resources they actually use,
as required by the existing immutable Scene contract. A Scene must not capture
the complete font request or renderer. SVG results retain no font source/body
after serialization. Returning a Scene must remain valid after its request and
engine are destroyed.

Acceptance must include batch-of-one versus multi-formula equivalence against
the same pinned Source, glyph deduplication inside a batch, exact metrics/SVG
aliases/outlines, and retained-Scene ownership. Measure cold and repeated
requests while leaving the engine alive, and verify zero loaded font bodies
after success, cancellation and errors. Compare the user's original native
runtime-rendering baseline (`2d58ded9`) with the same document and memory metric;
the font-only measurement alone does not explain the reported 128→218 MB delta.
Keep the scrolling checks: obsolete queued reads/native jobs are removed;
already-started obsolete reads cannot publish bytes or trigger repaint. No
opening-time full-document visual layout, HasMath document classification,
UI-thread file IO, or disk-backed editing preview is introduced.

## Pipeline and responsibilities

```text
TeX string + RenderOptions
    -> TeX input jax / package handlers / parser stack
    -> request-owned MML document
    -> CommonWrapper + BBox + font metrics + placement
    -> immutable mathematical Scene
    -> SVG writer
    -> existing Folia SVG path
```

The future Direct2D consumer takes the identical completed Scene. It performs
no TeX parsing, stretching, mathematical spacing, baseline calculation, table
layout or script placement. SVG performs none of those calculations either.
Both backends consume one completed layout. Packages such as Bussproofs may
perform Source-required Common subtree measurements while compiling MML;
those reuse the same mathematical implementation before the final Scene is
produced, without backend-specific placement. XML spelling, escaping, attribute
order and numeric formatting belong to SVG serialization.

MathJax's TeX packages, NodeFactory, MmlFactory/MmlNode/Attributes, CommonWrapper,
BBox, font providers and SVG responsibilities remain recognizable. C++ classes,
enums, value records and typed callbacks replace JS prototypes, mixins and
reflective dispatch. Source mathematical algorithms remain the reference;
arbitrary JS mutation behavior is no longer a native API requirement.

## Source structure correction (2026-10-20)

The upstream file responsibilities are the starting architecture, not merely
algorithm references. Removing JS object semantics does not justify replacing
the Common/MML/TeX family structure with a new collection of helper modules.
This section supersedes the earlier `output/common/placement/` allocation and
the fraction/radical packet paths. The numerical portions now replace those
canonical Common family files. The standalone placement files and corresponding
old Scene family files/registrations have been removed.

Review of `b030b874` found two problems: the new fraction/radical services had
only test callers, and their equations coexisted on disk with the superseded
Common and Scene family implementations. Duplicate family arithmetic has now
been consolidated. The former scalar APIs were replaced in place by actual
CommonMfrac/CommonMsqrt/CommonMroot classes in `95a119b1`; they now participate
in the native factory, MML, font, BBox and recursive linebreak collaboration.
The native Renderer now drives these families through the complete typed TeX,
MML, Common and Scene graph and the application worker consumes its SVG output.
The earlier component-only integration gap is closed; current acceptance and
remaining inactive-source cleanup are recorded in HANDOFF.md.

| Pinned upstream responsibility | Native owning files | Required boundary |
|---|---|---|
| `output/common/Wrappers/mfrac.ts` | `output/common/wrappers/mfrac.ixx/.cpp` | Fraction attributes, child metrics, ruled/atop/bevel measurement, slash stretching and width reconciliation |
| `output/common/Wrappers/msqrt.ts` | `output/common/wrappers/msqrt.ixx/.cpp` | Shared radical logic, surd stretching, p/q and base measurement |
| `output/common/Wrappers/mroot.ts` | `output/common/wrappers/mroot.ixx/.cpp` | Degree-specific dimensions/height; retain its relationship to msqrt |
| Numerical parts of SVG `Wrappers/mfrac.ts`, `msqrt.ts`, `mroot.ts` | The corresponding Common family above | Calculate child origins and rule geometry once, preserving separate Source measurement and painting quantities |
| SVG elements, attribute order, grouping and number spelling | `output/svg/` | Consume completed layout; no second mathematical placement calculation |
| Completed owned geometry | `output/scene/` | Store/freeze values; no node-family wrapper hierarchy or layout equations |
| `core/MmlTree/MmlNodes/mfrac.ts`, `msqrt.ts`, `mroot.ts` | Corresponding `core/mml_tree/mml_nodes/` files | MML structure, inheritance and semantics, distinct from output layout |

The numerical placement adjustment is necessary for the requested shared
SVG/Direct2D result: upstream SVG wrappers currently contain mathematical
positions as well as SVG construction. Move only the mathematical part to its
Common family. Preserve the rest of the upstream responsibility separation.
This is not a reason to create a separate placement family beside Common.

Replace canonical family files in place and retire superseded implementations
and their registrations in the same responsibility-sized change. Do not merely
move a free-function experiment into a wrapper-named file and claim the wrapper
is complete. MML/font/child/stretch orchestration and actual callers must be
reviewed too. Existing inactive consumers require explicit rewrite/removal
decisions; absence from CMake alone is not source cleanup.

File names describe their MathJax responsibility. Do not add `native_`, `typed_`,
`new_` or equivalent suffixes to distinguish a replacement from its original
owner. Rewrite the owning module and remove superseded code and registrations
within the reviewed change. `util/function_list.ixx` owns FunctionList;
`tests/math/function_list.cpp` owns its behavior tests. A source file left out
of CMake is still unfinished cleanup when remaining consumers reference it.

### Tag formatting ownership (2026-10-20)

`tagformat/tag_format_configuration.*` remains the single package owner. It
registers defaults and a priority-10 configuration callback. The selected Tags
creator is captured by value in a factory owned by ParserConfiguration; its
product remains uniquely owned by ParseOptions. The local catalog entry is
`configTags`, without a process-wide counter or permanent class registration.

`tags.*` retains numbering, equation/session state, tag MML construction and
default formatting. Its public formatting dispatch applies the configured
callbacks before the selected class's default formatting hooks, preserving the
Source outer-subclass precedence without synthesizing classes or prototypes.
Formatting borrows the input-jax TexOptionValues record and reads its current
TagFormatOptions on each call; it never borrows a temporary optional payload.
The same original default algorithms supply both ordinary Tags and the package
defaults. No second tag builder, numbering algorithm or MML owner is introduced.

### Setoptions and typed option access (2026-10-20)

`setoptions/set_options_configuration.*` retains the original package's three
filters, macro and configuration callback. Its private field dispatch maps
Source names to the existing finite C++ option members. It owns no mirrored
option dictionary and exposes no arbitrary property access. ParseOptions only
adds a const borrow of its ConfigurationHandler for the package-name check.

The TeX input domain is boolean/string/RegExp. Boolean fields use the incoming
primitive's truth value; regular-expression fields require an actual RegExp;
`digits` retains its existing string/RegExp union. Resource limits are numeric
comparison thresholds stored as native doubles, including fractions, negative
values, NaN and infinities. Counters and allocation sizes remain unsigned
integers. The setter only converts the numeric input; ParseUtil and the array
parser perform their original comparisons. It must not duplicate those
comparisons by projecting thresholds to integer capacities in the setter. Strings
remain strings, with `noundefined`'s false styling projected to an empty string
to preserve its disabled-style output. Arrays, maps and function callbacks are
configured through their existing typed C++ fields, not synthesized from TeX
strings. The nullable reference formatter may be cleared by false/empty input,
retaining its native callback type. Incompatible writes produce InvalidValue
rather than reviving arbitrary JS type mutation. Permission and field-presence
checks still precede conversion,
and successful earlier writes remain when a later key fails, as in Source.

### Empheq table transformations (2026-10-20)

The original `EmpheqUtil.ts` remains `empheq/util.*`: option parsing, allowed
environments, column counting, copied table/first-row phantoms, side cells and
table adjustment. `EmpheqConfiguration.ts` remains `empheq/configuration.*`:
Empheq environment entry/exit, token commands, delimiter commands and maps.
There is no separate Empheq item class, method module or table-layout service.

Utilities receive MmlNode references and TexParser references; the existing MML
document owns every node. Nested TeX parsers are automatic local objects that
borrow the current ParseOptions and copy the current environment. Snapshot an
inferred row's child pointers before reparenting them. Table copies use the
original ParseUtil::copyNode, preserving node-list registration. The outer
phantom in topRowTable uses the MML factory as in Source, while other new nodes
use the configured parser creators. BeginItem owns only its optional left/right
TeX strings and existing nestStart flag. StackGlobal.empheq and the environment
closing marker retain their existing owners; tags use the existing equation
state. No universal property store or parser/node identity registry is added.

False and empty side values disable that side; false is represented by the
empty native TeX string. Source bare true sides fail in TexParser.trim, and
equation* with only a right side fails on missing columnspacing. Native calls
reject these inputs with invalid_argument, without emulating JS exception
objects. Record them separately from successful MML comparisons and verify
the following numbered equation and label state recover.

Acceptance must compare actual Source MML for all six permitted environments
and their starred forms, left/right/both sides, labeled/ragged/empty tables,
all 32 fixed symbols and four delimiter commands, labels/tags, option parsing,
environment argument splitting, nesting/errors and the next parse. Direct
utility tests also check copied-node independence, parent links and all moved
inferred children. These package checks do not prove final Scene/SVG integration.

### Cases numbering and text column (2026-10-20)

The single upstream `CasesConfiguration.ts` maps to `cases/configuration.*`.
CasesBeginItem, CasesTags, CasesMethods and their registrations belong there;
the previous items/methods/tags module pairs are removed, without forwarding
modules. CasesBeginItem still derives from BeginItem and owns its one-shot end
marker. CasesTags still derives from AmsTags and owns its subcounter; the
existing equation state owns equation counters, labels and tag records.

NumCases retains the original duplicate Push of the same begin frame: the
first matching end pops one activation with an empty replay, and the final end
completes the remaining activation of that frame.
The original ArrayItem::numCases flag and BeginItem::left string already exist;
do not add another property store or alternate cases-array implementation.
Side material uses the accepted EmpheqUtil::left and original ParseUtil copies.
Entry dispatches ordinary arrays back to BaseMethods::Entry, otherwise scans
the second column with the original brace/control-sequence stopping rules,
trims only leading whitespace and invokes original internalMath at level zero.
Its explicit null result means ContinueDispatch: the base character map also
processes that entry afterward. Preserve that continuation and the resulting
extra cell/column metadata; NoParseResult would incorrectly stop dispatch.

Freeze signatures in the existing configuration module: initializeModule takes
ConfigurationHandler& and MapHandler&; NumCases takes TexParser&, FrameHandle
const&, span<TexArgument const> and returns StackEntry. Entry is a MacroHandler;
environment is an EnvironmentHandler and forwards its full argument span.
CasesBeginItem exposes its native bool end marker and overrides kind/checkItem.
CasesTags overrides start/autoTag and the inherited formatNumberDefault hook;
configured tagformat callbacks retain their existing outer precedence.

Acceptance compares independently generated Source MML for both environments,
text/math and escaped/nested second columns, row endings, tags/labels/references,
tag sides, selected tag modes and tagformat, empty/error/recovery states and
ordinary array fallback. Direct tests verify the one-shot end consumption,
number/subnumber formatting including explicit zero, environment argument
forwarding and real native factory products. These remain package checks,
separate from the final Common-to-Scene/SVG gate.

### Mathtools responsibilities and tag composition (2026-10-20)

Keep the six original Mathtools owners: configuration selects package handlers,
defaults, the multlined factory and fixPrescripts; mappings contains immutable
command/environment/delimiter tables; methods implements the original macro and
environment algorithms; items extends AMS MultlineItem; util owns the six original
helpers; tags owns named forms, the selected form and MathtoolsTagFormat. Existing
Base/AMS/Newcommand methods stay their original owners, referenced directly by
the mappings rather than copied into Mathtools. Do not introduce another table
assembler, parser, option store, tag-numbering class or rendering family.

The selected Tags product remains uniquely owned by ParseOptions, retaining its
actual AMS/Cases/custom parent behavior. AbstractTags owns one optional TagForm
strategy through unique_ptr; its narrow interface returns an optional formatted
tag (no result means use the parent). MathtoolsTags in mathtools/tags.* is the
concrete strategy and owns its finite string triples and optional current triple.
Selecting a form copies its triple, so replacing that name leaves the selected
value unchanged until the next selection, as in Source. No strategy owns or
captures a parser, Tags product, request, MML node or input jax.
The native option uses array<u16string,3>. Source validates the array length but
allows non-string elements; that arbitrary option mutation is outside this typed
API, rather than a runtime shape/coercion facility to reproduce.

setTagForm and the existing setFormatters record which was installed last for
formatTag precedence. Only these two formatting layers occur in this profile;
do not introduce a generic dynamic subclass/extension registry. A selected form
installed after Tagformat overrides its tag callback; an unselected form falls
through. Tagformat installed after Mathtools overrides the form. Other formatters
and parent numbering/finalization remain on the original Tags object. Factories
are configuration-local and capture the original parent creator by value; neither
global generated class names nor retained prototype chains are ported.

The full package is not accepted until its original mappings, methods, options,
factory, postfilter and tag composition run together through real TeX compilation
against pinned Source. A tag strategy test alone is component evidence only.

The whole-package port retains all 122 registrations in five original maps;
the reproducible mapping generator reads the pinned pristine Source tables and
checks their fixed fingerprint. Five environment handlers return StackEntry,
27 macro handlers use MacroInvocation/DispatchOutcome, and the six inherited
aliases name their original Base/AMS/Newcommand callbacks. Normal macro returns
are NoParseResult; explicit null-frame pushes stay at their Source call sites.

Paired delimiter definitions retain finite TexArgument values and immutable
callback IDs. GetArgCount without brackets represents Source undefined, which
activates PairedDelimiters' default count 1. An explicit empty bracket stays an
empty string; the string "0" remains truthy for substitution with zero arguments.
Do not confuse any of these with an explicit NullArgument. MakeAboxedCommand
writes the current definition layer directly; paired-delimiter declarations use
NewcommandUtil::addMacro and its original scope/protection/delimiter handling.

Mathtools' 20 fields comprise 14 strings, four flags and two typed maps. The
legacycolonsymbols field alone needs an owned bool-or-string value: Source
compares its previous value strictly, then selects add/remove by the old value's
truthiness. The other two settable flags have truthiness-only consumers. Field
presence, complete key validation and assignment order remain distinct. Source's
misspelled pariedDelimiters exclusion is preserved when recognizing keys; scalar
replacement of the real typed pairedDelimiters map, or bool replacement of a
string field, is rejected at the native boundary. No second option store exists.

The existing Unicode lowercase primitive and fixed Unicode 16 data are reused
for MathMakeBox's position comparison. They operate only on UTF-16 values and
do not restore runtime object or callable services. Source token construction
without text remains nullopt, distinct from an explicitly empty text node.

### Colortbl array extension (2026-10-20)

The one upstream `ColortblConfiguration.ts` remains `colortbl/configuration.*`.
ColorData, ColorArrayItem, TableColor and the priority-10 configuration belong
there. Remove the earlier extra items/methods pairs without forwarding modules.
ColorArrayItem derives from ArrayItem, reuses its EndEntry/EndRow/createMml,
and overrides the existing array factory entry. Make ArrayItem::createMml
virtual so the original array assembly dispatches to the extension normally.
Do not introduce a second table assembler or alter EqnArray's family.

ColorData owns cell/row strings and a vector of optional column strings; an
unset column must remain distinct from a defined empty color in the later-row
comparison. hasColor is local to the activation. Cell beats row, which beats
column; cell color clears after a colored entry, row color after EndRow, and
column settings persist for that array. New MML remains document-owned.

TableColor is a MacroHandler with immutable cell/row/col string arguments.
Resolve the existing session ColorModel and parse model/definition before
checking the active array, preserving original error ordering and partial
state updates. The configuration callback receives ParserConfiguration& and
TeX&; if the session has no model, invoke the already registered color package's
actual config callback. Do not activate its command map, add loaders or replace
an existing model. initializeModule takes ConfigurationHandler& and MapHandler&.

Acceptance compares real Source MML for precedence/reset, sparse columns,
preambles, later-row restrictions, ignored overlap arguments, all existing
color models, nested/fenced/styled/framed arrays, errors and next requests.
Verify the actual factory product/virtual createMml path, priority 10, callback
identity and registered-color config behavior with and without selected color.
These package checks do not establish final Scene/SVG integration.

The original `input/tex/ParseMethods.ts` belongs at `input/tex/parse_methods`,
not under `base/`. Its ten compiled methods handle variables, numbers, control
sequences, mapped character tokens and the environment entry. BaseMethods,
BaseMappings and BaseConfiguration retain their separate upstream duties.
ParseUtil owns getFontDef and Unicode-normalized Latin/Greek classification;
TexParser passes the existing NodeDefinition through its configured creators.
Properties such as autoOP must reach that creator before construction, whereas
mathchar0mo sets fixStretchy and registers the node after construction, as in
Source. Token attributes remain owned values, with absent versus empty explicit.
TexEnvironment additionally carries the specific multi-letter RegExp reference
and noAutoOP flag. Shared expression state models the actual configured pattern;
it contains no parser, frame or document owner and creates no object registry.

BaseMethods keeps all 75 upstream members in the original base/base_methods
module. The 70 character/macro members are compiled MacroHandler functions
`DispatchOutcome(TexParser&, MacroInvocation const&)`; Array, AlignedArray,
IndentAlign, Equation and EqnArray are EnvironmentBodyHandler functions returning
StackEntry with a borrowed parser, real begin FrameHandle and TexArgument span.
Definition arguments use the existing finite TexArgument types, never Property,
any, FunctionRef or a runtime method object. Original mathematical routines are
adapted in place, not retained beside a new dispatcher. ParseUtil owns its
shared text/under-over/alignment/equation/expansion helpers. BaseMappings owns
the pinned table data and direct binding of those compiled functions; package
configuration and filters remain BaseConfiguration responsibilities. Review
checks all original methods and real configured-parser trees/errors against
the pinned source, with no success stubs for pending members.

The input-jax compilation service uses the original core/input_jax owner with
compile-time Options/Data parameters. Its filter record borrows MathItem,
MmlDocument and the concrete ParseOptions; filters execute synchronously and
retain no record identity. TeX owns its configuration, ParseOptions and session;
the compiled registries/callbacks and current MML factory are borrowed services.
The caller supplies the document owning each compilation's nodes and keeps its
factory alive until rebinding or jax destruction. TeX never accumulates MML in
a session arena. Its compilation executes the original package filters and seven
FilterUtil passes before Common sees a tree. FilterUtil has its own original
module, rather than continuing as free functions mixed into tex_jax.cpp.
HTML delimiter discovery/DOM adaptor methods are outside this string compile
service and remain explicit pending owners; they are not replaced by fake DOM
results or called through the withdrawn object layer.

The token creator takes `optional<u16string_view>` through TexParser and
NodeFactory. Explicit null text creates no TextNode; an explicitly empty text
and the omitted-text default create an empty TextNode. This is the original creator collaboration,
including the configured boldsymbol token creator, rather than a second node
construction path for hidden delimiters. Boldsymbol owns its command mapping,
scoped argument parsing, token marking and priority-5 variant rewrite in its
original configuration module. Its scope is restored before pushing the result.

C++ module boundaries follow owners and dependencies. A packet, individual
function or test seam does not automatically need its own named module.
Family implementation details and numerical intermediates remain private
unless a real production consumer needs them. Native ownership changes remain
typed values, unique owners and explicit borrowed references within a request.

The Source class collaborations are requirements too, not only the file paths:
`CommonWrapperFactory` owns request-local wrappers; `CommonWrapper` owns common
wrapper state and algorithms; `CommonMfrac` specializes fractions;
`CommonMsqrt` specializes shared radical behavior; `CommonMroot` extends
`CommonMsqrt` with degree behavior. Preserve their Source method responsibilities
and initialization/call order using native C++ classes and RAII. Do not replace
this collaboration with a public collection of scalar-input/result functions.
The numerical consolidation in b23b2b46 does not implement these classes and
does not satisfy wrapper architecture acceptance. Reusable equations may become
private implementation details of their owning family during that port; their
current exported input/result/function surface is not the final wrapper API.

### Bussproofs responsibilities and proof frame (2026-10-20)

Retain the five upstream owners: configuration registers maps/items/filters;
items owns ProofTreeItem and its inner Stack; methods builds proof/sequent MML;
mappings owns the command/environment tables; util owns rule balancing and
semantic annotation. Replace these pairs in place; do not add helper owners.

The first component is only items. ProofTreeItem derives from BaseItem, owns
its inner Stack directly, stores its optional environment name, and borrows MML
through the existing document-owned node stack. Its checkItem takes the actual
FrameHandle, forwards that same handle to the inner Stack before returning
Drop, and on matching end replays a new MmlItem plus ReuseIncoming. Stop throws
EnvMissingEnd. toMml deliberately ignores its inherited optional flags, as the
Source override does, and combines the inner prefix with the proof content.
The proof marker writes the existing ProofNodeProperties::proof field. No
Context, arbitrary property service or exported inner-stack mutation survives.
Source's unused leftLabel/rigthLabel fields have no native consumer; do not
reproduce them. Add the methods' actual line/label/direction fields when that
owner is converted, rather than speculative state now.

This component is not package acceptance. Source util measures live subtrees
through CommonOutputJax.getBBox between MML edits. The current compile filter
data has no output service and LayoutJob::build is a root typesetting operation,
so neither a constant width nor skipping balanceRules is acceptable. Freeze a
real subtree measurement/compile-output borrow in the original owners before
activating the complete configuration. The inactive methods/configuration/util
pairs and their old consumers remain explicitly pending.

The native measurement operation is `CommonOutputJax::getBBox(document,
subtree, config, measurement, catalog, linebreakFactory) -> BBox`. It creates
one local CommonOutputJax from the caller's actual immutable font/profile
values and class registrations, calls setTeXclass(nullptr) on the supplied
subtree and returns factory.wrap(subtree).getOuterBBox() by value. This API is
an operation with explicit configuration, not a clone of an existing output
instance: it does not silently drop virtual customizations or class changes.
The local factory, visitor and node index die before returning; neither a
wrapper nor an expression pointer escapes. No document-root replacement,
parent edits, LayoutJob::build or inline-break/overflow marking is involved.
This retains Source's fresh nodeMap behavior without accumulating measurement
wrappers in the final layout job. MeasurementTarget remains the explicit host
for unknown-text metrics; mathematical measurement stays in Common.

Source createMo also constructs auxiliary mo/text MML. In the native model
these remain owned by the caller's request MmlDocument, including when a
measurement wrapper has died; they are not inserted into the primary child
tree. Repeated measurements on one document can increase its arena size.
This is an explicit request-lifetime tradeoff, not per-measurement MML release:
the final renderer must own a fresh MmlDocument per request and retain neither
that document nor its auxiliary nodes in session state or completed Scene.
Do not add manual arena rollback/retirement to mask that ownership contract.

An optional MathOutputJaxService pointer is borrowed per compile call along
the existing MathItem -> MathInputJaxService -> TeX -> InputJaxFilterData path.
It defaults to null for inputs that do not require an output service. There
is no stored output/request pointer on TeX, ParseOptions or package state.
Both pre/postfilters see the same current borrow, including error formatting;
exceptions and nested compile calls cannot overwrite a persistent binding.
The Bussproofs preprocessor rejects an absent measurement service using
the Source diagnostic. Full SVG service implementation still must provide
typeset/escaped; a measurement-only test service throws on those unused paths
and cannot stand in for a completed renderer.

Bussproofs keeps semantic-property insertion order in the
existing ProofNodeProperties owner. The ten finite keys are axiom, proof,
sequent, sequentProcessed, inference, inferenceRule, labelledRule, prooflabel,
sequentAdjust_left and sequentAdjust_right. Store only their enum order (at
most ten entries) alongside the existing uniquely held typed optional values.
Named typed setters and erase maintain it: updating a present key keeps its
position; erase followed by set appends. Copy/merge and the existing three-key
move operation must preserve the relevant Source order. Util serializes that
order into semantics, retaining zero/false and leaving existing semantics
untouched when there are no prefixed keys. proof-left/proof-right remain plain
spacing values outside this order. This bounded representation does not
authorize a generic property dictionary or a competing node owner.

For the Bussproofs package port, those ten optional values are private;
their existing snake_case names are read-only optional-valued getters.
setAxiom/setProof/setSequent/setSequentProcessed, setPremiseCount,
setRuleDirection/setLabelledRule/setProofLabel and setSequentAdjustLeft/Right
are the only writes. erase(ProofAttribute), orderedKeys() and mergeFrom()
operate on that bounded schema. left_space/right_space remain unprefixed
optional doubles. The original items owner holds the actual line/currentLine,
rootAtTop and borrowed left/right label state used by Methods. Prooftree is
an EnvironmentBodyHandler; all eight macros return NoParseResult on normal
Source-void paths. The original mappings/configuration register the same
commands, ProofTreeItem and four ordered filters. Util exports those four
TexFilterData callbacks, borrows output only from that argument, and creates
its measurement-only MathItem as a local object inside balanceRules. It stores
no current document, runtime module, object wrapper or callback identity.

### Physics package responsibilities (2026-10-20)

Replace the original physics items/methods/mappings/configuration pairs in
place; no extra implementation family, runtime object API or duplicate helpers.
Items owns AutoOpen's open/close/big/right strings, smash/ignore flags and signed
double openCount. It preserves short-circuit closing/decrement, nested literal
open counting, stop error, inferred=false behavior, smash/right construction
and removal of the three fenced-node TeX properties. Stack owns activations;
all node pointers remain request-document borrows.

The close-over marker belongs on original CloseItem as bool pre_autoclose;
the fence-completion marker belongs on original MmlItem as bool autoclose.
AutoOpen checks these concrete incoming classes in Source order. Do not add a
general BaseItem property map or another Stack transition path.

Methods owns all original command algorithms and their private local helpers.
Its 25 actual methods use MacroHandler signatures, including AutoClose in the
character MacroMap. Macro/NamedFn/Array retain BaseMethods callback identities.
VectorBold scopes the original token creator and environment changes with RAII;
nested vectors and errors restore the prior native collaborators, without a
runtime binding service. Source-defined macro expansion and parser positions
remain the original TexParser's responsibility. Matrix dimensions use parsed
primitive numbers, checked before native allocation; no source-text exceptions.
Normal VectorBold cleanup removes vectorFont/vectorStar rather than restoring
their previous values; preserve Source's resulting nested-vector output. Source
leaves its temporary creator installed after a creator exception, as observed
independently. Native restores creator/font on throws by its RAII contract;
report this strengthened lifetime boundary separately from normal tree parity.

Mappings retains the original eleven maps and complete entries/callbacks/args.
Configuration only registers those maps, AutoOpen and the existing typed
PhysicsOptions defaults italicdiff=false/arrowdel=false. Root owns Methods,
the two shared markers, CMake, all native execution and commits; the existing
6.1 Sol agent owns the other six original files and package Source/test fixture.
Capture registration data before compiling any Source formulas, and copy its
primitive attribute values immediately: Source mathchar0mo modifies shared
Token.attributes with stretchy=false during use. Post-use object state does
not describe the immutable native registration input; rendered nodes still
compare against the actual Source results with their parsed attributes.

### Unicode and Entities responsibilities (2026-10-20)

Unicode remains its original single configuration pair, containing the three
methods and one CommandMap. The private UnicodeCommandMap derives from the
existing CommandMap and owns the finite codepoint-to-font cache. A MapHandler
represents one loaded module environment: its TeX instances share this cache,
including across reset, while destroying the registry releases it. Independent
registries represent independent Source module environments. There is no
process-global cache, per-equation copy or universal package-state dictionary.
Initialization reuses an already installed UnicodeCommandMap in that registry.
The former height/depth cache slots have no consumer anywhere in Source;
preserve their bracket syntax and font argument selection without storing dead
metrics. Validate codepoints before inserting native cache entries.

The unicode MML marker is an optional bool on the original TexNodeProperties,
with the same original copy/merge owner. No new node or property bridge exists.
Use Entities.numeric on the original TeX spelling, including its radix rules;
RawUnicode and Char reuse the already verified string_util.unicodeString and
UTF-16 codepoint helpers. Macro aliases retain the three compiled callbacks.

Entities stays in util/entities.ixx/.cpp with a typed string table, loaded shard
set and bool loadMissingEntities option. Reuse the existing hash-locked 29-shard
generator/data. Install local compiled shards synchronously on first demand;
compare against Source's completed translate result after actual loader retries.
Each first-use installation restarts translation from the original input, since
a shard can restore an overridden base entry already visited in the prefix.
Generated shards preserve the actual add() entries, including base overlaps.
No JS Promise/retry scheduler or external resource loader belongs in the native
math pipeline. Disabled loading, explicit installation, typed add/remove,
case-sensitive table lookup and numeric range errors remain explicit behavior.
Use existing string_util.unicodeString for numeric conversion, including NUL,
supplementary characters and isolated UTF-16 surrogates; never duplicate it.

### Mhchem responsibilities (accepted package, 2026-10-21)

Keep the original `input/tex/mhchem/configuration` pair as the package owner:
MhchemMethods.Machine, compiled BaseMethods.Macro/AmsMethods.xArrow aliases,
MhchemUtils.relmo, the six ordered replacements, both maps and configuration.
Expose typed callbacks and `initializeModule(ConfigurationHandler&, MapHandler&)`;
remove MhchemModule's Context/Property/callable fabrication rather than retaining
parallel implementations. Source defines nine command entries (ce, pu and seven
x-arrows) and fourteen character entries. relmo preserves the captured Token's
attribute overrides after stretchy/REL/-mhchem defaults; the last replacement
queries the original lexical command/character maps rather than a reduced arrow
allowlist or the registry's later replacements. Native replacements take those
two TokenMap references explicitly. Machine uses the retained handler maps;
ordinary map mutation remains visible while registry replacement does not
redirect an existing package instance. Do not recreate generic module objects
or callable identities to preserve this typed ownership relationship.

The existing `dependencies/mhchem_parser/parser.ixx`, parser.cpp, patterns.cpp,
actions.cpp, texify.cpp and immutable pattern/transition data remain the owners
of mhchemparser 4.2.1. Reuse these native algorithms and correct proven Source
differences in place. No second parser, arbitrary object record, JS evaluator or
example-output lookup belongs here. Audit the native closed Parsed/Buffer/Match
records against the pinned declaration and every action consumer before changing
their representation. Prove regex-cache keys are bounded by source patterns;
request text must not accumulate in a process cache. Root owns CMake and the one
native build/test stream. Accept independent parser-to-TeX evidence, complete
package MML/errors/recovery, maps and replacements before advancing to texhtml.

### Texhtml and its Lite dependency (2026-10-21)

The old Lite interfaces still carry Property/Context projection and are absent
from the active native target. They cannot be reused as a native dependency.
Retain their original algorithms in the original owners while replacing that
representation. Lite is used only to parse foreign HTML content; it never sits
between Common layout, Scene and SVG. Do not introduce another HTML DOM,
generic JS value type, reflective callbacks or a second HTML snapshot library.

| Owner | Responsibility |
|---|---|
| core/dom_node | Native DOM node/document/window base references; no store, identity registry or property binding |
| core/dom_adaptor | Typed string/node/ordered-string-attribute interface and original default algorithms |
| adaptors/lite/element | Element tag, ordered string attributes, child vector, lazy Styles cache and original construction |
| adaptors/lite/text | Text and comment values and their different kind/value behavior |
| adaptors/lite/document | html/head/body aliases, doctype and weak defaultView |
| adaptors/lite/window, list | Window/document ownership and ordinary shallow node-reference list; no JS constructor slots/iterator objects |
| adaptors/lite/parser | Existing regex/token/Entities/open/close/repair/serialize/escape algorithms and immutable Source tables |
| adaptors/lite_adaptor | Existing tree/query/attribute/class/style algorithms and finite NodeMixin measurement options |
| core/mml_tree/xml_dom_adaptor | Freeze/clone foreign Lite subtrees into the existing opaque XmlRef domain and const HtmlAdaptor operations |
| input/tex/texhtml/tex_html_configuration | All of TexHtmlConfiguration.ts: macro, map, option, config hook and pre/postfilters |
| handlers/html/html_dom_strings | Original DOM-string traversal and includeHtmlTags hook; no package-local duplicate |
| output/scene/html_snapshot | Existing conversion of accepted foreign content to HtmlNodePrimitive, when actual Scene emission is connected |

DOMNodeRef is shared_ptr<DOMNode>, DOMDocumentRef is shared_ptr<DOMDocument>.
DOMNode has a virtual kind() and a weak parent, without generic fields.
LiteElement owns its children strongly; attributes are an ordered vector of
name/value UTF-16 strings, and Styles is the existing typed util owner.
LiteText owns its UTF-16 value; LiteComment retains raw comment spelling.
Factories in those original classes initialize parent links after shared
construction. LiteDocument owns root/head/body aliases into one tree.
LiteWindow owns its document, whose defaultView is weak. A separately retained
document does not keep an otherwise unused window alive; JS constructor-field
reflection and tracing-GC reachability are explicitly withdrawn APIs.
List and childNodes results are ordinary shallow vectors, with no array holes,
property descriptors or JS array identity. No process/global node arena exists.

AbstractDOMAdaptor returns typed document/node refs, optional string attributes,
ordered attributes and node vectors. Mutations consume node refs and strings;
numeric attribute input may use a separate double overload with existing
numeric formatting. Missing nodes are null refs. Preserve original traversal,
reparent, clone, UTF-16 split, selector subset, parser and serialization behavior;
record Source runtime errors separately from native invalid-domain exceptions.
Reuse current pure algorithm bodies rather than replacing Lite with a browser
or another HTML parser. NodeMixin options are named bool/double/string fields;
worker construction and arbitrary getProperty/setProperty are outside this API.

XmlDomAdaptor::fromDOM freezes a clone, using the same Lite node classes, inside
private const XmlObject payloads. It exposes no mutable DOM handle, identity
cache or nodeProperty. HtmlNode retains its existing shared const adaptor.
Real foreign-content queries remain on HtmlAdaptor; Scene copies values once
through html_snapshot and retains neither Lite nodes nor the adaptor. External
mutable aliases cannot change the accepted XmlRef payload after freezing.

Root freezes declarations before parallel implementation. Node/core headers,
Lite parser, Lite adaptor, XML bridge, package/DOM-string algorithms and independent
Source witnesses have disjoint owners. Root alone changes CMake and shared TeX
compile/filter plumbing, builds, verifies and commits. Accept Source-generated
full-tree/serialization/parser/operations fixtures, lifecycle release and error
recovery; then one full native math run. Package success still does not establish
Common emission, final SVG equality or Folia integration.

The existing MathInputJaxService/MathItem/TeX compile chain receives one native
CompileServices value containing borrowed output-jax and DOM-adaptor pointers.
This replaces the output-only argument, without a parallel compile path.
InputJaxFilterData carries both borrows; the texhtml prefilter copies the adaptor
borrow to ParseOptions and its postfilter clears it. An exception scope clears
that request borrow as well. The caller owns the adaptor for this invocation;
neither TexSessionState nor a generic package-data map retains it.

The real texhtml render tests also require original string delimiter discovery;
the remaining old FindTeX/FindMath object interfaces are not native services.
Port those original pairs, retaining the delimiter/brace/environment/escape/ref
algorithms. core/MathItem owns ProtoItem and protoItem(), with top-level string
index n, start.n/end.n UTF-16 offsets, open/close strings and MathDisplay's
inline/display/escaped value. FindMath accepts span<const u16string> and returns
vector<ProtoItem>; AbstractFindMath<Options> owns the derived typed option value.
FindTeXOptions has inline/display delimiter-pair vectors and three named bools.
FindTeX owns its compiled regexes and finite delimiter table, never input text
or a process cache. No Context, DOM-query overload, arbitrary class override or
alternate delimiter finder belongs in these native owners.

## Ownership

### Common-to-Scene emission contract (2026-10-21 Root lock)

Emission fills the existing Common owners. Old `scene/wrapper`, `scene/wrappers/*`,
`glyph_paint` and SVG object-host files are not candidates for registration.
Retain their Source provenance only when moving a verified numerical algorithm
to its original Common family. Do not add another wrapper hierarchy or an
emitter service/directory beside Common.

`CommonWrapper` exports `SceneNode = optional<scene::NodeId>`,
`SceneNodes = vector<SceneNode>` and the virtual
`SceneNodes emitScene(scene::Builder&, span<const scene::NodeId> parents)`.
Each call returns only the output segments made for that call; wrappers store
no Builder pointer, output-node registry or output lifetime owner. Multiple
parents/segments preserve the original line-break protocol. The original base
owner provides `createSceneNodes`, `standardSceneNodes`, `addSceneChildren`,
`toEmbellishedScene`, `placeScene`, `sceneDx`, and the style/scale/border/color/
attribute/id helpers. `placeScene(builder, nodes, x, y, optional<NodeId> target)`
preserves Source's default-first-segment versus explicit-element distinction;
only the default applies dx and id compensation. `emitChar` and
`emitUnknownText` remain base-owner helpers shared by TextNode and Mo, with an
explicit local UTF-16 buffer, not a persistent generic object or font cache.
`SceneContainers {SceneNodes nodes; SceneParents content;}` is the call-local return value
of virtual `createSceneNodes` and `standardSceneNodes`: nodes are the wrapper's
own output segments, content are valid parent ids for its children. Usually corresponding,
they differ for a broken Mrow (one outer stack, several inner lineboxes).
Style/paint/id apply to nodes; child emission and line placement use content;
emitScene returns nodes to its caller. This preserves Source's distinction
between `this.dom` and the nodes returned by `createSvgNodes`, without storing
a DOM field or a second output registry on wrappers.

Existing family pairs override this entry where Source overrides `toSVG`.
TextNode owns remapping, composition/unknown-run emission collaboration; Mo
owns fixed/assembled delimiter glyph origins, scales and clipping. Mrow owns
line segments/positions; Mtext owns its word-break slices; Math owns display
alignment values. Mfrac/Msqrt/Mroot consume their existing `layoutGeometry()`
without repeating equations. Other specialized families remain explicitly
unimplemented until ported, rather than silently taking generic row output.
The base's standard horizontal algorithm is valid only for Source families
that actually inherit it. No integration or full SVG acceptance precedes the
remaining specialized-family work.

Builder retains an ordinary, request-local value tree. Its finite operations
are `transform/set_transform`, `metadata/set_metadata`, `paint/set_paint`,
`entry_count`, `insert_primitive`, and `wrap_contents` in addition to existing
add/freeze calls. Getters return values; insert checks its index; wrap moves
the group's existing entries under one new group, preserving order and ids.
There is no arbitrary DOM property API, object identity registry, reflective
call dispatch or output pointer retained by Scene. `freeze() &&` closes the
mutable phase; a completed Scene owns all content independently of the job.

Scene adds group Paint, an owned CSS-color token alternative in PaintColor,
and optional TextFont {family, size in ems, italic, bold, variant} on TextRun.
CSS-color tokens are drawing paint values, not SVG nodes; a future D2D
consumer resolves that same color syntax without reading SVG or computing
mathematical layout. Primitive metadata/roles preserve Source element intent
and non-geometric attributes; PathElement distinguishes path/polygon/line.
Typed DisplayLayout {display, alignment, shift, minimum_width, full_width}
belongs to Scene/Builder, not a borrowed output jax. Raw style/attribute
spelling is serialization metadata and cannot replace these shared semantics.
All geometry has one em, y-up convention, including Rule rectangles. The old
writer's `-(y+height)` rectangle conversion contradicts its global y reflection
and Source fraction `rect.y = axis - thickness/2`; correct that conversion,
with independent Source evidence, rather than adding family-specific flips.

`LayoutJob::buildScene()` builds the actual Common root, emits through it,
sets root/font metrics and freezes the Builder. `CommonWrapperFactory::standardNodes()`
supplies the single concrete native profile catalog; its existing abstract
`defaultNodes()` contract remains empty. Renderer and tests use standardNodes,
not independently copied registration tables. Source-only probes do not count
as native callers. SVG writer consumes only completed Scene plus immutable
font-path resources, performs spelling/escaping/cache/number formatting, and
does not select glyphs, measure unknown text or place/stretch anything.

Acceptance: independent pinned Source witnesses first; native real TeX input
and standard catalog through LayoutJob::buildScene, inspect segment/geometry/
paint/font ownership and compare SVG. Cover ordinary text/mathbb, compositions,
unknown runs, fixed and assembled delimiters, fractions/roots, styles/id/link,
and multiline rows/text. Existing 75 Common/150 byte-exact SVG/full-profile
gates remain required. During implementation report unsupported families and
writer gaps explicitly. After focused tests, one full math run; no D2D pixel
gate and no Folia switch before the complete SVG gate passes.

The existing `CommonWrapper::sceneDx` is a virtual native layout query.
`CommonMrow` returns zero when an inferred row emits directly into its parents
(stack or no line break), because Source skips that row's styled SVG shell.
An explicit or broken inferred row retains the ordinary padding/border offset.
This is computed from the existing row state; no output-state flag is retained.
The pinned development oracle `common-mrow-style-source.cjs` observes own CSS
attributes on actual compiled MML: zero for the unbroken inferred path and
1.125em for its broken control. That programmatic MML state is not claimed as
a naturally authored TeX case.

### Script-family Scene emission (2026-10-21 Root lock)

The existing `common/wrappers/scriptbase` pair owns the shared one-script
emitter; `msubsup` owns the two-script override, `munderover` owns limits and
accent emission, and `mmultiscripts` owns pre/post pair alignment. Implement
these original pairs only. Reuse getBaseWidth/getOffset/getUVQ/getCombinedUV,
getDeltaW/getOverKU/getUnderKV and the existing stretch/BBox algorithms.
Numerical placement still present in Source SVG moves into these Common
owners; the SVG writer receives completed values and computes no script math.
Do not create script dispatch services, a parallel wrapper family or a second
layout result. All emission uses the existing SceneContainers contract and
call-local NodeId vectors; no retained Builder/Scene/output ownership.

New Source evidence from SVG's actual mixin bases also fixes the native limits
hierarchy: Munder derives from Msub, Mover from Msup, Munderover from Msubsup.
The latter maps sub/sup to under/over and inherits the two-script algorithm
for movable limits. Constructors pass the explicit script index because the
native MML under/over node is a distinct C++ type. Do not copy the sub/sup
equations into under/over or retain the prior Scriptbase-only inheritance.

Preserve Source child emission order, first/last segment selection, movable
limits, embellished handling, empty multiscript slots and scriptalign. Source
`child.place(x,y)` calls that child's default placeScene, including dx/id;
only calls with an explicit element pass an explicit target. In multiscripts,
the two anonymous script rows are explicit targets, their children are not.
Acceptance uses independent Source witnesses and the real full38 pipeline,
covering sub/sup/subsup, movable/display limits, accents, pre/post pairs,
alignment, nested scripts and line segments. Existing unsupported families
must remain visible; no source-string special cases or weakened byte gate.

### Inline fragments and transform spelling (2026-10-21 Root lock)

Source SVG's handleInlineBreaks consumes already completed Common line boxes.
Its native coordinator belongs in CommonOutputJax::finishInlineScene, called
once by LayoutJob::buildScene after ordinary root emission. Mrow keeps its
existing line-layout/emission responsibility. No second wrapper hierarchy,
SVG DOM, second layout calculation or retained output nodes are introduced.

Scene types add InlineBreakKind {opportunity, forced}, InlineBreak {kind,
spacing in em}, InlineFragment {root NodeId, completed Metrics, optional
before}, and FrameLayout {minimum_extent, inline_fragments}. Scene owns that
finite value and one tree. Fragment roots are its direct root groups in order;
there is no retained stacked geometry beside them. Builder::child_groups
returns immediate child ids by value. Its finite split_group_contents(shell,
content_sources) replaces a direct root shell at the same position with shell
copies containing the moved entries from the given descendant lineboxes.
Only shell values are copied; geometry/ids are moved, not cloned. Validate
all ids/ancestry/distinctness and prepare allocation before mutation. Getters
and setters for FrameLayout close this operation without arbitrary properties.

Common computes each fragment's h/d/w from lineBBox times the scale of mml
returned by getBreakNode. Its break spacing uses originalL, mo/mml force_break,
and the next sibling for linebreakstyle=after, exactly as Source. It sets the
minimum frame extent to currentMetrics.em/1000. SVG only projects those values
to frames, preserving one outer container, Source break-node spelling and
ordered metadata. Forced-break SPACE lookup and blank-letter spacing are
serialization; they cannot move or remeasure math. The future D2D consumer
uses the same fragment metrics, groups and physical spacing values.

A transform can contain both Source place's comma translation and an operator
adjustment's space translation. One group role cannot encode that difference.
Translation therefore gains a finite TranslationSeparator {role_default,
comma, space, comma_space} spelling field; its affine matrix ignores spelling.
CommonWrapper::placeScene records comma; Mo records Source's individual
adjustment spelling. Preserve real ordered operations, omit accidental identity
matrices, and never guess separators from operation position or node strings.

Acceptance: Builder extraction preserves the number/order/ids of all drawing
entries, rejects invalid inputs before changing the tree, and freeze owns all
fragment values. Mixed translations retain the same composed matrix. Compare
real plain/multiple-operator inline, narrow inline, forced before/after breaks,
scripts and multiline text to independent Source; retained Scene serializes
after its job/document die. No display/inline-policy changes to satisfy tests.

### Optional emitted segments (2026-10-21 Root correction)

Actual inline SVG comparison identifies a missing part of the emission
contract: Source Mo removes the suppressed segment and leaves a null slot in
its returned dom array. Skipping glyph creation but retaining an empty group
is observably different. SceneNode is therefore optional<NodeId>, SceneNodes
is vector<SceneNode>, and SceneContainers.content is SceneParents, the vector
of valid parent NodeIds. Input parents remain span<const NodeId>.

Only output/placement/style helpers consume optional segments. No numeric
sentinel, output side table, stale removed id, reflective object state or
second presence registry is introduced. Keep Source segment indices even
when absent. Mrow skips absent output for box/placement work but preserves
the unconditional line-reset/index rules. Default place returns for an absent
first segment; explicit target remains a valid NodeId. Other families check
and unwrap only where Source requires a present own container.

Builder::discard_group removes a non-root group's parent entry and its entire
subtree by ordinary RAII, after validating the request. Mo calls it for the
Source-suppressed segment and resets the corresponding optional output slot;
subsequent emission and Mrow placement skip that slot. A frozen Scene never
contains the removed empty group or a dangling reference. Acceptance covers
before/after/duplicate operators, embellished operators, narrow inline rows,
unchanged completed geometry/order and job/document retirement.

### Completed viewports and glyph transforms (2026-10-21 Root lock)

The remaining overline comparison shows that a nested viewport differs from
a clip-path group. Scene Group gains optional Viewport {bounds, view_box,
optional aspect_ratio}; both rectangles use em/y-up coordinates. It maps the
view-box onto bounds and clips content there. ViewportAlignment/ViewportFit
are finite min/mid/max and meet/slice/none values; absence preserves SVG's
default xMidYMid/meet. This is completed geometry shared with future D2D,
not an SVG DOM or a layout callback.

Mo reuses its Source addExtV/addExtH calculations, creates one viewport group
and emits the glyph directly inside. ElementRole vertical_extender_viewport
and horizontal_extender_viewport retain width,height,y,x versus width,height,x,y
spelling. Existing table_content_viewport/table_label_frame roles can use the
same geometry and explicit aspect alignment; SVG omits their bounds supplied
by Source table CSS, while Scene records resolved bounds. Their actual table
placement is still unimplemented and cannot be accepted by this declaration.

Glyph gains an owned Transform for glyph-local operations; do not invent a
group around a path to scale it. CommonWrapper::emitChar accepts a trailing
default Transform and applies it to the glyph or composition group. Unknown
transformed glyphs report unsupported output instead of dropping a transform.
Scaling gains explicit_axes spelling metadata for Source scale(1,s), even when
the axes happen to be equal; matrix composition ignores spelling metadata.

Builder add_group accepts trailing optional Viewport. Snapshot/extraction copy
only its finite values; freeze validates rectangles, nonnegative bounds,
positive view-box extents, aspect enums and glyph transforms. SVG projects
completed values with Source formatting, without delimiter/table/font layout.
Acceptance covers ownership, invalid geometry, same-matrix transforms and
real horizontal/vertical assembly; the independent overline case must become
byte-equal. Synthetic primitive checks do not accept the full Common profile.

### Padded and phantom emission (2026-10-21 Root lock)

`common/wrappers/mpadded.ixx/.cpp` remain the sole padded owner. Its emitScene
first uses the existing embellished protocol, then standardSceneNodes and the
already verified getDimens(); it does not copy dimension/line-breaking formulas.
Apply Source SVG's dW correction for negative dw and data-align, then X=x+dx-dW.
Only a nonzero X/y creates one anonymous child group under the first emitted
node, placed through this wrapper's explicit-target placeScene; children go to
that group. Otherwise children use the ordinary content parents. Return the
wrapper's own optional output segments, not the shifted content group.
The guard for unimplemented generic emission belongs to CommonWrapper's
emitScene entry; addSceneChildren is the one shared Source horizontal helper
used explicitly by specialized families. Do not duplicate its placement loop.

Source mphantom has no Common-specific geometry owner: its specialized SVG
wrapper only emits standard nodes, with no children. Move that responsibility
to exactly one `common/wrappers/mphantom.ixx/.cpp` pair with CommonMphantom;
inherit existing Common measurement unchanged, override emitScene only, and
register it in the same standard catalog. This is the mathematical node's
owner, not a helper collection or parallel wrapper hierarchy. Keep hidden
descendants available to measurement, but do not emit hidden drawing or retain
a second output tree. Root alone edits CMake, common exports and the catalog.

Acceptance uses independently captured full38/Newcm Source SVG for smash/raise,
positive padding/border, phantom and horizontal/vertical phantom combinations;
verify nonzero completed dimensions while hidden descendants have no Scene
glyphs, exact SVG bytes and existing lifetime checks. No expected SVG is derived
from native output. Semantics/fenced/table/actions remain separately scoped.

### Table emission (2026-10-21 Root lock)

Keep the existing mtable/mtr/mtd pairs. Mtable owns row origins, rules/frame,
percentage-width adjustment, labels and their completed viewports. Mtr owns
cell traversal, row/cell spacing and background expansion. Mtd owns cell
alignment/default placement and its background bounds. Reuse Common TableData,
computed widths/half-spacing, BBox and alignment methods; move only missing
Source SVG numerical work into these owners. No new table directory or wrapper
hierarchy. Existing 14-case table Source witnesses remain independent evidence.

In mtr.ixx, TableRowGeometry holds h,d,t_space,b_space,t_line,b_line;
TableCellPlacement holds x,y,l_space,width,r_space,l_line,r_line. They are
call-local completed numbers. TableRowScene holds own SceneNodes and label_nodes.
Mtable calls virtual Mtr::emitTableScene(builder,parents,geometry,label_parent),
retaining each row's result/geometry only in its emission stack. Mlabeledtr emits
its label separately into the supplied label group. Mtr::placeSceneCell takes
the cell, its emitted optional segments, row geometry and placement by reference.
Mtd::placeSceneCell returns alignX/alignY and places the existing emitted cells;
placeSceneColor adjusts completed rectangles. No output ids/Builder/label DOM
are stored on wrappers. Standalone Mtd overrides emitScene using Source's base
embellished/standard/addChildren protocol; table rows require table context.

Builder gains finite background_bounds/set_background_bounds (first entry only,
Rule with background_box role and data-bgcolor, otherwise absent/false),
reparent_group(child,parent) (root/cycle/multiple-owner validation before move),
and set_viewport_bounds(role,bounds), which only copies a completed Rect to
matching existing viewports. These are typed drawing-value edits, no property
lookup or identity/runtime service. Table labels may temporarily be root-owned
while assembling, then are moved once into their completed parent before freeze.

Scene roles table_frame/table_vertical_line/table_horizontal_line preserve
Source data-frame/data-line,class and attribute order. Common records actual
Paint: fill none, inherited stroke, width .07; dashed {.14}, dotted {0,.14}
and round cap. Writer does not repeat these CSS-supplied paint attributes;
Source class remains spelling only. Nondefault stroke-thickness/dasharray are
serialized after geometry from completed Paint values, in Source units; do not
duplicate numerical thickness strings in Common metadata. Mtable's emission is
thin orchestration over its original Source-shaped private methods for rows,
lines, frame, percentage width and labels, not one accumulated function.
Viewport adds clip_content=true; table viewports use false, reflecting Source's
overflow-visible CSS. Existing extenders retain true. Scaling gains trailing
precision=3 metadata (0..12); table alignment uses 2, as Source topTable explicitly
calls jax.fixed(scale,2). This new table evidence does not alter the settled Mo
default precision. Transform matrices ignore spelling fields.

FrameLayout gains optional PercentageFrame {percent,width_text,bounds,scale}.
Percent is a resolved percentage, width_text only Source spelling, bounds are
the completed absolute root viewport expressed as CSS pixels/1000 (the shared
Scene unit at this no-viewBox frame), and scale is ex/(x_height*1000).
Common OutputJax completes it after emission from root.pwidth, input metrics and
DisplayLayout minimum_width; it updates the two table viewport roles to those
bounds before freeze. Bounds.width=max(containerWidth*percent/100,
minimum_width*ex/x_height)/1000; bounds.height=max(h+d,em/1000)*scale.
FontMetrics.em/ex retain actual input CSS metrics (x_height/cap_height remain
em ratios): Source createRoot needs actual ex, not the previously normalized
ratio. Root-level percentage projection is completed once, never guessed from
MML names in the writer. SVG uses width_text, data-mjx-viewBox, scale(s,-s)
translate(0,-h*1000), with s fixed6. Ordinary and inline frames remain unchanged.

Acceptance: existing real-TeX matrix/arrays/aligned/colortbl cases plus independent
Source inputs covering labels on both sides, nested labels, percentage widths,
rules/frames, equal rows/columns, row/cell backgrounds, and nondefault em/ex
that distinguish Source scale precision. Inspect finite Paint/viewport/ownership,
compare complete SVG bytes, repeat after request retirement. Full75/150 and Folia
integration remain required after the remaining families, not accepted by this batch.

### CSS stylesheet dependency for Common (2026-10-20)

Keep `StyleJsonSheet` in existing `util/style_json.ixx/.cpp`; no Common-local
stylesheet implementation. Its native CSS tree is `StyleEntry {u16string name;
variant<u16string,double,vector<StyleEntry>> value;}` and `StyleJson` is the
ordered vector of entries. Leaves are CSS strings/numbers; groups are selector
definitions including nested rules. This is not a general runtime object value.
Copying owns a value snapshot. Merging updates named declarations in place,
appends new names, and replaces nested definitions shallowly as Source does.
Preserve constructor/addStyles/removeStyles/clear/cssText/getStyleString/
getStyleRules/getStyleDefString and exact Source whitespace/numeric spelling.
Use overloads for omitted getStyleRules input, not a Missing/null sentinel.
Acceptance compares actual pinned StyleJson.ts execution for replacement order,
removal/reinsertion, nested @media/@font-face, numbers, empty groups and UTF-16.
Arbitrary JS alias/prototype/coercion tests remain withdrawn; CSS output and
merging behavior remain required. Common shared declarations are governed by
`common-native-implementation.md`; drafts do not establish compiled acceptance.

### Typed factory and actual MML family decision (2026-10-20)

The generic `Tree/Factory` and `Tree/Wrapper` class roles remain. Replace their
reflective records in the existing files; do not bypass these bases in Common.
`FactoryServices<Product, Kind, Arguments>` and
`AbstractFactory<Product, Kind, Arguments>` use one ordered registry of
`FactoryClass<Product, Arguments>` values. Each record contains only a typed
`unique_ptr<Product>(Arguments const&)` creator and a typed instance predicate.
There is no second creator object/cache, universal argument vector, class-object
identity, source-call bridge or lifecycle lease. Replacing an entry retains its
position; deleting and re-registering appends it. `getNodeClass` returns an
optional record value, avoiding a registry reference that could be invalidated.
An explicit optional default kind preserves requested-kind/default-kind
selection; absence of both is an error. `nodeIsKind` uses the registered C++
instance predicate, preserving subclass checks rather than comparing kind names.

The generic factory constructs and passes the unique owner to a concrete
`adopt(unique_ptr<Product>)` hook. Common's concrete factory owns the wrapper
arena. Node factories transfer ownership to their document. A null creator or
product is an error, never a placeholder node. Product initialization and MML
indexing remain the concrete factory's responsibility after ownership transfer.

`WrapperServices<Product, Source>::wrap(Source&, Product* parent)` and
`AbstractWrapperFactory<Product, Source, Arguments>` retain the source-kind
dispatch and delegate to this same registry. The wrapper-factory base calls a
typed `creationArguments(Source&, Product*)` override; concrete Common arguments
contain its actual factory reference, MML node reference and borrowed parent.
`Wrapper`/`AbstractWrapper` retain `kind`, `wrap` and preorder `walkTree` in their
existing files. The base stores the one borrowed source pointer and one vector
of borrowed wrapper children. A native visitor captures any typed caller state;
it does not take `std::any` or a simulated JS argument/array. Common must not add
a parallel child collection or creator registry.

For MML, retain the current nonmovable document arena, typed attributes,
properties, intrinsic state and tree invariants. Restore the original C++
`MmlNode`/`AbstractMmlNode`/token/layout/base/empty classes and concrete family
inheritance as actual polymorphic arena objects. `MmlDocument` keeps one
`vector<unique_ptr<MmlNode>>`; its destructor uses the virtual node destructor.
The former final structural `MmlNode` has been replaced by actual polymorphic
family objects in the document arena. Do not
add a separate semantic façade, detached family object, NodeKind switch in each
consumer, or a fallback implementation of missing semantic methods.

Reuse the original C++ methods and replace their typed inputs/ownership in
place. Node-kind identity becomes `NodeKind`; family/class tests retain the
Source subclass relation. Parent and child storage have one authority, and
structural mutations use the document's existing ownership/cycle checks. The
MML family owns core/embellished/spacing, TeX classification and inherited
attribute algorithms. Common calls these real methods. The document/node named
module may keep the shared base declarations in its existing `:node` partition;
concrete family modules import that primary, and document implementation units
may import the families for construction without creating interface cycles.

Acceptance of this boundary requires typed factory registration/replacement/
fallback/subclass/ownership tests paired with pinned Source, and real factory
construction of family objects with Source semantic cases. Passing generic
factory tests alone is not Common/MML pipeline acceptance. Native MML family
signatures and the tree-node ownership interface must be reviewed before family
implementation assignments; do not independently invent these in each packet.

The existing `tree/node_types.ixx` and `tree/node.ixx` retain `Node`,
`AbstractNode` and `AbstractEmptyNode`. Their native template shape is
`Node<Product, Kind>` and `AbstractNode<Product, Kind, Base=Node<Product,Kind>>`
(likewise Empty). The one protected `Product* parent_` and
`vector<Product*> children_` live in `Node`, not again in MML. A borrowed
`NodeOwner<Product,Kind>` interface supplies document operations: `setChildren`,
`append`, `replace(parent, oldChild, replacement)`, `remove`, `clone(source,
keepIds)`, `nodeIsKind` and `kindName`. The base node's protected `treeOwner()`
returns that owner; `MmlDocument` implements the interface using its existing
checked tree algorithms. This is a typed ownership boundary, not a second tree
implementation or a lifetime registry. Source per-node methods delegate to it.

`AbstractNode` retains typed `isKind`, `setChildren`, `appendChild`,
`replaceChild(newChild, oldChild)`, `removeChild`, `childIndex`, `copy`,
`findNodes`, `walkTree` and `toString`. Subclass checks delegate to the same
registered native class predicates. Missing-child replace/remove remain Source
no-ops; document ownership/cycle checks still apply to actual edits.
`AbstractEmptyNode` retains its Source no-child overrides. Generic JS property
methods are retired; mathematical property values remain in the existing typed
MML `NodeProperties`, and cloning them belongs to the document. No `std::any`,
property bags, source readers/writers or callable receiver abstraction remain.

`MmlDocument` owns its one `MmlFactory` and exposes it by borrowed reference;
TeX `NodeFactory` and callers borrow that factory. Raw document construction
delegates to `MmlFactory::allocateRaw`, the single concrete-class registry, and
document adoption. `MmlFactory::make(kind, attributes, children)` keeps current
validation/default/arity responsibilities and calls `allocateRaw` directly.
It must not call back into `document.make`. Raw construction continues to omit
defaults, preserving the existing low-level API; ordinary parser construction
uses `MmlFactory::make`. Exact `cloneSubtree` and Source `node.copy(keepIds)`
retain their different, already identified contracts.

Inherited attribute inputs are explicit records `{name, origin: NodeKind,
value: AttributeValue}`. Origin is required for the Source `noInherit` rules.
Plain attribute additions have no origin until `addInheritedAttributes` stamps
the current node kind, excluding displaystyle/scriptlevel/style. Inheritance
display/level/prime inputs are native bool/double/bool with false/0/false
defaults. Verification omission means no verification, so the typed API keeps
an explicit absent-options call form distinct from `VerifyOptions{}`.

Foreign XML/HTML keeps its existing `XmlObject`, `XmlAdaptor`, `XMLNode`,
`HtmlAdaptor` and `HtmlNode` responsibilities; do not invent another DOM or an
unrelated snapshot API. `XmlRef` owns an immutable, XML-domain payload;
`XMLNode` retains a shared const typed adaptor owner. This protocol has only
clone/serialization and the actual HTML string-attribute/content operations,
with native strings, optional strings and vectors of XML references replacing
Property/PropertyList. Adaptors retain no MML/parser/wrapper state or references.
This is a foreign-content value boundary; Common layout does not construct DOM
nodes. Token text reads real `TextNode`/`HtmlNode` virtual methods rather than
reflective `textContent` fields. SVG consumes serialized content and measured
runs through its existing foreign-content contract.

| Lifetime | Owner | Owned contents | Permitted references |
|---|---|---|---|
| Process/profile | immutable `FontRepository`, `PackageRegistry` | font tables/outlines, native handler registration | shared const data; finite declared cache bounds |
| Renderer | `Renderer::State` / `TexSessionState` | macros, labels, tag counters, explicit package session state | value data or independently owned persistent mathematical data |
| One render | `RenderRequest` | `MmlDocument`, parser stack, package scratch, wrapper arena, layout scratch | references into this same request; no persistence |
| Returned result | `RenderResult` | SVG bytes and immutable Scene | owned values and shared const font resources only |

`MmlDocument` is a non-copyable, non-movable typed node arena, holding
`vector<unique_ptr<MmlNode>>`. Its root, parent and child links borrow stable node
addresses within that document. It owns no generic object records. Nodes cannot
be copied/moved independently, cross-document links are rejected, and tree
operations preserve the declared parent/child invariants. Detached construction
nodes still die with the one document. This avoids per-node retirement lists.

The parser's detached nodes and stack items belong to this request. Wrappers
borrow MML nodes and are destroyed before the MML document. Scene freezing
copies the required geometry/metadata and retains no MML, parser, wrapper,
request, mutable package-state or DOM pointer. Exceptions, cancellation and
early returns unwind the same RAII owners; cleanup has no retry protocol.

Persistent macros/labels are explicit semantic state. They cannot capture a
request through `std::function`, borrowed pointers, an untyped payload or an
identity registry. If a package genuinely retains mathematical content, its
typed session state owns a deep value copy or an independent MML document.
Source effects before a TeX error remain governed by MathJax's behavior;
RAII cleanup must not invent a rollback of persistent semantic changes.

## Native interfaces

Public `folia.math::Renderer`, `RenderOptions`, `RenderResult` and the plain C
UTF-16 bridge keep their external roles. `math.cpp` only assembles services and
orchestrates the pipeline. It implements no TeX commands or layout rules.

The foundation contracts are:

- `mml::NodeKind`: explicit mathematical node kinds, including token, inferred
  row, table, scripts, annotations and HTML content required by the profile.
- `mml::AttributeValue`: `variant<AttributeNull, bool, double, u16string>`.
  Attributes are ordered mathematical name/value data with explicit own/default/
  inherited lookup. There are no executable getters, prototypes or callables.
- `mml::MmlNode`: typed node kind, token text, attributes, node properties and
  document-local tree links. Node-family algorithms remain family modules.
- `mml::MmlDocument::make(NodeKind) -> MmlNode&`; `setRoot`, `append`, `insert`,
  `replace`, `remove`, `cloneSubtree` act on nodes owned by that document.
- `tex::TexSessionState`: owns macros, labels and tag counters. Its phase-one
  module is `folia.math.input.tex.session`, independent of the parser facade.
- `tex::ParseOptions`: references typed request/session services; owns no global
  object registry. `NodeFactory` borrows its document's `MmlFactory` and typed arguments.
- Package handlers have named typed signatures such as
  `void(TexParser&, ControlSequence const&)`; configuration is typed native
  registration, including explicitly typed lifecycle callbacks where required.
- `scene::Scene`: immutable, owned grouping/transform/glyph/rule/path/text/clip/
  interaction/foreign-content values with layout metrics and semantic metadata.
- `svg::write(Scene const&, SvgWriteOptions const&) -> string`: serialization
  only. It cannot import the parser, MmlFactory or CommonWrapper.

Source options are represented by explicit option structs. Package-specific
extensible data may use a closed data-only variant with owned strings, arrays
and maps; it cannot contain functions, prototypes, object handles, arbitrary
`std::any`, property traps, or a universal invocation API. Mathematical attribute
maps and macro dictionaries are legitimate domain data, not JS objects.

`parse_options.ixx` contains the finite `TexOptionValues` schema and the original
ParseOptions service declaration. It covers the 42 option fields observed in
the pinned profile through explicit scalar, callback and package-specific
records. Each optional distinguishes absence from false, zero or an empty
string. Source callback options are fixed C++ function pointers; macro and
environment options reuse the existing owned definition values. The schema is
not a parser, a package implementation or proof that these callbacks are bound.

`mergeDefaults` follows Options.defaultOptions: later present fields replace
earlier ones, package records and named dictionaries merge recursively, and
arrays are copied/replaced. It does not mean "fill missing fields", and it does
not implement userOptions warnings or the string-facing setoptions dispatcher.
Permission records retain the distinction between a denied package and a
per-option permission map. Real RegExp state remains shared where Source keeps
the same regular expression; no general runtime object model is involved.

The input jax must own one effective option record. Requests borrow it and the
math-style function captured at input-jax construction. Later mathStyle option
updates do not select a new resolver. `ParseOptions::getVariant` implements the
four actual Source functions, including Unicode 16 NFD for French style. Its
constructor and configured factory/Tags/ColumnParser collaboration are connected
to the real TeX input jax. The actual parser primary exports the declaration/value
and map families; it does not export or call the withdrawn Context runtime.

`macro.*`, `token_map.*` and `map_handler.*` keep the original Macro, token-map,
registry and subhandler responsibilities in that same parser primary. Macro
arguments are owned TeX values. `MacroCallbacks` copies two fixed typed function
tables at input-jax creation and has no rebinding/registration API; definitions
use stable one-based IDs and can recover a compiled pointer or its binding.
The actual 38-package callback table still has to be supplied by its native
configuration code. An invalid binding fails instead of selecting an erased
callable or returning a fabricated parse result.

MapHandler owns only each name's current real TokenMap. Selected SubHandlers
share those map owners, so a replacement does not invalidate an existing
configuration; after its last real owner releases it, the old map is destroyed.
Do not keep a vector of all past replacements. The ignored early declaration
packet used that retention scheme and is superseded here. A lookup key remains
separate from the stored token, as needed by let aliases. In-flight callback
arguments use value snapshots so replacement does not leave dangling borrows.
Subhandler lookup/composition, map replacement, real TexParser callback execution
and CommandMap's currentCS wrapper are Source-checked. CommandMap retains its
original wrapper responsibility rather than being replaced by MacroMap.

Newcommand keeps its configuration, mappings, methods, utility and BeginEnvItem
in the original five module owners. TexSessionState owns the sole command,
delimiter and environment definitions; session-backed TokenMap instances borrow
that data and are used only while their input jax is alive. Definition snapshots
protect in-flight calls from redefinition. Plain parameter counts are doubles,
without allocating a delimiter array for each declared argument; only a def
template requires at most nine sequential parameters and matching delimiters.
An explicit parameter declaration records whether substitution runs, including
[0] with a default argument. Suppressed commands invoke the configured fallback;
suppressed delimiters retain a null character and produce an empty mo. Global
definitions use the existing BegingroupStack over the same owned definition
layers. This does not claim that the begingroup package methods are connected.

Node construction retains the original four creator slots: node, token, text,
error. `NodeCreators` contains optional typed C++ function pointers, and the
factory merges only provided slots. Default createToken calls the configured
text then node slots; createError does likewise. Only the public node route
registers a completed result, after its creator returns. Null text differs from
an empty text node. These rules stay in NodeFactory. `NodeDefinition` and the
texClass/movablelimits/property application rules belong to the original
NodeUtil, not to the factory or renderer.

`NodeFactoryConfiguration` is the borrowed native ParseOptions protocol for the
current mathematical environment and kind/named node registration. It contains
no arbitrary keys, payloads or invocation service. The boldsymbol owner keeps
its configured token creator and variant rewrite; the token creator delegates
to the default token builder before marking/registering fixBold. Its mtext
branch does not read the parser environment. Its span-based rewrite, TexFilterData
entry point, live-node filtering, package initialization and scoped macro parsing
are connected and tested with the real TeX input jax, in addition to creator unit tests.

## File responsibility map

| Files | Responsibility |
|---|---|
| `src/math/math.ixx`, `math.cpp`, `native_renderer_bridge.cpp` | public facade, request orchestration and C ABI |
| `src/math/input/tex/tex_jax.*` | TeX input-jax lifecycle and persistent-state boundaries |
| `src/math/input/tex/session.ixx`, `session.cpp` | native macros, labels, counters, explicit session ownership |
| `src/math/input/tex/tex_parser.*`, `stack*`, `parse_options.*` | token consumption, parser scopes, stack items, request references |
| `src/math/input/tex/node_factory.*` | TeX node/token/text/error construction |
| `src/math/input/tex/<package>/` and existing package-named units | their pinned package syntax and algorithms |
| `src/math/core/mml_tree/node_kind.ixx`, `mml_node.*`, `mml_document.*` | typed node vocabulary, document ownership and tree operations |
| `src/math/core/mml_tree/attributes.*`, `mml_factory.*` | mathematical attribute inheritance and typed node construction |
| `src/math/core/mml_tree/mml_nodes/` | node-family validation, inheritance, TeX classes, operator semantics |
| `src/math/output/common/wrapper.*`, `wrapper_factory.*`, `wrappers/` | one upstream-aligned wrapper family hierarchy; BBox, stretching and shared numerical placement; fraction/sqrt/root remain in their canonical family files |
| `src/math/output/common/font*`, `src/math/fonts/` | typed font lookup, metrics, stretch data and bounded immutable resources |
| `src/math/output/scene/types.ixx`, `scene.ixx`, `scene.cpp`, `builder.*` | owned completed layout representation and freezing; no second layout hierarchy |
| `src/math/output/svg/writer.ixx`, `writer.cpp`, `node_metadata.cpp` | Source-compatible SVG serialization and metadata emission |
| `src/math/util/bbox.*`, numeric/string/length/Unicode utilities | pure mathematical/text operations with typed inputs |
| `src/platform/`, `src/app-winui/` | platform rendering, worker, cache, cancellation wiring |

## Removal and reuse rules

Remove the runtime object graph: `option_types`, `option_storage`, `option_object`,
`option_array*`, `option_iterator`, `context_property_bindings`, `native_identities`,
generic property/callable graphs, JS class/prototype services, Context module
registries, and their Context/bindings/host projection units. Remove imports of
these from algorithm modules rather than renaming or hiding them behind adapters.
The exact file inventory must account for every current math source file.

Existing Scene `ChildOwner` sparse arrays, Source identity projections and
`scene/wrappers` duplicate placement are not retained in the new Scene model.
An SVG DOM/Lite-adaptor simulation cannot sit between layout and SVG writing.
HTML/annotation support uses explicit owned content and measured runs, while
preserving the required Source output; it cannot reintroduce generic JS objects.

Pure algorithms and pinned font/Unicode datasets may be reused after their
transitive imports and ownership are inspected. A file is not reusable merely
because its name resembles a native utility. Source provenance and licenses
remain recorded. Node/CJS scripts may generate data and provide development-time
oracles; no JS engine or generated JS bundle executes in native math rendering.

## Integration policy

### TeX request and stack implementation (2026-10-20)

The original `StackItem`/`MmlStack`, `StackItemFactory`, `Stack`, `TexParser`,
`ParseOptions`, `Tags`, `base/BaseItems` and `ColumnParser` responsibilities now
have native implementations in their corresponding files. The obsolete flat
`input/tex/base_items.cpp` implementation is removed; `base/base_items.cpp` is
the sole owner of the 26 base frame algorithms. Factory construction uses the
existing typed class registry and returns activation handles without retaining
a product history. Stack owns the actual activation slots and keeps the top
and incoming handles alive through a check/pop/replay transition.

`TexScanner` remains the sole owner of the parser's immutable source snapshot,
UTF-16 cursor and current control sequence. `TexParser` delegates its lexical
methods to that owner and preserves the original dispatch, stack and LaTeX
provenance algorithms. Its private scope member registers before constructing
Stack and unregisters the exact parser on exceptions or normal `mml()` exit.
It never retains a parser in a registry after that activation ends.

`ParseOptions` borrows the effective input-jax options, package configuration
and session. It owns its factories, column parser, tagging service and
equation scratch. Its node lists contain document-owned MML borrows, preserve
registration duplicates and filter by the current root's parent chain. The
finite `in_lists` node property records additional postprocessing memberships;
it is copied by the original MML clone operation. It does not represent a
second node owner or a generic property bag. `clear()` clears only the original
ephemeral fields and tag scratch, retaining session definitions and options.

`Tags` uses the one equation record and the existing session counter/labels/ids
APIs. Numbering, nested environments, references and equation recompile state
remain on those owners. Formatted tag content invokes the actual TexParser;
there is no replacement text parser inside Tags. Source `enTag` table creation
uses the configured NodeFactory. Tag creators and the default name belong to
the actual ParserConfiguration instead of a process-wide JS class registry.

TeX assembles script nodes by physical child slot before input-jax postfilters
select the final single/double script family. `MmlDocument::setChildSlot` owns
this operation, validating document ownership and parent links. An absent slot
is a null mathematical child, not a generated placeholder or a JS array object.
Exact cloning preserves these slots. Completed layout must receive the result
of the original TeX postfilters; this addition does not authorize incomplete
script nodes to flow into Common layout.

The original ParseUtil fence/palette/initial-operator helpers and NodeUtil
child/text/operator helpers are implemented on their original owners. Token
text delegates existing MML virtual methods so HTML content retains its
meaning. No BaseItems-local copies of those helpers are introduced.

These implementations are component progress. Native BaseMethods, package
mappings, the input-jax compile/filter phases and full nested tag parsing are
still required. Tests using a small configured callback map exercise the real
TexParser collaboration, not the complete selected package profile. Source
full-profile parses are reference evidence only until the corresponding native
pipeline produces and compares their results. Full TeX-to-SVG/Folia acceptance
remains open.

### TeX frame construction and transfer (2026-10-20 Root lock)

Keep the existing generic Tree Factory registry and the actual StackItemFactory
role. MML and Common factories return document/job-owned borrowed products;
StackItemFactory must instead return a `unique_ptr<BaseItem>` to the caller.
It must not retain every constructed frame in a request arena or shared_ptr
registry. Stack owns accepted frames and releases discarded frames naturally.

Extend the existing `FactoryServices` and `AbstractFactory` templates with a
fourth result type parameter, defaulting to `Product&`. The registry still
constructs one `unique_ptr<Product>` using the same typed FactoryClass and
calls `adopt`; both `create` and `adopt` return the selected result type.
StackItemFactory selects `shared_ptr<BaseItem>` and its adopt operation converts
the newly constructed unique owner once to a frame-activation handle. Existing document/wrapper factories keep their
current reference-returning ownership contract. This is a compile-time native
ownership signature, not a runtime migration switch, duplicated registry or
compatibility path. Construction, registered order, fallback and predicates
remain the already verified Factory algorithms.

The earlier unique-owner-only frame draft is superseded by new concrete
Source evidence: CasesMethods.NumCases calls BaseMethods.EqnArray, which pushes
the begin frame, then pushes that same frame again. The actual stack contains
two simultaneous slots for one CasesBeginItem; its mutable fields, environment
and globals are shared. Cloning, deduplicating or moving away the sole handle
would change this configured package. Root replayed the actual Source methods
in the formal `tools/math-port/tex-frame-activation-source.cjs`, with explicit argument/
tag hook limits. This finding concerns activations, beyond the earlier review
of checkItem replacement lists; it does not invalidate those observations.

Use `FrameHandle = shared_ptr<BaseItem>` only for actual frame activations.
Factory callers, Stack slots and transition locals may share that handle;
frames never own their parser, factory, outer Stack or document. There is no
factory product registry, request frame arena, raw-pointer adoption or
shared_from_this bridge. The last slot/local release destroys the frame by
normal RAII. MML remains document-owned and environment remains a finite value.

For Stack transitions the active top stays alive during checkItem, which takes
`FrameHandle const& incoming`. ProofTree passes a handle into its inner Stack
at the actual Source call site before returning Drop; outer Drop releases only
its local reference. A typed replacement list contains newly created handles,
reuse-top/reuse-incoming tokens and borrowed MML. Stack first executes the
actual Pop/environment transition, then resolves those tokens with the original
handles still available, then recursively pushes entries in order before
continuing the outer input sequence. Aliased slots still see the same frame's
mutations. Do not add a universal object ownership or identity service.

The actual Source observations are in ignored `tex-stack-transition-source/`:
24 cases and 88 hash-locked files, replayed by Root. Left/middle reuses top;
Over/Style/Position/Prime/Nonscript can reuse incoming; Prime also returns bare
MML. Source `delete item.env` does not clear BaseItem's inherited accessor
backing `_env`. Thus a surviving replayed/popped frame's environment cannot be
cleared based on the source statement alone. The rest of parser/ParseOptions
declarations remain to be frozen separately.
This contract does not claim a native parser or the full package profile works.

Use the existing closed `TexEnvironment` value record for environment contents.
Stack and BaseItem share a `shared_ptr<TexEnvironment>` only where Source has
one active environment scope: a non-open accepted frame aliases that scope;
an open frame starts with a fresh empty record and overlays present fields from
the previous scope when copyEnv is true. ArrayItem has copyEnv=false. Start
receives the already copied parser seed. BaseItem.clearEnv clears the existing
record in place (`*env = TexEnvironment{}`), so existing scope aliases observe
the cleared fields. The distinct explicit `this.env = {}` in Left/middle
installs a fresh record before replay; do not conflate those operations.
Pop restores the remaining top's scope or a
fresh empty scope; it does not erase the popped frame's record. This finite
domain value contains declared bool/number/string optionals and the specific
multi-letter regular-expression reference, with no
parser/frame/MML owner, callbacks or universal property lookup, so it cannot
form an ownership cycle. Copy operations copy the values rather than sharing
the new open scope. StackGlobal is a separate typed stack-owned record borrowed
by its frames; it is not an extension namespace in TexEnvironment.

Transition results distinguish Accept, Drop and Replay explicitly. An empty
Replay still pops the old top and is different from Accept. Source errors are ordinary TexError exceptions;
checkItem throws before Stack Pop, so the original owners and active scope
remain valid while the caller handles the error. Native tests must exercise
success and throw cleanup, scoped environment aliasing/copying, same-top replay,
incoming replay and bare-node replay against the actual Source observations.
They must also check actual CasesBeginItem aliases, shared mutations, and final
destruction after the last slot/local releases, including nested-stack Drop.

TexParser remains the owner of source positions and stamping StackItem.startStr,
startI and stopI at the actual Push call. Its existing TexScanner is the sole
owner of the current UTF-16 source through `shared_ptr<const u16string>` and
exposes a `sourceSnapshot()` value for those immutable frame snapshots. There
is no parallel current string in TexParser. `replaceSource` installs one new
immutable buffer and retains its existing cursor semantics (no implicit reset);
old frame snapshots remain valid until their last owner releases them. Cursor
reset is the explicit parser expansion operation, not a scanner side effect.
The buffer contains text only, never parser/MML/frame owners or callbacks.

There will be one native math implementation and one `folia_math` target.
No `native2`, old/new selector, runtime compatibility adapter or legacy fallback
is introduced. Replacements use the canonical file paths/module responsibilities.
During reconstruction, incomplete layers are reported as incomplete; they cannot
silently call the withdrawn Context implementation or return fake SVG success.
Root switches the target to the reviewed native dependency graph as a deliberate
source change, and removes obsolete runtime files/tests in reviewable batches.

Folia retains its current math worker until the complete native SVG path passes
the new acceptance gates. Then it switches once through the C ABI. Mermaid is
unchanged and paused. An additional D2D pixel comparison is not a gate for this
SVG integration; the future D2D backend remains a separate consumer of Scene.

### Enclosure Scene entry (2026-10-21 Root lock)

The table hline Source case creates a real menclose; the observed factory error
requires the existing CommonMenclose to become reachable. Keep the immutable
29-notation SVG profile in its original svg/menclose_notations owner. Add an
explicit optional notation span to CommonWrapperFactory::standardNodes; callers
at the composition boundary supply that existing profile. An empty span keeps
the explicit missing-profile error. Common never imports SVG or duplicates the
catalog. The supplied catalog must outlive its layout job (the pinned SVG catalog
has static storage), just as CommonMenclose already requires.

CommonMenclose::emitScene owns traversal on its original menclose pair:
standardSceneNodes, one enclosure_content group translated by the existing bbox
left extender, the original child or initialized msqrt, then ordered notations.
Reuse notationGeometry/sceneGeometry completed em geometry; do not recalculate
line endpoints in the writer. The first accepted subset is the Source line
recipes (side, paired side, diagonal and horizontal/vertical strike), plus radical
child traversal. Other shape recipes remain explicitly unsupported by emission
until their exact typed primitive projection is implemented and Source-tested;
a path approximation must not be presented as byte-equal rect/ellipse output.
Line marks are Scene PathElement::line with enclosure_mark role; SVG serializes
completed coordinates then Source stroke-width, with inherited fill/stroke.
No additional group is introduced around each line. The required table row-rule
case and all line/radical combinations now match the pinned SVG.

Table follow-up Source evidence: SvgMtable.handleColor unconditionally writes
getWidth() to its first output child; that child can be the bottom broken-border
line, rather than a bgcolor rectangle. The 48-case run exposed the missing width
attribute. Keep that completed table width as Path::layout_width (optional em
value, not line stroke thickness or a second geometry). Builder::set_background_width
updates only a first background Rule's bounds.width or first background Path's
layout_width. Other first entries return false. Mtable calls it at the original
handleColor stage; SVG writes the optional path width after paint/metadata.
The width does not change path geometry. Validate it finite and nonnegative.
This replaces the earlier assumption that table color sizing only reaches a rect.
Solid border insertion also retains the original captured child anchor; broken
borders independently insert before the current first child, as Source specifies.

Direct MathML table acceptance uses the pinned package's MathML compiler only
to reach equalrows/equalcolumns/dotted attributes absent from the TeX profile.
The independent Source tool freezes the actual compiled MML tree, typed own
attributes, table metrics and raw SVG. Native tests rebuild that exact tree via
MmlDocument and consume the same Common LayoutJob and SVG writer; no native
output is used as an expectation. The test fixture uses flat constexpr records
to avoid MSVC heap exhaustion on recursive vector initializers. Its provenance
locks the MathML compiler separately; it is not extra TeX package coverage.
Source preserves mtr mathsize="150%" but keeps that row's rscale at 1 in the
captured case. A separate mstyle scales cell content to 1.5. These observations
do not establish nonunit row-scale coverage.

### Complete enclosure geometry (2026-10-21 Root lock)

The 29-notation Source survey and four valid boundary cases are captured by
`tools/math-port/common-scene-source.cjs` in the canonical 148-record fixture;
the original 115 records remain unchanged. Actual Source output puts the sole
stroke-width style before path d, with separate cap, join and fill attributes.
Phasor's join token is `mitre`. Empty roundedbox retains rx=.267 for a .467-wide
rect. Preserve those observations without retaining SVG markup in Common.

Extend the existing Scene types, not a second enclosure scene hierarchy:

- `CoordinateMode {absolute, relative}`; trailing `mode` on LineTo and ArcTo,
  default absolute. Relative endpoints are completed local deltas, not formulas.
  Consumers resolve ordinary vector path coordinates; SVG retains l/a commands.
- `ArcFlagSeparator {space, comma}`; trailing `flags_separator` on ArcTo,
  default space. This affects serialization only, retaining Source's `0,1` token.
- Rule gains trailing `optional<Unit> corner_radius`. It denotes a symmetric
  rounded rectangle; keep the declared radius. Consumers apply ordinary shape
  radius limits, while SVG retains the supplied rx. No hand-built corner arcs.
- Ellipse contains `Point center`, `Unit radius_x/radius_y`, Paint, metadata and
  role. Append it to Primitive so existing alternatives keep their indexes.
- Path gains trailing Transform, `PathSyntax {compact, spaced}`,
  `PathPaintEncoding {attributes, stroke_width_style}`, and
  `StrokeJoinSpelling {standard, mitre}`. Geometry, paint and transform remain
  authoritative once; the finite spelling fields affect projection only.

Scene freeze validates new enums, finite values, nonnegative radii and path
transforms and snapshots them. The mitre spelling requires an actual miter
paint join with stroke_width_style. Relative commands have the same finite
coordinate validation as absolute commands. Do not store SVG d/style strings.

In the original Common Notation pair, replace the path-only NotationGeometry
record with `variant<scene::Rule, scene::Ellipse, scene::Path>`. Keep existing
bbox/extender/arrow formulas and traversal. Box emits one Rule; roundedbox adds
its existing thickness+padding radius and deletes its old path approximation;
circle emits one Ellipse with the existing center/radius formulas. Lines retain
their proven PathElement::line emission. Phasor/longdiv retain their math and
use spaced/stroke_width_style Paths; longdiv's arc stores its actual relative
delta and comma flags. Arrows reuse the original arrow helper, retain relative
line deltas at the original Source calls and move its transform onto Path.
Remove the superseded absolute-path construction helpers when no longer used.
All these primitives carry enclosure_mark role; there is no extra mark group.
CommonMenclose emits the variant once and preserves its accepted child/radical
traversal. Its previous shape restriction is removed only with concrete output.

File ownership: Scene types/validation and native_raii_scene UT stay with the
Scene owner; writer.cpp/native_svg_writer UT only serialize those values;
common/notation pair owns numerical recipes and native_common_menclose UT;
CommonMenclose pair owns traversal; common-scene-source/fixture/common_scene UT
owns independent full38 Source comparison. No new production files or modules.
Root owns interfaces, build parallel4, Source generation review and small commits.

SVG projects enclosure rect x,y,width,height,fill,stroke-width,optional rx;
ellipse rx,ry,cx,cy,fill,stroke-width; stroked path style,d,cap,join,fill;
filled arrow d,optional transform. Noninherited paint values must still be
represented. Defaults preserve all already accepted table/glyph/line output.
Spaced path commands and numbers use Source spacing; compact paths remain as
before. Transform belongs on the path, never an added g.

Acceptance: all 29 actual TeX notations plus four surveyed boundary inputs and
the existing mixed enclose/cancel package cases compare raw SVG bytes through
TeX -> MML -> Common -> Scene. Keep the prior 115 Source records unchanged;
do not relabel this extended corpus as the outstanding canonical 75/150 gate.
Preserve existing exact layout expectations, replace path-approximation UT
assertions with the real rect/ellipse/delta values, and exercise freeze/retirement
for the new values. Root runs focused comparisons, then the complete math tests.

### Semantics and fenced traversal (2026-10-21 Root lock)

Add Scene overrides in the original semantics and mfenced declaration/implementation
pairs only. Their accepted Common construction, bbox and linebreak algorithms
remain authoritative. No new production module or permissive base-wrapper gate.

CommonSemantics first uses toEmbellishedScene. Otherwise it creates standard
containers, directly emits only presentation child0 into their content parents,
and returns its own nodes. Empty children leave the standard containers; a null
presentation child is an explicit error. Do not add child placement or traverse
annotations. This does not introduce foreign XML/HTML payload support.

CommonMfenced creates standard containers, then emits its existing inferred
mrow into content parents, without extra placement. Temporarily point each
direct child wrapper's parent at that mrow. Capture every original pointer
before mutation and restore its exact value with local RAII on success or
exception; never assume the prior pointer was this. Generated fence/separator
wrappers already belong to the mrow. Do not change MML parents or rebuild rows.

Independent direct-MathML evidence reuses the pinned compiler/Common/SVG/NewCM
boundary. Extract that runtime/capture into common-mathml-source-runtime.cjs;
the existing table entry retains its table observations, fixture formatting,
CLI and unchanged eight-case output. New semantics/fenced cases and entry live
in common-semantics-fenced-mathml-cases.json and
common-semantics-fenced-mathml-source.cjs, with a separate canonical
semantics-fenced-mathml-scene.inc. Preserve original element tree records;
actual text nodes additionally carry getText() contents. Unsupported XML payload
capture fails explicitly. A single runtime/reference instance serves a process.

Shared test-only flat tree reconstruction belongs in
tests/math/support/mathml_scene.hpp, namespace mathml_scene_test:
AttributeKind{text,boolean,number}; Attribute{name,kind,text,number,boolean};
Node{kind,is_inferred,parent,attribute_first,attribute_count,text};
Metrics{em,ex,container_width,display};
Case{name,metrics,nodes,attributes,source_svg}. Attribute names and text use
u16string_view; case names/kind/SVG use string_view. Spans borrow constexpr
fixture arrays. Node text
is trailing and defaults empty. constructRoot(document,nodes,attributes) returns
an owned MML root, checks parent/attribute ranges and inference markers, and
uses allNodeKinds/nodeKindName instead of duplicating a kind catalog. It sets
actual text-node contents; it does not parse markup, infer layout or keep owners.
Existing table tests reuse these types/construction, retaining table assertions.

Dedicated native_common_semantics.cpp and native_common_mfenced.cpp tests consume
the new Source cases. Keep their responsibilities separate; shared fixture arrays
are sourceSemanticsCases and sourceMfencedCases. Compare complete SVG bytes after
fresh document/job layout and again after their destruction. Exercise ordinary,
scaled/nested presentation, hidden text annotations and empty annotation-xml,
embellished forced breaks, default/custom/repeated/empty separators, absent
fences, nested and tall-content fences. Actual XML annotation payload coverage
waits for its original XML owner. Add focused direct wrapper checks for empty
semantics and exact fenced parent restoration, including a throwing child.

Root owns CMake, build parallel4, native verification and commits. Separate Luna
owners handle semantics+its tests, mfenced+its tests, the shared test helper/table
reuse, and Source tools/cases/fixture. Freeze interfaces before builds. Acceptance
requires new Source reproducibility, unchanged table fixture, successful focused
Source/ownership tests and one complete math run after final production changes.

### Canonical profile discovery (2026-10-21 Root lock)

Use the first 75 records of the existing Common Scene witness: the fixed 18
baseline inputs followed by the locked 57 profile inputs. Preserve their order,
input metrics, expected Source errors and actual initial TeX options. Additional
focused and direct-MathML cases do not replace this corpus.

The existing Common Scene test may select this corpus with a test-only
FOLIA_MATH_SCENE_PROFILE=canonical environment variable. Default focused86
behavior stays unchanged. This is an evaluation selector, not a production
renderer option. Execute the real TeX/MathItem/Common/Scene/SVG path for every
case and report actual success, first SVG difference or native exception. Source
errors must be reported separately with their expected name/message and the
observed native outcome; do not count any two exceptions as equivalent. Keep
failed/unverified cases visible and do not change expectations to make them pass.

The Source owner records input flags before conversion (including
initialAllowTexHTML), final flags and reference errors in ignored evidence,
using the existing pinned reference once per process. Existing SVG fixture
bytes remain unchanged unless an independently established input mismatch
requires a separately reviewed correction. Use prior TexHTML observations as
the settled input boundary. Native measurements stay on the actual existing
typed MeasurementTarget; do not supply per-case invented bounding boxes.

Root runs one build with parallel4, then the bounded 75-case evaluation. Only
after examining its actual results will Root freeze the next original-owner
implementation tasks. Abnormal native process exit stops broader execution.
Passing focused86 or compiling the probe does not establish the 75/150 gate.

### Canonical composition repairs (2026-10-21 Root lock)

The pinned 148-record capture uses one persistent TeX input session; canonical75
must use the same scheduling, while each compilation still owns a new
MmlDocument and freezes a document-independent Scene. Reuse the existing TeX
compile/setMmlFactory boundary. Preserve the Source fixture and case order;
do not replace continuous-session evidence with ignored fresh-case candidates.
The default focused test keeps its established scheduling.

The canonical test composition supplies a real typed LiteAdaptor to
CompileServices and to unknown-text measurement. Reuse LiteAdaptor::nodeSize
on a real text element (1000, true); SVG measurement consumes its width and
uses the Source SVG constants h=.75/d=.2. Do not duplicate NodeMixin character
classification or use case-specific measurements. The default focused sentinel
continues rejecting unexpected host measurements. This is test composition;
production Common host wiring and HTML Scene emission remain separate work.

Newcommand initialization belongs to the original package/session owner, with
idempotence tied to live session state rather than a borrowed address comparison.
Fix that boundary and test deterministic successive TeX lifetimes, preserving
within-session definitions and the existing repeated-initialization contract.
Font remap precedence belongs to FontData's existing resolver; direct glyphs
must prevent inherited remaps from overriding Source character ownership.
Neither issue belongs in Common row traversal or fixture special cases.

### SVG merror owner and immutable values (2026-10-21 Root lock)

Source has no Common Merror mixin: SvgMerror directly extends SvgWrapper.
Port the existing legacy scene/wrappers/merror pair to svg/merror, replacing
MerrorPlacement with SvgMerror : CommonWrapper and emitScene. Remove the old
placement registration; do not retain two merror implementations. The original
svg/wrapper_factory pair becomes a native thin CommonWrapperFactory subclass;
its defaultNodes() supplies the concrete Common catalog plus SVG notation and
merror registration/class metadata. Common must not import SVG or gain an
merror source-text branch. Test composition consumes this SVG catalog.

Scene additions are narrowly scoped owned values:

- Title { std::string text; } is ordered semantic content in Primitive. It has
  no geometry and is not a TextRun or Group; freeze copies its string. Existing
  Group.tooltip behavior stays unchanged.
- ElementRole::error_background identifies the Source merror rect encoding:
  data-background, width, height, y; x is zero and omitted. Geometry is the
  completed em rectangle {0,-d,w,h+d}; the writer does not calculate layout.
- PaintColors { PaintColor fill, stroke; } and trailing optional
  Paint::stylesheet preserve class-rule color declarations separately from
  authored presentation fill/stroke. Both are owned typed values; a blue
  authored color is not lost when the merror class declares red. SVG serializes
  authored paint and retains Source class styles, not extra inline class-rule
  attributes. This does not introduce a general CSS engine or a second layout.
  Class-style cascade outside these merror declarations remains in its separate
  profile acceptance work.

SvgMerror uses standardSceneNodes once. Append background then a truthy title
to its first own node, then call the existing addSceneChildren on the content
parents. Preserve source own/content split, bbox, child order, scale and links.
Attach the Source yellow/none class paint to the error background and red/red
class paint to directly contained g children. Reuse child_groups and paint
accessors; add read-only Builder::link(NodeId) and viewport(NodeId) value
accessors so the owner can exclude children serialized through an outer anchor
or as nested svg, which do not match the Source direct-g selector. Do not
recursively repaint descendants or overwrite authored paint.
The two Source CSS selectors and their color spellings belong only to this
SVG owner and its class registration; derive native declarations from those
same constants rather than scattering another palette in Common or Scene.

Writer support adds only Source Title text/order and error rect encoding. Scene
freeze must retain Title and stylesheet strings independently of request owners;
other primitives, tooltips and paint encodings keep their accepted behavior.
Root owns rename/retired registration/CMake/build/Git. Scene, SVG writer,
merror/factory and Source/UT owners have disjoint files. Acceptance requires
independent pinned MathML witnesses (normal/empty title, special-character title,
multiple/nested children and authored color), exact SVG, class metadata,
stylesheet-vs-authored paint, retirement, canonical75 and one full math run.
The two Source action errors and HTML remain outstanding, not merror passes.

Source observation during implementation corrects the planned Title escaping:
pinned LiteParser.PCDATA explicitly includes title, and serializeInner joins its
text values verbatim even for serializeXML. The observed title containing &,
quotes and <tag> is therefore unescaped in the raw Source SVG. New Title.text
preserves semantic text as an owned string; the SVG writer reproduces that
specific Source title serialization rather than changing the independent
fixture. It is not stored or interpreted as a prebuilt SVG fragment. Existing
Group.tooltip serialization and other text escaping are not changed by this
observation. The direct Source special-character witness is required evidence.

### XML/HTML native owner and request services (2026-10-21 Root lock)

Migrate the existing Common XmlNode, SVG xml_wrapper, Scene html_types/snapshot,
and SVG serializer owners. No second HTML tree implementation, Property/Context
bridge, serialize-then-parse production path, alternate layout, or legacy fallback.
The following interfaces are fixed before parallel implementation:

- MML XMLNode adds a const `xmlAdaptor()` getter returning its existing shared
  const XmlAdaptor owner. HtmlAdaptor imports core.dom_adaptor and adds pure
  `DOMNodeRef cloneDOM(XmlRef) const`. XmlDomAdaptor implements it as an owned
  deep clone of its existing frozen typed Lite subtree; never expose mutable
  stored nodes. The two opaque test adaptors explicitly reject DOM export.
- Common types adds `HtmlMeasurement { AbstractDOMAdaptor& adaptor; DOMNodeRef
  parent; DOMNodeRef container; }`. Parent is the actual math-start parent,
  container is the request-owned output measurement container. Add trailing
  optional<HtmlMeasurement> CommonOutputJaxConfig::htmlMeasurement. CommonOutputJax
  owns that optional for its request lifetime and exposes a checked
  `HtmlMeasurement const& htmlMeasurement() const`. Missing services fail explicitly
  only when XML output needs them. Resolve automatic htmlHDW once using the
  real adaptor.canMeasureNodes: true -> ignore, false -> force. This is not a
  general callback bag. Scene retains none of these services or DOM refs.
- CommonXmlNode stays abstract (addHDW is backend-owned), derives CommonWrapper,
  and overrides initializeSource, no-op getStyles/getScale/getVariant, computeBBox.
  Keep Source method names and order: getHDW returns optional<u16string>,
  splitHDW returns UnknownBBox, getFontStyles returns existing styles::StyleList,
  getHTML returns DOMNodeRef, addHDW(DOMNodeRef, styles::StyleList&) is abstract,
  measureXmlNode returns UnknownBBox; public rscale and protected cloneXML()
  support the backend. cloneXML uses the XMLNode HtmlAdaptor owner.cloneDOM.
  Font precedence remains parent styles, metrics.family, actual adaptor.fontFamily
  of measurement.parent, then initial. Formatting uses accepted lengths/fixed
  utilities; no per-case literals. Build and attach the real measurement DOM,
  call actual nodeBBox, and RAII-remove request container/measurement even if
  either measurement throws. Never substitute zero for a measurement call.
- Scene html_types becomes standalone primary
  folia.math.output.scene.html_types, without legacy foundation/Transform imports.
  Keep HtmlNodePrimitive's existing owned ordered attributes/text/children and
  namespaceHint. ForeignObjectGeometry becomes owned CSS-pixel x/y/width/height
  plus `double em_per_pixel=1` (pixels -> math em); remove old transform and
  synthesizeMissingAttributes. HTML is the explicit unit exception to em geometry.
  html_snapshot becomes standalone primary folia.math.output.scene.html_snapshot;
  snapshotHtmlNode uses only typed AbstractDOMAdaptor and typed comment text.
- Replace ForeignContent raw markup/runs with `{HtmlNodePrimitive content;
  ForeignObjectGeometry geometry; SemanticMetadata metadata;}`. Remove ForeignRun
  and ForeignContent from Primitive. Add trailing optional<ForeignContent>
  Group::foreign_content and matching trailing Builder::add_group argument with
  default nullopt; mutable/frozen groups, copies and split operations retain it.
  Builder::has_foreign_content(NodeId) const reports presence with normal handle
  validation; merror excludes these foreignObject roots from its direct-g rule.
  This is the single NodeId-bearing foreignObject, so normal placeScene can
  prepend em translations without an extra g. Use existing
  ElementRole::foreign_content_viewport. A foreign group has no ordinary entries,
  viewport, clip, tooltip or link; reject invalid combinations. Validate finite
  nonnegative CSS width/height, finite x/y and positive em_per_pixel. Freeze owns
  the full HTML subtree and never retains a DOM/document/adaptor/wrapper.
- SVG xml_wrapper exports native SvgXmlNode : CommonXmlNode and SvgHtmlNode
  (no extra layout), original addHDW, emitScene and concrete registration functions
  xmlRegistration()/htmlRegistration(). Source addHDW builds mjx-html-holder then
  updates StyleList height/width/vertical-align and removes font-size/font-family.
  emitScene uses parents[0] only, no standardSceneNodes or extra g. Get metrics,
  getBBox, getHTML in Source order; snapshot once and add one foreign group with
  geometry {0,-h*em,w*em,(h+d)*em,1/em}, em=metrics.em*metrics.scale*rscale.
  Common catalog stays abstract; SVG factory replaces xml/html registrations and
  retains actual Source class styles (including Common XML styles).
- SVG serializer pair is migrated from the inactive Property scene serializer
  to the narrow existing materializeHtml responsibility. Public
  `std::string serializeHtml(scene::HtmlNodePrimitive const&)`; private typed
  materialization into a local LiteAdaptor followed by serializeXML preserves
  Source escaping, comments, PCDATA, namespaces, void and empty elements.
  This is temporary serializer output only, never a second retained/layout tree.
  It must not parse source text or do layout/measurement. Existing writer calls
  it for the owned foreign content; do not duplicate the LiteParser rules.
- Writer emits foreignObject directly for Group.foreign_content: data-mjx-xml,
  optional nonzero x, y/width/height as fixed CSS px, transform then contents.
  Combine normal group transform (em -> SVG translations) with Source
  scale(1000*em_per_pixel) matrix(1 0 0 -1 0 0); this is unit projection only.
  Do not put the SVG 1000 factor in Scene. No recomputed bbox/font layout.
  Keep existing all other primitive/viewport/cache behavior and title rules.

Root owns CMake, common_scene composition, old XML placement/callback removal,
format/build/tests/commits. Old scene/xml_node pair and XmlSvgNodeCreator are
retired, not adapted. The inactive SVG output_jax caller of that bridge is also
removed in the same cleanup; no Context binding may be added to active code.

Acceptance: independent pinned Source direct MathML XML/HTML witnesses cover
unknown measurement, nonzero forced HDW, use/ignore/automatic policy, scale !=1,
parent font precedence, escaped text/attributes, comments/empty/void/mixed
namespace and placement after preceding math. Capture actual styles and bbox;
fixtures are flat constexpr and default/no-op reproducible. Native tests compare
exact SVG, real measurement calls, exception cleanup, payload cloning and Scene
after all request owners retire. Shared test helper may accept an optional typed
XML initializer callback (default rejects XML as before); do not add XML parsing
to production Common. Then default86, canonical75 and a full math run. The two
Source action event errors remain outstanding; no claim of complete75/150 or
Folia integration follows from this batch.

Source XML scale-2 witness reveals a missing pre-existing CommonOutputJax
setScale responsibility: output option scale and math metrics.scale are distinct.
Extend the same owner batch, without changing expected SVG: trailing
DisplayLayout::font_scale defaults to 1 and is finite/nonnegative (Source
overflow=scale can resolve to zero when containerWidth <= 4). CommonOutputJax
finishScene resolves metrics.scale * options.scale, including Source display
`overflow=scale` width fitting (containerWidth-4, pxPerEm, outerBBox), into this
owned final value. It must not derive it from XML geometry or change em layout.
Writer serializes the resolved scale as container font-size percent using the
existing native Styles/length formatter. Preserve authored style order/override,
merge minimum width in Source order, and insert generated scale style before
processMath display/width/justify attributes (when scale=1, existing ordering is
unchanged). Separate Scene validity, Common computed-scale, and writer focused
checks cover this addition. The Source scale-2 case keeps actual metrics.scale=1.

The direct MathML XML witness reaches SvgAnnotationXML, which Source defines in
SVG/Wrappers/semantics.ts as an ordinary SvgWrapper plus one class-style rule.
Allow annotation_xml in CommonWrapper's existing generic emitScene dispatch
(the existing Common catalog registration remains generic). In the existing
svg/xml_wrapper pair add SvgAnnotationXML : CommonWrapper and
annotationXmlRegistration(); SVG catalog replaces that one registration with
the Source foreignObject[data-mjx-xml] class styles. No second annotation
layout, extra Scene wrapper or generic fallback. Source observer captures the
actual annotation-xml constructor.styles into a separate flat typed table;
native catalog metadata must match that table as well as XML/HTML metadata.

### 2026-10-21 Root lock: maction ownership and nested math

Keep CommonMaction's accepted selected/BBox/break algorithms and the actual MML
selection owner. Common adds getParameters() and tipDx()/tipDy() values only,
using Source data-offsets splitting/defaults/length2em after base initialization.
CommonOutputJax::buildScene(CommonWrapper&) extracts the existing LayoutJob
completion path; LayoutJob delegates to it. This entry takes an already wrapped
auxiliary math, uses the same factory/measurement/finishScene, and does not parse,
change document.root, mark breaks or re-run TeX classification.

Scene primary owns an ActionRegion entry (Rule hitbox plus optional shared const
completed Scene tooltip), alongside Primitive and const Group entries. It is a
semantic rectangle with optional mathematical tooltip, not a second HTML tree
or prebuilt SVG. Freeze copies the pointed-to Scene value to sever mutable
aliases; completed Scenes retain no wrappers/MML/DOM/adaptors. Nested scenes have
their own NodeId scope. Group gains optional Action, a variant of ToggleAction
(double selection, size_t child_count, bool enriched), MathTooltipAction
(Unit dx/dy, uint32_t post_delay_ms=600/clear_delay_ms=100) and StatuslineAction
(owned UTF-8 text). These are interaction descriptions, never callbacks/timers.
Builder provides add_action_region(NodeId, ActionRegion) returning its entry
index, set_action_region_tooltip(NodeId,index,shared_ptr<Scene const>) for late
attachment after selected-child emission, and set_action(NodeId, Action).
Copies/splits preserve action. Reject nonfinite offsets and invalid
region geometry. Toggle selection retains all numeric values, including NaN and
infinity: the already accepted MML selection algorithm handles these and Source
can serialize their data-toggle spelling. Text tooltip uses the existing raw
Source Title entry.
No frontend dispatch/rerender capability is claimed by these data descriptors.

Original SVG maction_surface pair will become svg/maction pair and native
SvgMaction : CommonMaction, owning hitbox/selected-child/action ordering and
Source class styles. For mathematical tips it creates auxiliary math in the
same request MmlDocument, wraps with the same factory, builds the nested Scene
through CommonOutputJax, and restores the tip's original parent by RAII on both
success and exceptions. No generic Common fallback or duplicate placement file.
Writer projects ActionRegion as Source rect (width,height,x,y,fill,pointer-events)
and its optional hidden foreignObject/mjx-tool/mjx-tip/nested SVG, without extra
groups or container. SVG font-cache traversal must follow independently observed
Source nested-cache behavior; independent write() calls cannot assume equivalent
cache state. Source witness must include none/local and nested-following glyphs.

Keep existing strict Lite errors/goldens intact. A separately labelled witness
adds a real EventTarget registration host to actual Lite nodes, never replaces
MathJax handlers. Capture actual callbacks' registration receiver/type/order,
before/after serialization, Source class styles, BBoxes and successful SVGs.
Cover toggle/selection, text/math tooltip, statusline, missing tips, unknown
action, offsets, line segments and nested tips; test owner retirement and parent
restoration. Root reviews evidence and interfaces before SVG implementation;
Root alone runs native/build, owns CMake/legacy cleanup and small commits.

Historical initial gate, superseded by the accepted resource-order batch below:
ActionRegion first accepted fontCache=none and local actions without math tips;
local math tooltips initially reported UnsupportedError. All retained local
witnesses now pass after implementing the complete generation-order cache plan.
A per-Group tooltip deferral is insufficient for multiple own
segments: Source emits all selected segments before processing all tooltips.
Do not serialize each nested Scene independently and claim cache parity.

### 2026-10-21 Root lock: resource emission order and recursive local cache

Generation order and drawing-tree order are distinct Source facts. Record one
trailing uint64_t resource_order on GlyphRun and ActionRegion (0 means unspecified
input). Builder owns a request-local monotonic counter starting at 1. Assign an
order when adding/inserting a nonempty GlyphRun; assign an ActionRegion's order
when a mathematical tooltip is present at creation or attached later. Clearing
that tooltip resets its order. Ignore caller order on Builder insertion, retain
orders through moves/splits/freeze, and reject counter exhaustion before mutation.
This changes no geometry, drawing order, MML ownership or backend selection.

Scene snapshot validates resource orders per Scene, excluding nested Scenes'
independent scopes: all resource-bearing entries must either specify unique
nonzero orders or all omit them. A direct Group-based Scene whose entries all
omit them receives deterministic tree-traversal orders during its snapshot;
mixed specified/unspecified and duplicate explicit orders are invalid. Existing
completed nested Scene values are copied immutably as before. Non-resource
entries and empty GlyphRuns need no order. This is one finalized resource order,
not a second mutable output tree or retained callback program.

Port the original svg/font_cache pair to native FontCachePlan, retiring its old
Property/NodeRef bridge. API in that owner:
  CacheScopeId = size_t;
  CacheDefinition {string id; scene::Glyph const* glyph;};
  CacheScope {vector<CacheDefinition> definitions;
              unordered_map<scene::Glyph const*,string> references;
              unordered_map<scene::ActionRegion const*,CacheScopeId> tooltips;};
  FontCachePlan(Scene const&, span<string const> prefixes);
  static size_t FontCachePlan::scopeCount(Scene const&);
  CacheScope const& FontCachePlan::scope(CacheScopeId) const;
Pointers are strictly temporary borrows into the write() input Scene; the plan
cannot outlive it and never enters Scene/session caches. Root scope is 0. A
nested occurrence gets its own scope even when immutable Scene data is shared.

Replace SvgWriteOptions.local_id with vector<string> local_cache_prefixes,
containing the complete resolved Source prefix for root then nested processMath
calls in generation order (including trailing '-' or empty fixed prefix). The
renderer/session, not this pure serializer, allocates automatic IDs. Repeated
fixed prefixes are valid. Prefix count must equal scopeCount when glyph references
exist; if the entire Scene has no glyphs, an empty list can omit unobservable
identities. Expose scopeCount for the eventual renderer allocator; do not infer
automatic/fixed mode by parsing an ID string, retain a second ID option, or mutate
external counters in write(). Update all existing callers in this same batch.

FontCachePlan collects each Scene's GlyphRun/tooltip events and sorts by finalized
resource_order. Entering processMath creates a defs bucket, clears the active
cache, and consumes its supplied prefix. Each glyph records its href in the
current invocation's references and adds its first ID definition to the currently
active bucket. Recursing through a tooltip replaces that active bucket/cache;
return clears the prefix and deliberately leaves the nested bucket active, as
Source does. Later outer glyphs may therefore have definitions in nested defs.
Per-invocation reference maps avoid alias collisions when data is shared. Writer
emits the original drawing tree, looking up hrefs by scope and glyph address;
each SVG receives its planned defs at existing Source single/inline placement.
Local empty caches serialize as <defs/>. None mode retains inline paths; global
keeps its current explicit document-owner requirement. Remove local math-tooltip
Unsupported once its real cache path passes independent Source byte comparisons.

Luna ownership: scene_values_writer owns types/Scene/Builder + its order tests;
common_scene_entry owns original font_cache pair + plan tests; common_base_emission
owns writer + existing writer caller migration; common_scene_source_tests owns
the Source observer/fixture/maction Source UT. Root owns CMake, inactive old
FontCache callers, review, builds and commits. Source adds actual useLocalID
prefix observations and multi-segment math-tooltip cases, preserving previous
inputs/SVG bytes. Acceptance covers auto/fixed/empty prefixes, nested/following
glyphs, multi-segment scopes, empty defs, immutable sharing/retirement and full
regression; no per-case production special cases or simulated JS objects.

### 2026-10-21 Root lock: mglyph image owner and Scene value

Keep CommonMglyph getParameters/computeBBox, charWrapper and document/factory
ownership. Do not create a second image layout or reparse its attributes in the
writer. The existing abstract Common registration remains a shared-layout owner.
Add native svg/mglyph.ixx/.cpp corresponding to Source SVG/Wrappers/mglyph.ts:
SvgMglyph final : CommonMglyph, emitScene and mglyphRegistration(). SVG catalog
replaces the mglyph registration. standardSceneNodes returns own/content roots;
the index branch calls the existing charWrapper.emitScene with content parents,
without adding another math tree child or extra placement. The image branch
appends exactly one image to content[0] and returns own nodes, matching Source.

Scene types adds Image {Size size; string href; string alt; Transform transform;}
to Primitive. These are owned values; sizes are authored mathematical em extents
and may be finite signed or zero, because Source length2em does not clamp them.
Snapshot validates finite width/height and the existing finite Transform rules,
deep-owns the strings and retains no DOM/adaptor/loader/Context. Images are not
font-cache glyph events and have no resource_order. Existing Size/Transform and
ElementRole::image_glyph express geometry; no prebuilt SVG/HTML or opaque handles.
SvgMglyph resolves size from existing width/height, copies src/alt, and supplies
ordered Translation{0,height+(valign||0),space}, Affine{1,0,0,-1,0,0}. Do not
invert BBox or multiply dimensions by SVG's 1000 in Common/Scene.

Writer serializes Image directly and self-closes: width,height,transform,
preserveAspectRatio="none",aria-label,href in exactly this Source order. Use
existing fixed numeric/transform/attribute projection, including 1000 conversion
and empty alt; no x/y, extra g, image decoding, intrinsic-size inference or URI
fetching. All image data belongs in the existing primitive/Scene path. Retire
the old inactive scene/wrappers/mglyph pair and its placement registration; do
not keep two implementations or adapt Property/NodeRef to the new value.

Luna files: scene_values_writer owns types.ixx/scene.cpp and native_scene_images
UT; common_scene_entry owns svg/mglyph pair, SVG catalog and native_svg_mglyph
owner UT; common_base_emission owns writer.cpp and native_svg_images UT;
common_scene_source_tests owns source-mglyph-scene.cjs, generated flat named
fixture arrays and native_svg_mglyph_source UT. Root owns CMake, old placement
retirement, current Scene inspection helper, review/build/tests/Git.

Independent Source witnesses use the pinned full38/NewCM profile, real MathML
compile and actual MathItem.display, root/mglyph BBoxes, ordered image attrs and
SVG. Use embedded data URIs only, without external network loads. Cover auto,
explicit and non-em sizes, positive/negative valign, signed/zero extents, empty
and escaped alt/href, images after preceding math, style/scale/placement, none
and local cache (actual useLocalID prefixes), legacy ASCII/supplementary index,
and separate invalid verification observations. Observe actual Source text
measurement requests/results for legacy font branches; native tests must use
that measurement contract, not hardcode case-specific geometry. Preserve Source
inputs/expected SVG independently of C++. Verify exact SVG before/after complete
request retirement, owned Image values and legacy wrapper parent/ownership, then
default86/canonical75/full math. Unknown/error observations remain explicit;
do not classify successful rendering of Source-produced merror as mglyph-image
output or silently omit legacy cases.

### 2026-10-21 Root lock: remaining table profile witnesses

Keep the existing CommonMtable, CommonMtr/CommonMlabeledtr and CommonMtd owners,
TableData, Common linebreaks and Common-to-Scene emission. Do not create another
table layout, SVG table wrapper, compatibility route, or expand mtable.cpp with
unrelated helpers. Existing direct-MathML8, SVG14 and numeric branch8 evidence
remains unchanged. New evidence is separate from those accepted fixtures.

Luna Source owner creates tools/math-port/source-table-profile.cjs and the flat
named arrays in tests/math/fixtures/table_profile_source.hpp. Use pinned full38
NewCM, real MathML compilation, actual MathItem.display and real none/local
FontCache prefixes. Observe after original processMath completes: root BBox,
every table's compiled-MML node index, BBox, rows/columns, hasLabels/isTop/pWidth,
and TableData H/D/W/NH/ND/L. Record actual unknown-text requests/results if any.
Keep Source errors explicit; do not alter inputs or expected SVG for C++.

Matrix: left/right labels with contrasting alignment, minlabelspacing and
data-width-includes-label; percentage child and nested table; ragged rows with
row/cell alignment overrides; horizontal stretchy operator in a constrained
column; text and operator column breaks using the established branch inputs.
Witness each representative in none and local modes. No rowspan/columnspan
implementation is added: pinned Common does not consume those attributes.
Source compilation must be inspected before inferring any normalization.

Fixture contract shared with the independent native-UT owner:
TableProfileBBox {double w,h,d,scale,rscale,L,R}; TableProfileTable
{size_t node_index; TableProfileBBox bbox; size_t rows,cols; bool has_labels,is_top;
double pwidth; span<double const> H,D,W,NH,ND; double label_width;};
TableProfileMeasurement {u16string_view text; string_view variant,font_family;
bool italic,bold; double w,h,d;}; TableProfileSourceCase
{string_view name,mathml; bool requested_display,display; double em,ex,width;
string_view font_cache; TableProfileBBox root_bbox; string_view error_name,
error_message,svg; span<string_view const> local_cache_prefixes;
span<TableProfileTable const> tables; span<TableProfileMeasurement const>
measurements; span<mathml_scene_test::Node const> nodes;
span<mathml_scene_test::Attribute const> attributes;};
constexpr sourceTableProfileCases[] stores the cases. Spans reference named
namespace-scope arrays only; raw strings are split below compiler limits.

Independent UT owner creates tests/math/native_table_profile_source.cpp, using
support/mathml_scene.hpp and SvgWrapperFactory::defaultNodes. Capture native
layout state after buildScene, compare every observed table and complete SVG,
then serialize again after document/job/measurement destruction. Measurement
must match actual Source requests, not invent per-case geometry. Numerical
comparisons use ordinary guarded bool expressions; do not put dereferences
behind Boost.UT overloaded logical operators. BBox is folia::math::BBox.

Root owns CMake, review, builds/native execution, mismatch triage, commits and
the checkpoint. Common table owner prepares a read-only Source responsibility
map while witnesses are built; production edits require a concrete observed
mismatch and stay in its responsible original method. Acceptance includes
independent Source default/write/no-op, final native layout/SVG in both modes
before/after request retirement, unchanged existing table regression evidence,
default86/canonical75 and full math. All builds remain single and parallel4.

### 2026-10-21 Root lock: canonical hosted none/local witnesses

Keep strict common_scene.inc, its 75 outcomes and default86 behavior. Hosted
Source is additional evidence, using the same canonical18+profile57 input
objects in exactly their existing order. Do not copy/shorten/reorder the corpus
or replace the two strict Lite event-API errors with fabricated expected SVG.

Source input owner extracts only the canonical75 definitions/default mapping
from common-scene-source.cjs into tools/math-port/common-scene-cases.cjs,
exporting canonicalSceneCases(profileCases). The strict generator imports it;
all appended witnesses, capture semantics, scope/dependency checks and default
fixture output stay unchanged. Prove strict default bytecheck/hash unchanged.
This file replaces the existing input table and has two real Source consumers.

Source-mode owner creates tools/math-port/source-canonical-scene-modes.cjs and
tests/math/fixtures/canonical_scene_modes_source.hpp. Parent generation runs
none/local in separate bounded child Node processes, each with heap512 MB,
cwd C:/Windows and one createReference. Each child evaluates canonical75 once
in order with one persistent input jax, original metrics/display/tagSide and
full38/NewCM configuration. Set fontCache once for that process; keep localID
default null and observe actual useLocalID prefixes, including nested calls.
Do not add a per-case cache reset: pinned Svg.typesetSvg already clears local
cache, calls useLocalID, emits and then clearLocalID. Keep failures explicit.

Reuse the accepted Source14 passive host behavior: the actual Lite receiver
registers its actual callback on a real EventTarget; retain receiver, target
and callback through full serialization, verify listener registration leaves
the receiver's serialization unchanged, then unregister. No dispatch or
replacement SVG/DOM behavior. This is Source host support only, never native
production event logic. Capture actual MathItem.display, initialAllowTexHTML
(the existing precedence), final root.getOuterBBox w/h/d after processMath,
complete SVG, errors, listener shape/DOM invariance and ordered cache prefixes.
Preserve raw diagnostics and input/session/provenance hashes in ignored JSON.

Fixture contract: CanonicalSceneMetrics {double width,height,depth};
CanonicalSceneMode {bool display,initial_allow_tex_html; CanonicalSceneMetrics
metrics; string_view error_name,error_message,svg;
span<string_view const> local_cache_prefixes; size_t listener_count;
bool listener_dom_unchanged;}; CanonicalSceneCase
{string_view name,tex,tag_side; bool requested_display; double em,ex,width;
array<CanonicalSceneMode,2> modes;}; modes[0]=none, modes[1]=local.
constexpr sourceCanonicalSceneCases[] has exactly75 records. Named static
arrays back every span; split raw strings below compiler literal limits.

Native helper owner extracts the existing InputOwner, Lite text measurement,
configuration/measurement services, NativeOutput and buildScene into test-only
support/tex_scene.hpp. Keep its registration order, package list, metrics,
request document, compile services and persistent/transient session semantics.
Use namespace tex_scene_test and TexSceneInput {string_view tex; bool display;
double em,ex,width; string_view tag_side;}. Header definitions are inline.
common_scene.cpp maps CommonSceneCase fields into this input through one thin
test adapter and keeps its strict/default selection, serialization, diagnostics
and stats. Do not duplicate a TeX pipeline or expand the large test body with
the hosted evaluator. All production files remain outside this extraction.

Native hosted owner creates tests/math/native_canonical_scene_source.cpp,
consuming that same helper and the new independent fixture. Use two fresh
InputOwners and persistent TeX sessions, one per cache mode; one document per
input. For each case compare completed Scene.metrics with Source outer w/h/d,
write with the actual mode/prefix list and compare full bytes after request
destruction. Count 75 layout pairs and150 SVG comparisons, with per-case first
byte diagnostics; require unique ordered inputs, zero Source/native errors and
all required listener invariance. Do not infer IDs, source measurements, actual
display or SVG bytes from native outcomes. Unknown-text measurement continues
through the existing real Lite adaptor, not a fixture-specific replacement.

Owners: common_base_emission owns shared JS inputs/strict generator refactor;
common_scene_source_tests owns hosted Source driver/fixture; common_rows_emission
owns test helper extraction/common_scene.cpp; common_scene_entry owns hosted UT.
Root owns CMake, review, builds/native runs, mismatch triage, commits/checkpoint.
Freeze all owners before one parallel4 build. Acceptance: strict Source unchanged
default/hash; hosted default/write/no-op, independent two-process Source150;
native75 layout pairs/150 SVG bytes; default86 and strict75 unchanged; full math.
This proves no browser dispatch, request192 lifetime run or Folia integration.

Canonical execution exposed two concrete assumptions to correct in their
original owners. CommonMspace::computeLineBBox must pass an absent authored
length through the existing empty-length/default-size rule; feeding the default
as a numeric length scales it twice (.2 becomes .04). Fix that method and add a
focused absent/empty/explicit/zero leading regression, retaining Source output.

The actual profile constructs its font before addExtension; extension instance
cacheID can therefore be absent even though the constructor's static cache-ID
map contains entries. The generator must not assign static IDs back to those
Source instances. Capture effective SVG suffix as instance.cacheID||'', permit
that empty suffix in FontRepository::resolveGlyph and generated aliases, and
keep missing variants/glyphs distinct from valid empty IDs. GlyphResource.font_id
denotes outline provenance, independently of the SVG cache suffix: generated
resources use face.name + "/" + originating variantName, and aliases share that
same immutable resource. This replaces the previous conflation, adds no runtime
metadata fallback and retains both font metrics/outlines and Source cache bytes.
common_base_emission owns the generator/resolver/focused-font UT; Root reviews
and regenerates the locked ignored provider output before the single build.

### Request lifetime observation lock

The next batch observes the accepted pipeline without changing its ownership.
MmlDocument still owns its node arena, CommonWrapperFactory still owns wrappers,
and Lite DOM parent links remain weak. No retire/reset registry or alternate
renderer is introduced. `buildScene` is already the Scene-only entry; full
render requests call the same entry and then SVG::write. Do not copy the compile
pipeline or add a second compile-only helper for this gate.

One small `util/instance_count.ixx` module may provide an allocation-free typed
InstanceCount<T> member. It counts actual construction/destruction (including
copies/moves), with relaxed atomic numeric totals and no addresses, callbacks,
resets or owned objects. Its actual consumers are the original MmlNode,
MmlDocument, CommonWrapper, DOMNode, DOMDocument and DOMWindow owners. Empty
counter members use no_unique_address; existing object semantics remain intact.
These are observation totals, not an allocator or lifetime-management service.

One focused request-lifetime UT reuses tests/math/support/tex_scene.hpp unchanged.
Warm a fixed semantic session across every input, retain a completed Scene and
an immutable SVG font catalog separately, then measure 192 and a longer 768
mixed Scene-only/full-SVG requests. Compare actual live counts at every request
boundary and verify construction/destruction totals advanced. A Windows Debug
test-support heap snapshot reads real CRT normal/client block counts and bytes;
compare warmed request boundaries before making UT assertions or logging.
This domain is explicit: it does not measure OS/GPU allocations or replace
object ownership review, and peak arena size is not a zero-growth proof.

Use actual unknown-text measurement exceptions, malformed TeX/merror, and a real
writer error followed by successful requests. Retained Scene SVG must survive
later requests and destruction of InputOwner, using only its separately retained
immutable font catalog. Persistent macro/label semantics and cancellation at the
future C ABI remain separate gates. Report request and cleanup time; RSS is only
supporting evidence. Any growth is diagnosed at its original owner, never hidden
by changing counters or adding manual releases.

Owners: common_base_emission implements the counter and three MML/Common owner
members; common_rows_emission owns only DOM base members; common_scene_entry
owns the focused UT and CRT-only test helper. Root reviews, registers sources,
formats, runs the single parallel4 build and tests, then commits small batches.

### ParseOptions request ownership lock (2026-10-21)

New evidence is the repeating CRT footprint and expired request borrows in
TeX/ParseOptions, not growing MML/Common/DOM populations. Preserve every accepted
parser, package, tag and layout algorithm. No end-of-compile clear/retire patch,
counter relaxation, copied compile pipeline or second ParseOptions implementation.
The original ParseOptions.ts and tex.ts C++ owners contain this change; add no
production file or generic Core request payload. MathInputJaxService::compile
and MathItem's existing mathematical service contract remain unchanged.

`ParseOptions` retains configuration/session collaborators: handlers, effective
options, StackItemFactory catalog, tag formatter product, BegingroupStack,
ColumnParser, math-style resolver, packageData and TextMacrosData configuration.
Store effective NodeCreators there as a finite typed value. Remove its MML
factory argument: a configured input jax must not retain a request factory.

In parse_options.ixx/.cpp, `ParseRequest(ParseOptions&, MmlFactory&)` is an
explicit noncopyable/nonmovable caller-owned RAII object. It owns NodeFactory,
TexEquationState equation, parsers, mathItem, texhtmlAdaptor, root, nodeLists,
error and the text parser bridge pointer texParser. All names retain their
Source roles. Its private binding saves the previous active ParseRequest and
restores it on scope exit; this only bounds a temporary borrow and never clears
or destroys another owner's data. Construct/validate factories before binding,
so constructor failure cannot leave an active pointer. Destructor unbinds before
normal member destruction. Request data never moves back into ParseOptions.

ParseOptions exposes request() const/nonconst (logic_error outside a scope),
hasRequest() noexcept, and setNodeCreators(NodeCreators const&). The setter merges
configured creators and updates an active factory if configuration changes
during a request. Existing NodeFactory creation algorithms remain unchanged;
consumers access `options.request().nodeFactory`. Other expression fields use
the same request() accessor; configuration-only fields remain on ParseOptions.
clear() retains its Source meaning at the start of an expression, operating on
the active request, not cleanup at request exit.

In tex_jax.ixx/.cpp, `TeXRequest(TeX&, MmlFactory&)` owns one ParseRequest plus an
optional text-macro ParseRequest, both bound for the caller's compile/typeset/
observation lifetime. It restores nested scopes through member RAII, including
partial constructor failure. TeX construction no longer receives a factory;
remove AbstractInputJax's unused persistent factory pointer and setMmlFactory.
compile checks that the active request factory matches its document, keeps
latex as a local snapshot after prefilters, and returns the final request.root;
remove persistent latex_/mathNode_. Configuration is still installed only once.
No implicit request, compatibility path or retained default request is allowed.
MathItem and CompileServices adaptor borrows are narrower than the frame: keep
the existing compile-scope restoration role for them, saving/restoring previous
borrows on every return/throw. This neither clears expression data nor changes
the lifetime of the caller-owned document; post-compile root/list observations
remain valid until the frame ends.

TextMacrosData keeps only textConf/ParseOptions configuration. Its texParser
borrow moves into the text ParseRequest; internalMath retains its existing
nested restoration and copyLists algorithms against that frame. Configure text
options without a document factory, and prepare the already bound text frame.
Tags' equation accesses move to request().equation; definitions/formatters and
session counters/labels remain persistent. reset(offset) must also work with no
active request: reset session semantics, and only reset expression fields when
a frame exists. Remove noexcept where a now request-dependent accessor may throw.

Caller scopes are explicit: the TeX scene helper creates TeXRequest after the
input jax exists and before compile, and it ends before MmlDocument destruction.
Single-document package fixtures own a frame beside their document/jax so all
existing in-document observations remain valid. The 64-fresh-document TeX test
creates one frame per loop and verifies absence of an active request afterwards.
Standalone parser fixtures own ParseRequest directly. Do not migrate inactive
Context tests/bindings or touch the thirteen protected dirty paths.

Acceptance: invalid/missing/foreign request rejection, nested binding restoration
and exception unwinding; existing parser/package Source tests unchanged in
meaning; canonical75/150 + default86 + strict73/two-errors unchanged; real
192+768 allocation/object/error/recovery/retained-Scene gate; one full math run
after final changes. Macro/label persistence and C ABI cancellation remain
separate gates. Root reviews all changes by responsibility and commits in small
packets; agents do not build, run native tests, modify CMake or mutate Git.

### Direct MathML limits witness lock (2026-10-21)

This batch extends evidence only, at the existing limits owners; it does not
replace accepted CommonMunder/CommonMover/CommonMunderover algorithms. Extend
`common-limits-source-witness.cjs` with a separate `--scene` mode, retaining the
existing numeric witness and fixture byte-for-byte. New data belongs in
`tests/math/fixtures/common-source/limits-mathml-scene.inc`; add one compact
table-driven test to the already registered `native_common_limits.cpp`.
No new production file/module, copied layout pipeline or CMake registration.

The fixed matrix is munder/mover/munderover times four conditions: inline
movable integral, display sum limits, explicit accents on x, and horizontally
stretchy accents over/under a wider row. Capture all twelve real MathML inputs
in none/local cache modes through the pinned full38 profile and NewCM. Record
actual compiled MML, root and selected limits-wrapper BBox, resolved local
prefixes and complete SVG; do not synthesize cache IDs or expected geometry.
The fixture owns named static arrays, not temporary spans or generated asserts.

Shared fixture contract: LimitsSceneBBox {w,h,d,scale,rscale,L,R};
LimitsSceneSourceCase {name,font_cache,display,em,ex,width,nodes,attributes,
root_bbox,limits_node_index,limits_bbox,local_cache_prefixes,svg}, with the
existing mathml_scene_test::Node/Attribute spans. The test reconstructs the
captured tree via the existing helper, invokes LayoutJob/defaultNodes once,
compares both BBoxes and final Scene metrics, and writes complete SVG before
and after document/job teardown. Each of 24 records must complete. Known-glyph
inputs reject unexpected text measurement; no result is skipped or replaced.

Source owner: common_mo_emission (generator and fixture only); native-test
owner: common_limits_emission (original test file only). Root reviews Source
write/default/no-op provenance and unchanged numeric fixture, then owns one
parallel4 build, focused limits, hosted canonical and the required final full.
Any mismatch is diagnosed at its original owner, never by adjusting expected.

### Application Renderer owner lock (2026-10-21)

Replace the existing inactive `math.ixx/.cpp` implementation at its original
owner, after accounting for its protected dirty diff. Do not introduce another
renderer or re-enable NativeRenderSession/Context bindings. The old dirty
callback/projection retirement hunks only served that withdrawn graph; their
request cleanup requirement is satisfied by typed document/frame/job RAII.
The resident-record diagnostic leaves with the object-layer diagnostic probe.
No unrelated protected file changes in this packet. Candidate files remain
ignored until Root reviews and deliberately promotes the original owner.

Preserve RenderOptions/RenderResult/RenderError and every render/renderScene/
toSvg UTF-8/UTF-16/stop-token signature and all C ABI signatures. Remove the
obsolete RendererDiagnosticsProbe and its Context/identity-count API, rather
than return invented zero counters. Native lifetime tests use actual owners.

Renderer::State owns the immutable NewCM bundle, Lite adaptor, configuration/
map/callback catalogs, HTML options, warning sink and one configured TeX input.
Register the same 38 packages in the pinned order once; no per-call registry.
Each call owns MmlDocument, TeXRequest and AbstractMathItem. A private narrowly
scoped MathOutputJaxService adapter delegates typeset/getBBox to the existing
LayoutJob/CommonOutputJax and SvgWrapperFactory catalog; it adds no mathematics.
Use the same Lite text/HTML measurement and linebreak wiring as the accepted
tex_scene helper, including the actual html parent for texhtml measurement.

Metrics are display, em (default16), ex=em*.5, width(default1280), scale1;
Common displayOverflow=linebreak, inline breaks enabled/forced. Completed Scene
outlives document/frame/job. render serializes that Scene exactly once with
SvgWriteOptions font_cache=none and the immutable NewCM catalog. renderScene
does no SVG serialization; toSvg delegates to render. No DOM SVG intermediate,
Node invocation, runtime object facade, release loop or duplicated layout.

Retain cancellation::Request/Scope and real existing checkpoints. Check owner
thread and session validity, then pre-cancellation before entering the pipeline;
pre-cancel leaves the session usable. In-flight cancellation makes subsequent
calls return SessionUnusable, including cancellation during serialization.
Other errors unwind typed scopes; malformed TeX remains successful merror SVG.
The C ABI continues enforcing finite positive metrics, UTF-16 units, opaque
result ownership, thread/cancel/deadline/status/OOM rules without STL crossing.
Root owns CMake promotion, console-runtime linkage and native execution.

Acceptance before WinUI switch: production Renderer matches independent Source
captures using its actual public options, covering the canonical75 TeX strings,
UTF encodings and metrics. Per-case options absent from RenderOptions (such as
tagSide) must be recorded as a distinct profile, not secretly set in the native
runner or added only for fixtures; existing canonical/strict evidence stays
unchanged. Also verify
Scene-only/result ownership after renderer teardown; errors and next request;
pre/in-flight cancel, deadline and wrong-thread cases; C ABI same SVG bytes,
result destruction and invalid arguments. Root then locks the worker/DLL wiring
separately, retaining priority/cache/generation behavior and the existing SVG
parser. No D2D pixel gate or math QuickJS fallback. WinUI remains unchanged
until this native application-facing gate passes.

The actual locked canonical75 records have empty tag_side, em16, ex8 and
widths 1280/100/80/120. Root inspected their generated evidence: every none-mode
record is directly representable by the current public API. Reuse that existing
independent fixture for all 75 UTF-8, 75 UTF-16 and 75 opaque-C render calls;
no new oracle or expanded RenderOptions is needed for this batch.

Root promotes math.ixx/.cpp into folia_math and builds the existing C ABI source
as shared target FoliaNativeMath linked to that library. On Windows, generate
the nine-function C export .def under the ignored build directory; do not
export C++/STL symbols or add Windows headers to portable math. FoliaMathTests
links the real DLL for its new opaque-ABI test; C++ public profile/cancellation
tests replace their existing test owners. All console probes retain the test
runtime and an adequate stack reserve. DLL build output stays in build/core.
The worker switch, deployment and WinUI build remain the subsequent packet.

### WinUI math worker integration lock (2026-10-21)

Prepare this packet as ignored candidates while the native full gate runs;
Root promotes only after that gate passes. MathJaxRenderer.h and its public
queue/cache/dependency API remain unchanged. In MathJaxRenderer.cpp replace only
the worker backend with the verified opaque C ABI. Create/use/destroy one
native renderer on its worker thread. Convert queued UTF-8 explicitly to owned
UTF-16 units; submit display/em/width with the existing two-second deadline.
A per-call cancellation handle outlives its stop_callback and result copying;
the worker stop token invokes only RequestStop across threads. Disabling stops
and joins as before; Clear preserves its generation/drop behavior.

Use RAII for all opaque handles; native statuses provide retry classification,
not substring guesses from human messages. Keep the first-transient-failure
retry policy, completion revisions/callbacks, priority queue bound256, cache
budget16MiB/count4096, keys, generation checks and callback-outside-lock flow.
Cancelled/unusable/OOM sessions are rebuilt on that worker. Native successful
SVG goes directly to existing ParseMathJaxSvgOutput; its data-mjx-error mapping
remains Formula. No SVG parser/normalizer edits, extra Scene ABI or D2D path.

Current non-vendored app search finds QuickJS consumers only in this worker and
its vcxproj build target. Remove those runtime calls, QuickJS link/build target
and deployment of unused JS math/font assets; retain licenses and development
Source oracle files. Do not touch Mermaid or its Rust normalizer dependency.
No fallback or runtime selector. The app links only the nine-function C import
library and deploys the verified DLL, with portable math headers as includes.

Reuse build_app.ps1 to configure/build FoliaNativeMath with parallel4 before
MSBuild (also capped4). Debug reuses build/core; Release uses build/core-release
so the existing test tree is not silently reconfigured. vcxproj has matching
build-dir defaults, early missing-library checks and an after-build DLL copy.
No additional application build scripts/projects, header-only std replacement, restored package commits
or extra build tree for the Debug path. Existing app stack reserve32MiB remains.
Root runs setup.ps1, supported WinUI build and relevant deterministic platform
tests, then verifies a real queue->C ABI->SVG-parser worker run including cache,
priority/dependencies, generation changes, formula errors and shutdown.

The reproducible test owner is tools/math-port/winui-math-worker-probe.cpp with
its PowerShell runner. It invokes the supported build_app.ps1, reuses actual
MSBuild worker/parser/module/PCH objects and the deployed DLL, and includes the
console failure runtime. It adds no alternate application implementation or
build project; generated compiler/linker files and logs stay under ignored build.

### Retire the old SVG Context graph (2026-10-21)

The native_document/render_session pair assembled the discarded Context service
graph. math.cpp now owns the application request orchestration using the real
TeXRequest/MathItem/LayoutJob owners. The old output_jax pair mixed mutable SVG
DOM construction with mathematical placement; Common output_jax now owns the
shared frame/line placement and the SVG writer owns serialization. The old
menclose_surface callbacks delegate to, or duplicate, the retained CommonMenclose
and notation algorithms. Keep those mathematical owners and Source witnesses.

scene_context_nodes and output_jax_bindings are mutable object projection only.
wrapper_classes, wrapper_constructors and wrapper_source_methods construct JS
identities and reflective dispatch; the typed CommonWrapperFactory/catalog and
original Common family classes own the actual products and algorithms. Remove
these nine module pairs together after their source consumers are accounted.

Legacy native observers that require this graph are not a second supported
pipeline. Retire their C++ harnesses and paired native-result drivers/comparers;
keep independent Source oracles, inputs, locks and fixtures. Their whole-object
field/call/descriptor traces do not become C++ API contracts. Retained mathematical
coverage remains in the typed family Source tests, full canonical SVG/metrics,
75-case Scene-route geometry, package corpora and actual lifetime tests. Do not
describe that as a replay of every old reflective trace field. Tracked draft
copies are also source consumers; directory names do not make them ignored.

### Scene responsibilities after retirement (2026-10-21)

The obsolete Scene family placement hierarchy and its mutable object adapters
have retired. The eight remaining Scene sources are builder.{ixx,cpp},
scene.{ixx,cpp}, types.ixx, html_types.ixx and html_snapshot.{ixx,cpp}.
They own value construction, immutable results and frozen foreign HTML content.
Mathematical measurement, stretching, placement and line breaking remain in
the original Common family owners. SVG serialization and resource planning
remain in the SVG writer/cache owners; future Direct2D consumes the same Scene.

CommonMrow::sceneDx owns the inferred-row origin exception observed in Source:
unbroken inferred rows skip the styled row shell; broken line stacks retain
the CommonWrapper style offset. It adds no request state or parallel layout
helper. The old family, script, menclose, glyph-painter and support files are
not alternative production implementations. Other inactive object-projection
files still require per-owner consumer review before retirement.

### Font direction and value boundaries (2026-10-21)

The original output/common/direction.ixx owns the closed StretchDirection enum,
matching the responsibility of Source Direction.ts. FontData re-exports that
module and keeps DelimiterRecipe; Common wrappers share the same type. The
move preserves enum values and all stretch/layout algorithms. No string-valued
DIRECTION/V/H compatibility aliases or second direction type remain.

FontRepository/FontGlyph and SvgFontResources publish immutable final font data.
The existing NewCM module owner builds all pinned shards before publication;
Source module/event counts are provenance, while direct glyph/remap/resource
counts come from the actual native values. Old mutable FontDataClass/tuple,
constructor/prototype, ListPatch and runtime JS-loader bindings are retired.
The Source data generators/locks, final typed payload and independent oracles
remain; no runtime loader/cache/promise API is reintroduced to satisfy old probes.
