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

Tab accepts the current proposal, Ctrl+Right accepts its next word, and Escape
dismisses it. Existing completion lists and snippet tab stops retain priority.
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

## Cost and lifetime

Use a small bounded prefix/suffix window and preceding block context, gathered
from block-local source without a full-document serialization. Stable instructions
and stable context precede changing cursor text. Do not include timestamps or
request IDs in prompts. The transport preserves provider usage, including cache counts only when supplied
by that provider. Local context hits reuse proposals without a provider request.
In long paragraphs the prefix start advances in 256-codepoint chunks; nearby
keystrokes append to a stable prefix while staying inside the byte budget. This
helps provider caching where supported, without promising cache hits.

Defaults: 4 KiB prefix + 1 KiB suffix + 2 KiB preceding blocks (UTF-8 bytes),
128 output tokens, 512 proposal codepoints, at most four lines. Document views
are bounded before copying/encoding. The 10 MiB case exercises start/middle/end
and Unicode expansion. These byte caps are not a model-specific token estimate.

Requests debounce for 450 ms, start at least 1.5 s apart, and use a 15 s total
network deadline. Errors do not retry automatically. Local LRU holds at most
32 entries / 256 KiB of payload, only in memory.
Cancellation, document changes, focus loss and tab/window changes retire request
generations. Late results cannot overwrite the current proposal. Network waits
and thread joins do not run on the UI thread.

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

Interaction reference: [VS Code inline suggestions](https://code.visualstudio.com/docs/editing/ai-powered-suggestions).
