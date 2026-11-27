#pragma once

import folia.app.text_layout_options;

namespace winrt::Folia::chat
{
    inline constexpr double chatBodyFontSize = 15.0;

    inline double ChatColumnMaxWidth(TextLayoutOptions const& options)
    {
        return 768.0 + 220.0 * (1.0 - options.margin / 100.0);
    }

    inline void ApplyChatTextSize(
        Microsoft::UI::Xaml::Controls::TextBlock const& text, double baseSize, TextLayoutOptions const& options)
    {
        text.FontSize(baseSize * options.font / 100.0);
    }
}
