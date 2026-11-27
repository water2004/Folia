# Settings

The gear at the bottom of the activity rail opens one Settings tab per window.
The Settings home page lists its categories, and the breadcrumb returns to
that page. Select a document tab or a workspace rail control to leave Settings.
Files and Outline keep their shared workspace sidebar. Settings pages use a
centered content column and share typography and form styles. Shortcut and
LaTeX lists keep native scrollbars at the page edge and virtualize their rows.
Closed choices leave wheel scrolling to the page, and numeric AI budgets use
integer inputs. On the Themes page, choose a profile to see its packaged PNG
preview, then select **Apply**.

Under **General → Window appearance**, the window and reading area can use Mica,
Mica Alt, desktop acrylic, or the Folia theme's solid color. The reading area
defaults to **Follow window**. The activity rail, sidebar, and tab strip share
the window material. The selected tab, document toolbar, and editor use the
reading material.
**Auto-hide document tools** reveals the file information and editor actions
just below the tabs on hover or keyboard focus. The tabs remain visible and
hovering over the tools does not resize the document.

## Math rendering

Formulas use native `folia::math` and render automatically.
LaTeX command suggestions have an independent setting.

## AI input prediction

**AI providers** lists saved providers by name and domain. Select a provider to
edit its connection and models, or add a provider. A provider can use a local PNG
icon or a monogram derived from its name.

In the provider editor, **Test connection** checks access to the model list
and reports the elapsed time. The **Connection options** section controls hosted
search and full request URL mode. Enable full request URL mode to edit the
optional model list URL below the service address and give a custom endpoint
a separate discovery route.
The API key is stored in Windows Credential Manager.

When upgrading from settings that stored a connection and model under input
prediction, Folia imports that selection into **AI providers** on startup.
It keeps the configured protocol and reasoning level and copies any saved key
within Windows Credential Manager. If the same connection and model are
already present, Folia links to that model and keeps its configuration.

Adding or opening a model goes to its settings page. Fetch the upstream list
there and select a model to fill the fields the upstream explicitly supplies.
The request ID, display name, input and output token limits, and image input
support can also be set directly. The image input switch starts off unless the
selected upstream model explicitly advertises support. Select supported reasoning levels using
the protocol's preset checkboxes. The model's hosted search
capability is independent of the provider's permission to use it. Applying
changes returns to the provider draft. **Save provider** commits its connection
and models together. The switch beside each model controls whether it is
available in chat and input prediction. A disabled model remains editable.

**AI input prediction** selects a saved model and, when available, one of that
model's reasoning levels. Context size, output tokens, suggestion frequency,
and explicit caching are independent prediction settings. See
[inline completion](inline-completion.md) and [agent chat](agent-chat.md).

## Persistence

Settings use schema version 6 and are saved atomically to `Assets/settings.json`.

```json
{
  "schemaVersion": 6,
  "completion": {
    "enabled": false,
    "modelId": "",
    "eagerness": 1,
    "contextKB": 7,
    "outputTokens": 128
  },
  "latexSuggestionsEnabled": true,
  "themeId": "system",
  "languageId": "system",
  "shortcutBindings": []
}
```

An invalid or unavailable file uses defaults and reports a diagnostic. AI changes
are applied with Save; credentials never enter the JSON file. LaTeX suggestions,
language and shortcut changes save immediately. A language override takes effect
on the next launch (`system`, `en-US`, `zh-CN`). Theme selection stays in preview
until Apply is pressed; leaving without applying discards that preview. An active
custom theme cannot be removed until a different theme has been applied.

## Shortcuts and insertion actions

Shortcut bindings are resolved deterministically by editor context. A Code or Math binding overrides the same Global gesture only while the caret is in that context; conflicts inside one scope are rejected and reported before settings are saved. A binding with no gesture is intentionally disabled. There is no hidden hard-coded Ctrl-key fallback, so clearing a built-in binding actually disables it.

The page shows built-in and custom actions in one list. A row menu changes or clears a binding; custom actions can also be edited, reset, or removed. Adding or editing a custom action opens a focused dialog for its name, scope, gesture, and insertion template.

`Format document` is available in both rendered and source modes and defaults to `Shift+Alt+F`. It is an explicit operation and one undo step: ordinary Save remains character-exact. Formatting canonicalizes block markers, list delimiters, table separators, recursive container prefixes, and inter-block spacing through the unified block tree. It deliberately leaves inline Markdown spelling, incomplete inline CST states, fenced code/math source, and HTML syntax islands unchanged. Running it twice is idempotent.

Templates may expand to multiple lines, but the Settings field is deliberately a single-line literal. It first decodes `\\` as a backslash, `\n` as a newline, `\r` as carriage return, and `\t` as a tab; unknown or dangling escapes are rejected. The decoded source then uses a deterministic subset of VS Code snippet syntax:

- `$1`, `${1}`, and later numbered markers define the order visited by Tab; `$0` is the final caret stop.
- `${1:default}` inserts and selects default text, so typing replaces it before Tab advances.
- `${TM_SELECTED_TEXT}` inserts the exact selected block-local Markdown source.
- `${1:${TM_SELECTED_TEXT}}` inserts the selection as the first editable placeholder. A useful wrapping action is `\mathbf{${1:${TM_SELECTED_TEXT}}}$0`.
- `$$` inserts a literal dollar sign. Other decoded characters are preserved verbatim.

For example, the one-line literal `\\frac{${1}}{${2}}$0` expands to `\frac{…}{…}`, while `\\begin{matrix}\n$1\n\\end{matrix}$0` expands to a three-line matrix environment.

Insertion remains a normal block-local source edit, and the temporary Tab-stop session is UI state rather than a second document coordinate system. Selected source is inserted verbatim rather than parsed as another snippet, so Markdown and LaTeX dollar characters inside the selection are not accidentally interpreted as placeholders.

## LaTeX command suggestions

LaTeX suggestions have an independent immediate-apply switch. In rendered mode, typing a command prefix such as `\fr` inside inline or block math opens a native WinUI candidate popup beside the caret. Up/Down changes the selected candidate, Tab or Enter inserts it, and Escape closes the popup. Accepting a command replaces only the command prefix in the current editable block, using the shared snippet parser and Tab-stop session. Replacement and the first parameter selection form one source transaction, so Undo restores the original caret and Redo restores the parameter selection. The popup offers up to 32 ranked candidates in an eight-row scrollable viewport.

Built-in commands are loaded from `Assets/latex/commands.json`. Personal commands and usage history are stored separately in `Assets/latex/commands.user.json`, so application updates do not rewrite user data. Each catalog row owns exactly one trigger and one template; `matrix` and `begin{matrix}` are therefore separate commands even when their expanded templates are similar. Triggers are stored without the leading backslash and may include registered suffix syntax such as `{matrix}`. Personal commands can shadow a built-in trigger without creating a compatibility entry.

The built-in catalog contains 1769 command and environment templates, generated from the locked native TeX package profile. Its 70 environments include paired begin/end templates and required arguments. `sub` and `sup` retain their TeX subset/supremum meanings; script insertion shortcuts are `subscript` and `superscript`. See [catalog generation](../tools/latex-completion/README.md) for source ownership and validation. Runtime completion reads static data and does not initialize a math renderer.

The LaTeX page uses the same add/edit dialog and single-line escaped template notation as insertion actions. Its overflow menu resets usage ranking, while command-row menus edit, disable, reset, or remove the relevant entry. Ranking applies exponential time decay with a 14-day half-life and keeps exact prefix matches first. Built-in matrix, cases, and aligned commands expand `\n` escapes into real multiline environments; accepting a completion does not expose the command prefix itself as `TM_SELECTED_TEXT`.

## Built-in themes

Built-in themes are complete profiles rather than accent-color overlays. Each profile supplies editor and shell colors, typography, syntax colors, and block layout metrics.

- `Folia Light`, `Folia Dark`, and `Folia High Contrast` are the neutral defaults.
- `Inkstone Paper` is a spacious warm-paper reading theme with ink and cinnabar accents.
- `Moss & Linen` is a compact, low-glare light theme with subdued green structure.
- `Arctic Night` is a cool dark theme with cyan navigation and restrained syntax colors.
- `Ember Night` is a warm dark theme with amber headings and terracotta structure.
