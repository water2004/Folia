# Historical P19a — superseded FunctionList object model

This document records the withdrawn Context/Property/FunctionRef design. It is
not the current implementation contract and must not guide new work. The
current `util/function_list.ixx` owns the synchronous, compile-time callback
list under the original MathJax class responsibility. Its tests are in
`tests/math/util/function_list.cpp`; configuration-specific composition belongs to
`input/tex/configuration` and `tests/math/input/tex/configuration.cpp`. Progress and
remaining integration limits are recorded in `HANDOFF.md`.

The text below is historical evidence only.

Root read complete pinned FunctionList.ts/CJS, PrioritizedList.ts/CJS and
existing native priority/Property/parse seams before freezing this packet.
FunctionList has four source members; all must be implemented, including async.
Source SHA256 d4106c42268e0d7cb3549f8ffdbeb5bab87eae6daffae7314d5e724045a6918f.

Root owns all .ixx declarations, storage correction, callable/promise services,
module/build lists, independent source oracles/fixtures/mutations, ledger/scope,
review and acceptance. Luna math_layout owns only util/function_list.cpp and
evidence/p19a-function-list-methods.md. No other edits, builds/tests, commits,
delegation or integration. Raise interface gaps instead of inventing APIs.

## Existing storage boundary

PrioritizedList now has one shared Storage array of shared record objects.
Its iterators capture that array at begin and retain yielded record identities.
Protected itemStorage() exposes the same handle to the subclass; there is no
copied async list. Changing the current handle cannot redirect old iterators
or an async run. Add/remove retain the already accepted numeric priority,
stable insertion, last identity removal and mutable DEFAULTPRIORITY behavior.
Root independently verifies this correction against source including lifetime,
rebinding and mutations; existing expectations are not regenerated to fit code.

## Native callable/array construction seam

FunctionValue retains either raw non-callable Property or actual FunctionRef.
Function::call receives the actual prioritized item record and span of immutable
argument slots. Slots are std::any with original native values/shared handles;
raw JS scalar values use Property, and native bool returns are equivalent scalar
bools. Non-callable invocation throws TypeError at execution, not registration.
functions::invoke captures the callable then calls it on the same record;
functions::isFalse stops only exact native/Property false; functions::promise
recognizes a non-null native PromiseRef belonging to the injected queue realm.
Other objects, including foreign promises/thenables, remain ordinary results.

FunctionDefinitionsRef is the source's nullable/live definition-array handle.
Each definition is a FunctionValue or live FunctionTupleRef. Tuple contains
item (Missing for an empty tuple), optional numeric priority (omitted defaults
at that add site), and extra ignored slots. This follows the existing typed
numeric PrioritizedList seam and TS [function,number] contract; arbitrary JS
non-array iterables/proxies and non-number priorities are not a new erased VM
surface. List/tuple/callable identity and changes during callbacks are retained.

initializeFunctions maps the source constructor after native allocation so
virtual addList overrides see the actual object (C++ base constructors cannot
provide JS constructor-time virtual dispatch). makeFunctionList<T> owns the
object before that phase. Null list skips addList; present empty list calls it.
addList reads current vector length/definition on each turn, then current tuple
item/priority, calling virtual add with explicit priority only when present;
otherwise calls add(item) to read live default at that point. Null list/tuple
access throws; no early callable validation or list reversal/deduplication.

execute snapshots incoming rest argument slots once at entry, iterates the same
captured source storage and invokes each current record with those slots.
Only exact false returns false; all other results continue, including promises.
Exceptions propagate with exact native identity/partial state.

## Async body and generic host

asyncExecute receives the native session QueueRef explicitly (source realm's
Promise/job service), copies rest argument slots once, captures itemStorage
BEFORE creating its output promise, and starts execution synchronously.
State index begins -1; preincrement then read CURRENT length of captured array.
Each record is held through its callback. Native state owns only array, slots,
index, weak queue and output; callback continuations own state. Lock the queue
only for each synchronous resume; a missing session raises TypeError through
functions::typeError. No self-owning recursive
std::function/state cycle or borrowed FunctionList this after return.

Promise result uses the literal source two-step chain:
`input.then(execute).catch(fail)`. Do not combine direct rejection handling on
input: source rejection propagates through the intermediate promise before
catch. Fulfilled values (including false) are ignored; execute resumes at the
next index. Direct false resolves output false; exhaustion resolves true.
Initial callback exceptions reject output synchronously; continuation exceptions
are captured by then and passed through catch. Keep partial callbacks/list/data.

Root promise.ixx/cpp supplies a session-local deterministic FIFO job queue and
native Promise state, then/catch, first-resolution wins, fulfillment/rejection
and promise adoption. It executes no JS/QuickJS and creates no threads/platform
objects. Then reactions are queued even for already settled promises. Handler
exceptions reject the intermediate with currentReason(): ThrownValue retains
its arbitrary original any reason; other exceptions retain exception_ptr.
Ordinary C++ exceptions retain the captured exception_ptr and their type/payload;
MSVC may copy the thrown exception during current_exception capture. Identity
across native propagation is the captured handle; caller-created exception_ptr
address equality across a rethrow/capture is not a C++ guarantee. Raw shared
reason identity uses ThrownValue and is preserved from the original callback.
Promise and async-frame queue ownership is weak to avoid queue/job/frame cycles;
the native session/caller owns the queue while active. Jobs are pumped on that session
thread; thread creation, event-loop pumping and concurrent completion ingress
belong to the eventual worker integration. Callback scopes must not capture
owning session references that form cycles. No inline reaction scheduling.
The queue owns promise state records; PromiseRef is an identity-bearing native
handle with weak queue plus record index. Internal reaction/adoption edges never
own their arena, including mutual pending adoption. Session destruction releases
all state/jobs/captures; surviving handles raise TypeError on state access after
the session expires. This is the established session-local native handle boundary,
not garbage collection or a browser Promise implementation. Reactions/callback
captures are cleared on settlement; no completed callback graph is retained.

## Root acceptance

Execute actual pinned CJS, not a model of its algorithm. Compare complete four
members, callback receiver/item/priority identity, typed definition defaults,
stable priorities/duplicates/NaN/infinity, direct false versus every other raw
result, non-callable registration/invocation, constructor virtual phase, input
array and active item/storage mutation/rebinding. Async observations compare
initial synchronous work, controlled Promise resolution/rejection, each queued
reaction phase, already-settled/pending/rejected promises, false fulfillments,
continuation errors, foreign promises/thenables, captured args/list lifetime and
independent/reentrant runs. Root separately verifies the generic promise host,
including adoption, exception/reason identity, first settlement and queue order.
Require compiling semantic mutants and existing/full core regression before
scope acceptance. This foundation does not pass TeX/Common/SVG/Folia gates.
