# Inline completion

## Contract

Folia builds, configures and runs independently. Reused protocol adapter source
is vendored with its license and source revision; no Fairy installation, process,
configuration, key store or absolute source path is a dependency.

The user configures a protocol, base URL, model and key in Folia. The transport
adds `/chat/completions`, `/responses` or `/messages` to that base path; a host-only
base uses `/v1`. Trailing slashes are normalized and query parameters are retained.
The settings field is `completion.baseUrl`; there is no legacy endpoint migration.
Completion is opt-in. Keys use the Windows credential store, separate from portable settings.
Only bounded context from the active document is sent to the configured service.

Settings expose context size (1–64 KB, 1000 UTF-8 bytes per KB), maximum output
tokens (16–4096), suggestion frequency and reasoning effort. Defaults are 7 KB,
128 tokens, Balanced frequency and service-default effort. Conservative,
Balanced and Proactive select stable instruction variants; they do not change
the model's reasoning parameter or guarantee a suggestion. Clicking Fetch reads
`GET /models` under the configured base path and selects that model's
`effort.supported_levels` metadata. No reasoning levels are preset or guessed;
an existing saved choice remains visible until refreshed. Only upstream choices
supported by the protocol adapter are offered. Missing metadata leaves service
default, which omits the effort parameter. Fetching is asynchronous, bounded and
cancelled on model/base/protocol changes or page removal; stale results are ignored.
The metadata schema is documented by the [upstream Models API](https://api-docs.deepseek.com/api/list-models/).

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

The model returns a JSON object with `oldText` and `newText`. `oldText` is an
exact prefix of the following block-local source, at most 128 Unicode scalars;
an empty string denotes insertion. `newText` replaces that text. The response
contains no document offsets. Invalid text, bounds or schema are rejected, and
an empty edit denotes abstention. Generated text is bounded to 512 scalars and
four lines; it is never silently truncated into a different edit.

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

## Cost and lifetime

Use a small bounded prefix/suffix window and preceding block context, gathered
from block-local source without a full-document serialization. Stable instructions
and stable context precede changing cursor text. Do not include timestamps or
request IDs in prompts. The transport preserves provider usage, including cache counts only when supplied
by that provider. Local context hits reuse proposals without a provider request.
Stable preceding/following blocks and the stable portions of the current block
are placed before the changing editing region in the request. The current line
stays in that final region; long lines and bounded prefix starts use fixed
256-codepoint anchors instead of sliding on every keystroke. Moving the caret
or replacing text within a short line therefore preserves the entire stable
context message. This helps provider caching where supported, without promising
cache hits or a particular server-side ratio.

The context budget is divided roughly 4:1:1:1 between prefix, nearby suffix,
preceding blocks and following blocks. Defaults are 7 KB of UTF-8 context and 128 output tokens; suggestions
remain limited to 512 codepoints and four lines. Output tokens may include
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
