#pragma once

#include <array>
#include <string>

namespace winrt::Folia
{
    struct MediaDisplaySettings
    {
        static constexpr std::array imageWidths{25, 50, 75, 100};
        int imageMaxWidthPercent = imageWidths.back();
        std::string mermaidTheme; // Empty follows the Folia theme.

        bool operator==(MediaDisplaySettings const&) const = default;
    };
}
