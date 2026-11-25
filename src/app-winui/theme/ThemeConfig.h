#pragma once

import folia.core.theme;
import folia.core.syntax_marker;

namespace winrt::Folia
{
    struct ThemeFileLoadResult
    {
        std::optional<folia::ThemeProfile> profile;
        winrt::hstring diagnostic;
    };

    struct ThemeLoadResult
    {
        folia::ThemeProfile profile;
        bool loadedFromFile = false;
        winrt::hstring diagnostic;
    };

    std::filesystem::path BuiltinThemeDirectory();
    std::filesystem::path BuiltinThemeProfilePath(folia::Theme variant);
    ThemeFileLoadResult LoadThemeFile(std::filesystem::path const& path);
    ThemeLoadResult LoadThemeProfile(folia::Theme variant);
    std::shared_ptr<const folia::SyntaxMarkerTheme> DefaultSyntaxMarkerTheme();
}
