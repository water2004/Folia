# P19a root local acceptance — complete FunctionList

> Historical evidence, superseded on 2026-10-20. The object/Promise-based
> FunctionList implementation and its test generator are retired. Current
> synchronous utility behavior is checked by `util-function-list-source.cjs`,
> `tests/math/util/function_list.cpp`, and `tests/math/util/prioritized_list.cpp`.
> The claims below do not describe the current implementation or parser wiring.

Root accepts all four members of pinned MathJax 4.1.3 FunctionList.ts, SHA256
d4106c42268e0d7cb3549f8ffdbeb5bab87eae6daffae7314d5e724045a6918f.
Actual CJS SHA256 is
571f57801b863b2d9f01a66dc72f2836b7fd06a1a48a3fe0057e4ec3d51a7278.
Root personally reviewed both complete source bodies and the native diff;
Luna owned only the function body/report under function-list-contract.md.
This is one complete utility file, not TeX/Common/SVG or Folia acceptance.

The four-member AST ledger is p19a-function-list-members.json. The source
constructor maps to post-allocation initializeFunctions so native virtual
addList dispatch sees a complete derived object. Definitions use live typed
vectors/tuples, original callable identity and source numeric priorities.
execute uses inherited array-capturing iteration; asyncExecute captures the
same array before output creation and preserves the separate then/catch links.
Both retain actual receiver records and snapshot rest argument slots once.

Root function-list-oracle.cjs runs the actual hash-pinned CJS, including 224
synchronous mutation/result cases, 126 asynchronous result/action cases,
11 external mutations during a pending callback, seven constructor cases,
ten native-Promise reference sequences, one source priority iterator capture
case and 24 invalid registration/storage cases: **403 source cases/assertions**.
The fixture retains 370 source scalar/array/record values. Async snapshots use
Promise.resolve await boundaries in the source and queued FIFO marker jobs in
the native host, rather than incorrectly assuming one reaction per await.
Outcome observation is itself a then reaction in both systems. Comparisons
include full call/receiver/priority/argument traces, current and original arrays,
record identities, mutations, partial errors and every observed queue phase.
Only bool false stops; missing/null/zero/NaN/strings/arrays/records/symbols,
thenables and foreign-realm promises continue. Fulfilled false is ignored.
Initial errors and continuation/input rejection preserve distinct phase order.

Six separate native host tests add **29 assertions** for list/argument ownership
through asynchronous suspension and release after completion; original raw
shared reason identity and ordinary exception payload/captured-handle forwarding;
mutual pending adoption/session teardown; iterator survival after owner death;
independent concurrent runs; and null/uninitialized callable/list handles.
The session owns an arena of promise records; handles and async frames hold
weak queue references. Internal adoption/reaction edges cannot retain an arena
after session teardown, including mutual pending adoption. Settled values stay
owned until session teardown, while settled reactions/captures are released.
This is session-local native ownership, not a browser garbage collector.

MSVC can copy a C++ exception when current_exception captures a rethrow. Native
ordinary errors preserve type/payload and the resulting exception_ptr identity
through propagation. Caller-created exception_ptr address equality across
rethrow/capture is outside the C++ guarantee. ThrownValue preserves arbitrary
original shared reason identity. Arbitrary JS iterables/proxies/accessors,
non-number priorities, general thenable assimilation and arbitrary JS subclass
overrides are outside the frozen typed callable/array/Promise host. No interpreter
or generic substitute TeX parser is introduced. Native Promise host files add
no MathJax source count. Worker thread creation/queue pumping and event ingress
remain future integration duties; a caller must own its active session.

PrioritizedList's previously accepted iterator now captures the original
StorageRef instead of following an owner's replacement array. Add/remove,
stable priorities and shared mutable default semantics remain unchanged.
The old 1,223-check primitive fixture reproduces byte-for-byte; new source
cases independently verify old-array mutations/rebinding and native lifetime.
PrioritizedList's accepted source count does not increase for this correction.

Verification: function-list-oracle.cjs --check and primitive-oracle.cjs --check
pass. Focused FunctionList tests and `cmd /c build_test.bat 2>&1` pass, including
all existing core suites (ignored log build/p19a-full-core.log).
function-list-mutation-test.cjs built and detected all **26** semantic mutants
with exit 1 assertion failures; no compile failure or OS crash counted as a
detection. They cover constructor/virtual/empty dispatch, definition order/live
length/default/priority, source iterator capture, null error category, receiver
and rest slots, exact false, async indexing/capture/fulfillment/rejection links,
queued jobs, catch/handler returns, first settlement and adoption scheduling.
Original sources and expected fixtures were byte guarded and unchanged.
Results are p19a-function-list-mutations.json; all products/logs remain ignored.

After this local acceptance the source ledger is 51/246, with 195 source files
still required. G0–G7 and the Folia production switch remain closed.
