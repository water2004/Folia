# Agent chat

Open **Conversations** from the left rail. Select **+** to start a chat in a
kept tab. Single-clicking an existing conversation reuses the active group's
preview tab, shared with the file browser. Double-click a session to keep it.
Typing a draft, adding an image, changing its settings, or sending a message
also keeps the tab. Each conversation has one tab across the application's
windows. Closing the tab preserves its messages.
Right-click a conversation to rename it or delete its local history.

Open **Settings → AI providers** and select or add a provider. In its editor,
configure the connection and add the models you want to use. The model page
holds its upstream ID, display name, token limits, available and default
reasoning levels, and model capabilities. Save the provider to publish its
pending changes.
Chat and inline prediction select from the same model catalog. Open the model
chip in chat to choose a reasoning level supported by the current model. Select
the model name to switch to another enabled model. The context indicator shows
the input tokens reported for the latest request to that model and its configured
window. Hover over the indicator for exact counts.

Press Enter to send or Shift+Enter for a new line. The + menu attaches images or inserts a reference. Type `@` to select an open
Markdown document or a path inside the selected workspace. Use the arrow keys
to choose a suggestion and Tab or Enter to insert it. Selecting an open tab grants
access to that specific tab for this turn. Paste a screenshot or copied image
into a chat with a vision-enabled model. You can remove individual thumbnails
before sending. A message accepts up to four images.

The access menu controls open tabs, the workspace folder and hosted search.
A model can request a document outline, search for text or read successive
ranges. Tab reads use a snapshot of the requested document, including unsaved
edits. Workspace reads use UTF-8 files and paths relative to the selected folder;
Folia verifies the resolved path using the handle used for the read.

The conversation opens on the four most recent turns. The page controls retrieve
earlier turns from disk. While a reply is streaming, scrolling up holds your
place. Select the down arrow to return to the latest output. The toolbar
can open a document alongside the conversation. Selecting a document citation
opens or focuses that document at the cited location.

Markdown is rendered as the response arrives. Between text passages, one line
shows the current activity and then summarizes that group. Expand it for the
individual actions and their details. A single action opens its details directly.
A reasoning caption uses an explicit provider summary when available. Other
thinking text stays in the disclosure. Folia does not request an extra summary
for the activity line. The effort track and the send and stop buttons follow `colors.shellAccent` in the active theme.
Right-click a message to copy it. A connection failure appears below the affected
reply. Transient failures before output can retry automatically. After partial
output, Retry starts a new attempt and preserves the interrupted reply.
The configured maximum output applies independently to each model request.

Type `/` for conversation commands, including model selection and access settings.
After the first successful reply, Folia requests a short title from the configured
model. Renaming the conversation takes precedence over the generated title.

Hosted web search requires permission on the provider connection, a model that
supports it, and opt-in in the conversation. Responses uses the hosted
`web_search` tool. Anthropic Messages uses `web_search_20250305` and handles a
hosted `pause_turn`. Folia reads declared search support from Messages model
metadata when available. Search availability can also depend on the upstream
account and request options. Chat Completions search uses dedicated models and
a separate request option. Folia's per-conversation search control is available
through Responses and Messages. Hosted search data and citations come from the
configured provider. Model replies are displayed
with Folia's native Markdown renderer. The Sources menu opens URLs on explicit
selection, and automatic image fetching is disabled in the transcript.

## Implementation

- `src/ai/providers` owns the protocol formats and stream termination checks.
  `src/platform/windows/ai` owns HTTPS, model endpoint resolution, and Credential
  Manager access. Inline prediction shares these layers.
- `src/ai/agent` owns bounded turns, function call/result pairing, replay for the
  current turn, and context compaction. It accepts a provider request callback
  and a registry of tools carrying capabilities granted by the host.
- `src/app-winui/agent` provides the composer, native Markdown transcript,
  session repository, and local tool implementations. Workspace shell code owns
  sidebar and tab placement.
- `src/app-winui/settings/storage/ModelCatalog` resolves the shared model
  records for chat and inline prediction.
- The `ai-models` folder in Folia's data area contains provider connections and
  configured model records. The `agent-chat` folder contains conversation metadata
  as individual JSON files, message journals as append-only JSONL and PNG attachments
  in each conversation's media directory. Key material
  stays in Windows Credential Manager.
  Compaction summarizes previous conversation in the model context and leaves
  the original messages in the local journal. In-progress turns require
  explicit user input to continue. The journal retains visible reasoning, step
  boundaries, compact tool previews, citations and reported usage. Full tool
  receipts and protocol replay data are scoped to a run.
- The tool registry advertises and dispatches operations after the trusted host
  grants their scope. When adding a write operation, introduce a separate write
  grant and check the actual target and revision at the point of execution.
  The model protocol, transcript storage, and tool-call/result pairing can
  remain the same.

Design references include [DeepSeek Harness session and composer layout](https://www.deepseek.com/en/harness/)
and [CC Switch model aggregation](https://github.com/farion1231/cc-switch/blob/main/docs/user-manual/en/4-proxy/4.6-aggregation.md).
