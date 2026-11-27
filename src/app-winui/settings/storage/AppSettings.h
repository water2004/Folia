#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include "windows/completion/CompletionService.h"
#include "media/MediaDisplaySettings.h"
#include "workspace/WorkspaceAppearance.h"

import folia.platform.editor_shortcuts;
import folia.app.text_layout_options;

namespace winrt::Folia
{
    struct AppSettings
    {
        bool latexSuggestionsEnabled = true;
        bool openFilesInNewWindow = false;
        folia::platform::completion::CompletionConfig completion;
        std::string completionModelId;
        std::string themeId = "system";
        std::string languageId = "system";
        MediaDisplaySettings media;
        TextLayoutProfiles textLayouts;
        WorkspaceAppearance appearance;
        std::vector<folia::platform::editor::EditorShortcutBinding> shortcutBindings =
            folia::platform::editor::default_editor_shortcuts();

        bool operator==(AppSettings const&) const = default;
    };

    struct SettingsLoadResult
    {
        AppSettings settings;
        bool loadedFromFile = false;
        winrt::hstring diagnostic;
    };

    SettingsLoadResult LoadAppSettings();
    std::optional<winrt::hstring> SaveAppSettings(AppSettings const& settings);
    std::optional<winrt::hstring> SaveTextLayouts(TextLayoutProfiles const&);
}
