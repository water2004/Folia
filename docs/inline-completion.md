# Inline completion

## Contract

Folia builds, configures and runs independently. Reused protocol adapter source
is vendored with its license and source revision; no Fairy installation, process,
configuration, key store or absolute source path is a dependency.

The user configures a protocol, base URL, model and key in Folia. The transport
adds `/chat/completions`, `/responses` or `/messages` to that base path; a host-only
base uses `/v1`. Trailing slashes are normalized and query parameters are retained.
Path resolution and authentication depend only on the selected API protocol
and configured base URL. No provider domains or mounts are remapped.
The settings field is `completion.baseUrl`; there is no legacy endpoint migration.
Completion is opt-in. Keys use the Windows credential store, separate from portable settings.
Only bounded context from the active document is sent to the configured service.

Settings expose context size (1–64 KB, 1000 UTF-8 bytes per KB), maximum output
tokens (16–4096), suggestion frequency and reasoning effort. Defaults are 7 KB,
128 tokens, Balanced frequency and service-default effort. Conservative,
Balanced and Proactive select stable instruction variants; they do not change
the model's reasoning parameter or guarantee a suggestion.

The model field is an editable native dropdown: users can fetch and select a
model or type an ID directly, including IDs absent from the list. **Fetch models**
reads `GET /models` under the configured base URL without a prior model selection
and without document content. Chat Completions and Responses use Bearer headers;
Messages uses the protocol's API-key/version headers. Custom base paths and query
parameters are retained for every host. No generation requests detect capabilities.

The reasoning field is also an editable native dropdown. Advertised capability
values are suggestions, not an input restriction: the user may type a value when
metadata is missing or incomplete. Typed values are not inserted into the fetched
candidate list. Model IDs and effort values saved by the user remain editable,
and a refresh preserves the current inputs even when absent from returned metadata.

The OpenAI Models schema supplies model IDs but no reasoning-level discovery
field. Messages reads the standard `capabilities.effort` object: both the feature
and each offered level must explicitly declare support. Private
`effort.supported_levels` fields are not consumed. No model-name rules, provider
tables, or preset effort lists populate the picker. Missing capabilities show an
explicit message inviting manual entry; they are not reported as model-level
lack of support. Manual effort values use the selected protocol's standard field:
`reasoning_effort`, `reasoning.effort`, or `output_config.effort`. The adapter checks
only token syntax (up to 32 ASCII letters, digits, underscores or hyphens), leaving
the vocabulary to the service. Blank or the service-default item omits the field.

Selecting a model updates capabilities locally. Refresh preserves its text,
including manually entered IDs, without selecting a different model. Discovery
is bounded to 1 MB per page, 16 pages and 4096 unique model IDs. Messages follows
its standard `has_more`/`last_id` pagination using encoded `after_id` cursors and
rejects repeated cursors. OpenAI's standard Models response is a single list.
Changing the base/protocol clears stale choices; leaving the page or actually
changing credentials cancels pending work and late results are ignored.

See the [OpenAI Models schema](https://developers.openai.com/api/reference/resources/models/methods/list)
and [Anthropic Models schema](https://platform.claude.com/docs/en/api/models/list).

Budget fields are ordinary integer text inputs. Closed setting choices do not
use the mouse wheel to change selection; the page keeps scrolling. Open lists
retain native scrolling and keyboard selection.

The status bar runs a native progress ring only during a request. Completion,
an empty suggestion, token-limit exhaustion and request errors have distinct
states. Cancellation, focus loss and document changes stop that indicator;
late responses cannot restore it. Empty token-limited responses are not cached.

Tab accepts the visible AI proposal, Ctrl+Right accepts its next word, and Escape
dismisses it. Automatic TeX lists and snippet sessions do not block AI requests,
delivery or preview. When both suggestions are visible, arrows navigate the TeX
list and Enter inserts its selected command; Tab and Ctrl+Right accept AI.
Without an AI proposal, Tab accepts the TeX candidate or advances a snippet.
Shift+Tab retains backward snippet navigation. Escape dismisses AI first, then
the TeX list, then the snippet session on subsequent presses.
One deterministic key policy assigns each gesture to the TeX list, snippet or
AI proposal, taking modifiers and IME composition into account. Shift+Tab never
accepts a TeX item. A snippet releases Tab as soon as it reaches its final `$0`.
The TeX list's hint follows the visible proposal, so its acceptance instructions
match the action Tab will perform. Accepting AI closes the old command list;
accepting a TeX command cancels obsolete AI results through the ordinary edit flow.
Typing over selected text, including a snippet argument, schedules predictions
just like typing at a caret. An AI edit confined to the active argument retains
snippet navigation; an edit crossing that argument retires the outdated tab stops.
Partial acceptance and matching typed prefixes consume the existing proposal;
they do not issue a request per word. New proposals follow ordinary idle typing.
Partial acceptance uses ICU word segmentation, including Chinese words, emoji
sequences and combining marks.

## Responsibilities

- `src/completion`: portable request context, bounded cache, proposal state,
  acceptance boundaries and prompt policy. No Windows or editor mutations.
- Vendored protocol adapters: request/response/SSE mapping for Chat Completions,
  Responses and Anthropic Messages; no tools, agent loop or session history.
- Windows transport: asynchronous worker execution, streamed HTTP, cancellation,
  response limits and credential storage. No renderer or document access.
- Application completion controller: immutable document snapshots, debounce,
  request generations, settings and UI-thread delivery. One active request,
  no backlog of obsolete cursor locations.
- Display preparation: plain muted generated text inserted into the existing
  DirectWrite flow. It wraps with the text, without Markdown styling or math
  rendering. It is absent from CST, selection source, copy, save and export.
- Acceptance: existing source/tree editing transaction, with exact undo/redo.
  Display offsets never become a second selection coordinate.

## Edit proposals

The client marks a short editable tail at the caret (up to 32 Unicode scalars,
ending at the current source line). These input markers identify existing text
to the right of the caret; their spellings do not occur in the captured document.
The reply explicitly selects one operation:

- `<insert>literal source</insert>` inserts new text without consuming any right
  context. Ordinary continuations do not have to repeat existing closing text.
- `<replace>literal source</replace>` replaces the marked tail, retaining any
  existing characters that should remain. This supports changing or passing
  through existing text as part of one proposal.

Empty output is abstention. An empty insert leaves the source unchanged; an empty
replace deletes the marked tail. Missing framing, other operations and
out-of-budget output are rejected. There is no JSON or previous reply-format
fallback, model-owned document offset, or guessed operation.

Instructions describe an inline editor operation, rather than asking for a
completed standalone document. The model should extend the thought and structure
already open at the caret, retain needed right-hand text, and emit no presentation
wrappers. These are source completion rules shared by all content; they do not
classify the editing language or prescribe particular syntax delimiters.

TeX backslashes, indentation and leading newlines are literal payload. Only
outer framing whitespace is ignored; payload CRLF/CR is normalized to LF.
For replacement operations, the client obtains expected source from its captured
window and trims copied common suffixes out of the edit, keeping the caret before
untouched right context. Insert operations never infer an overlap or delete
existing text. Generated text is limited to 512 Unicode scalars without a separate
four-line restriction. The model is not told whether the document is Markdown,
code, or TeX; it receives available source and infers its environment.

A bounded diff identifies the existing characters to reuse, including paired
brackets between newly supplied arguments. The preview shows only insertions
as gray text; reused characters retain their source mapping, and removed text
is gray with a strikethrough. Nothing changes in the document until acceptance.
Word acceptance divides the same edit and accounts for consumed existing text,
so later acceptance does not duplicate delimiters.

The editor supplies the range to the existing semantic Markdown paste command
without first changing selection. Expected source and document generation are
checked before acceptance. Replacement, block structure changes and the caret
are recorded in one transaction. The source view uses its source-patch history.
Thus Markdown fences still create blocks and undo restores the original caret,
not a temporary replacement selection.

## Prompt configuration

The instruction text, frequency policies and optional examples are runtime data
in [Assets/completion.json](../src/app-winui/Assets/completion.json), not compiled
strings. `completion.prompt` in the user's `settings.json` can contain a custom
object with the same shape. An absent override uses the packaged file; saving
unrelated settings does not copy the defaults into user settings, so built-in
updates continue to apply. Explicit custom prompts are preserved on save and
upgrade. Restart Folia after manually editing configuration files.

The default guidance completes the nearest unfinished unit using its existing
notation and layout, then stops. Block bodies begin after their opening line;
closing boundaries already present in the source are retained. The output
budget is a ceiling, not a requested completion length. Live evaluations must
inspect the spliced source, including leading whitespace, matching boundaries
and the stopping point, rather than counting valid operation tags alone.

`instructions` supports `${edit_start}`, `${edit_end}`, `${policy}` and
`${max_characters}`. Expansion is nonrecursive and never processes document
source. `policies` contains `conservative`, `balanced` and `proactive` text.
`documentPrefix` labels the source. Each optional example supplies `before`,
`editable`, `after` and the complete operation `response`; examples become
ordinary user/assistant exchanges before the current document. Their order and
contents stay fixed between requests, preserving the shared prompt prefix.

`regionName` names the input range marker (up to 24 ASCII letters, digits or
underscores, starting with a letter); its spelling is adjusted if it occurs in document content.
`beforeEdit` and `afterEdit` separate the source before the cursor, the existing
right-hand region, and the remaining source. They allow source fragments to be
labeled clearly without inserting editing metadata into an unfinished source
construct. These labels describe source positions, not Markdown/code/math modes.

Configuration is read with a size bound during settings/controller preparation,
not during scrolling or each request. Missing or invalid prompt configuration
reports an AI configuration error without resetting unrelated settings or
falling back to compiled instructions. The portable completion module only
validates and expands the explicitly supplied configuration; filesystem and JSON
loading belong to application settings.

## Cost and lifetime

Use a small bounded prefix/suffix window and preceding block context, gathered
from block-local source without a full-document serialization. Stable instructions
and stable context precede changing cursor text. Do not include timestamps or
request IDs in prompts. The transport preserves provider usage, including cache counts only when supplied
by that provider. Local context hits reuse proposals without a provider request.
Document context is presented in reading order in one user message: preceding
source, current prefix, marked editable tail, protected suffix, following source.
The system instructions and unchanged preceding text remain ahead of the active
region. Long lines and prefix starts retain fixed 256-codepoint anchors rather
than shifting on every keystroke. Following text is kept after the active region
for coherent context; it is not reordered solely to lengthen a cache prefix.
This preserves useful prefix reuse without promising a provider cache ratio.

Neighbor collection consumes the byte budget instead of stopping after three
owners/lines. Source-mode lines use single line breaks; block-local source uses
block separators. It uses the maintained editable index and views, with bounded
work, no full document traversal, parse or serialization.

The context budget is divided roughly 4:1:1:1 between prefix, nearby suffix,
preceding blocks and following blocks. Defaults are 7 KB of UTF-8 context and 128 output tokens; suggestions
remain limited to 512 codepoints. Output tokens may include
model reasoning, so an exhausted limit need not produce insertable text. Document views
are bounded before copying/encoding. The 10 MiB case exercises start/middle/end
and Unicode expansion. These byte caps are not a model-specific token estimate.

Requests debounce for 450 ms and start at least 1.5 s apart. Network reads use
a 15 s inactivity timeout; each new chunk, including reasoning or heartbeat
data, starts a fresh wait. There is no total generation deadline. Reads consume
available data promptly, and a protocol terminal event completes the request
without waiting for connection closure. Errors do not retry automatically. Local LRU holds at most
32 entries / 256 KiB of payload, only in memory.
Cancellation, document changes, focus loss and tab/window changes retire request
generations. Late results cannot overwrite the current proposal. Network waits
and thread joins do not run on the UI thread.

Accepted Markdown uses the ordinary semantic paste transaction with an explicit
replacement range. Empty code
blocks expose a body caret below their generated language header. When a fence
at EOF has no opening line break, the first body insertion creates that source
boundary in the same reversible edit; typing never extends the language name.

## Verification

Deterministic tests cover debounce/request counts, stale delivery, cancellation,
local cache reuse, typed-through proposals, word/full acceptance, Unicode,
reversed selection, undo/redo and zero full-document work. Real display tests
cover wrapping, source hit mapping, nested containers, source view, theme colors
and absence from save/export. Protocol fixtures cover all three protocols and fragmented UTF-8/SSE. Native
loopback HTTP tests cover fragmented responses, cancellation and service errors
with explicit fixture credentials, without reading saved keys or calling a model.
Live model quality and latency require a user-configured service and are reported
separately from fixture results.

Context reference: [GitHub Copilot input processing](https://docs.github.com/en/copilot/responsible-use/inline-suggestions).

Edit reference: [VS Code inline completion ranges](https://code.visualstudio.com/api/references/vscode-api#InlineCompletionItem).

Streaming reference: [WinHTTP available-data reads](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpquerydataavailable).

Interaction reference: [VS Code inline suggestions](https://code.visualstudio.com/docs/editing/ai-powered-suggestions).
