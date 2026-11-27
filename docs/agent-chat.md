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
window. Hover over the indicator for exact counts, including cached input tokens
when the provider reports them.

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

The provider editor offers DeepSeek, OpenAI, Anthropic, Google Gemini, OpenRouter,
xAI, and custom connections. The provider type supplies the available protocols,
default address and hosted tool formats. Proxy addresses and model aliases work
with the same presets. Existing untyped connections open as Custom.

Search requires a connection that allows it, a configured model with search
enabled, and permission in the current conversation. Models inherit the route's
search format or can select an alternative in their model settings. For example,
a DeepSeek Responses profile can use DeepSeek search for Flash and standard
Responses search for Pro. DeepSeek's hosted search accepts local functions in
thinking mode. With thinking off and local tools available, Folia exposes a search
function to the agent. Each invocation sends just the public query through the
same model and endpoint and returns the cited result to the turn. Google Gemini
uses its OpenAI-compatible chat endpoint. Its native Search grounding tool
requires Google's native API.

Folia renders the provider's actual search events and citation URLs in the
transcript. The Sources menu opens a URL on selection. Markdown replies use
Folia's native renderer with automatic remote image fetching disabled.

## Implementation

The chat request starts with stable instructions and the permitted tool schemas.
A saved summary and the recent conversation follow. The active turn appends
tool calls, their results and any protocol state needed to continue the request.
Images from recent turns use the model's vision capability. When a request
approaches the context limit, Folia trims older tool results, then summarizes
complete older exchanges if it still needs space. The current question and the
latest tool batch stay together. The assistant can search the original local
conversation and page through a matching message after compaction.

Chats use a stable session identifier on routes that accept it. Supported
OpenAI models combine rolling caching with explicit boundaries at the instructions
and saved summary. Anthropic uses automatic ephemeral caching. OpenRouter passes
a session identifier and uses the selected upstream model's caching contract.
xAI receives a routing key. DeepSeek and Gemini use their automatic caches.
Custom connections rely on the endpoint's defaults for chat. The inline prediction
cache setting applies separately. Reported cache reads come from the service.

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
