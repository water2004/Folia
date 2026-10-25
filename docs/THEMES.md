# Theme configuration

The application loads built-in themes from `Assets/themes` and manages imported themes in `Assets/themes/custom`. The repository ships complete neutral (`light`, `dark`, and `high-contrast`) and distinctive (`inkstone-paper`, `moss-linen`, `arctic-night`, and `ember-night`) profiles; MSBuild copies them to the configured Assets directory. Imported profiles can be previewed, selected, replaced by ID, and removed from the native full-window Settings view. Built-in profiles cannot be removed.

The theme file is the normal runtime source of truth for:

- editor and shell colors, including selection, caret, Markdown markers, code, math, tables, quotes, callouts, and syntax highlighting;
- body, heading, code, UI, and UI-monospace fonts, sizes, line heights, weights, and italic style;
- document padding and block spacing;
- quote, callout, code, math, table, image, TOC, frontmatter, footnote, and unsupported-block geometry;
- title bar, navigation pane, scrollbar, status bar, and footnote-preview dimensions.
- Markdown delimiter SVG artwork, icon size, and surrounding clearance.

Every property in the shipped files is required. A file with a missing field, wrong type, invalid color, unsupported schema version, or mismatched variant is rejected as a whole. The application reports the failure in the status bar and uses one complete built-in fail-safe profile; it never combines a partially loaded file with unrelated hardcoded values.

Colors use `#RRGGBB` or `#RRGGBBAA`, with alpha in the final byte. Font weights use numeric OpenType/DirectWrite values from 100 through 999. Font sizes and layout values are device-independent pixels and must be finite and non-negative.

## Syntax marker assets (schema version 2)

`syntaxMarkers` contains `sizeEm` (positive square size), `gapEm` (non-negative
clearance on each side), and an `icons` object of embedded SVG strings. The two
dimensions are relative to the marker's font size. Built-in profiles use 1.05
and 0.30. For block delimiter rows, size is relative to the line height and
inline clearance is omitted, aligning their tiles with the source content.
SVGs use `viewBox="0 0 24 24"`, contain vector shapes rather than text
or images, and are limited to 64 KiB UTF-8 each. `currentColor` uses the profile's
`colors.marker`. Keep drawing inside the view box so layout and ink agree.

Required icon keys: `star`, `doubleStar`, `underscore`, `doubleUnderscore`,
`tilde`, `doubleTilde`, `backtick`, `doubleBacktick`, `tripleBacktick`, `dollar`,
`doubleDollar`, `mathOpen`, `mathClose`, `mathBracketOpen`, `mathBracketClose`.
Use native Direct2D-supported SVG shapes and paths; fonts, HTML and external
assets are unnecessary. The entire theme remains one portable JSON file, so
the existing import flow also copies its artwork. There is no file access
during drawing. Applying another theme clears both icon resources and text
layouts so artwork, size, and hit testing change together.

To update a version 1 custom profile, set `schemaVersion` to 2 and add the
complete `syntaxMarkers` object from a shipped theme before customizing it.
Older or incomplete profiles receive the existing whole-profile diagnostic;
Folia does not silently mix their settings with another theme's artwork.

MSI treats the built-in `themes/*.json` files as application resources. Its
generated payload replaces each exact packaged filename on install/upgrade
and repair, including same-version upgrades. `themes/custom/` and
`settings.json` are excluded from the MSI payload and replacement rules.
The isolated Windows Installer regression can be run with
`powershell -ExecutionPolicy Bypass -File tests/installer/test_theme_upgrade.ps1`;
it creates its own test product and does not install or uninstall Folia.

The `syntax` array has exactly eleven entries in this order:

1. plain text
2. keyword
3. type
4. function
5. string
6. number
7. comment
8. punctuation/operator
9. preprocessor
10. variable/property
11. constant/special

Theme selection in Settings changes only the preview until `Apply` is pressed. Applying a theme rebuilds both the block render model and native DirectWrite/Direct2D resources. The applied `Follow Windows` selection reloads the matching light, dark, or high-contrast profile when the Windows theme changes.

## Assets directory

All application data and resource paths are relative to one Assets root:

- `settings.json` stores runtime settings, selected theme/language, and shortcut bindings;
- `themes/*.json` contains protected built-in profiles;
- `themes/custom/*.json` contains user-managed profiles;
- `latex/` contains built-in commands plus the separate user catalog and usage data;
- `licenses/`, `branding/`, and other media folders contain distributable static resources.

Without an explicit build value, the root is `./Assets` relative to the process working directory. Set the MSBuild property `FoliaAssetsDirectory` to name the Assets directory itself:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_app.ps1 -Configuration Release -AssetsDirectory "C:\path with spaces\Assets"
```

The configured value is compiled into the application and static Assets are copied there after the build. Environment references such as `%LOCALAPPDATA%` are expanded by the application at runtime; the installer uses the explicit `{LocalAppData}\Folia\Assets` token, which is resolved for the current user. Packaging should pass or materialize an appropriate per-user Assets path rather than writing settings into the installed program directory.
