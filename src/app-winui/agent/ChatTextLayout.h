#pragma once

import folia.app.text_layout_options;

namespace winrt::Folia::chat
{
    inline constexpr double chatBodyFontSize = 15.0;

    inline double ChatColumnMaxWidth(TextLayoutOptions const& options)
    {
        return 768.0 + 220.0 * (1.0 - options.margin / 100.0);
    }

    inline void ApplyChatTextLayout(
        Microsoft::UI::Xaml::Controls::TextBlock const& text, double baseSize, TextLayoutOptions const& options)
    {
        auto size = baseSize * options.font / 100.0;
        text.FontSize(size);
        text.LineStackingStrategy(Microsoft::UI::Xaml::LineStackingStrategy::BlockLineHeight);
        text.LineHeight(options.lineSpacing == 100 ? 0.0 : size * 1.25 * options.lineSpacing / 100.0);
    }
}
