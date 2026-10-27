#include "pch.h"
#include "editor/rendering/EditorContentPreparation.h"

namespace winrt::Folia
{
    std::wstring ToWide(std::u32string_view text)
    {
        std::wstring wide;
        wide.reserve(text.size());
        for (auto codepoint : text)
        {
            if (codepoint <= 0xFFFF)
            {
                wide.push_back(static_cast<wchar_t>(codepoint));
            }
            else
            {
                codepoint -= 0x10000;
                wide.push_back(static_cast<wchar_t>(0xD800 + (codepoint >> 10)));
                wide.push_back(static_cast<wchar_t>(0xDC00 + (codepoint & 0x3FF)));
            }
        }
        return wide;
    }

    D2D1_COLOR_F InlineForeground(folia::InlineStyle const& style, D2D1_COLOR_F fallback)
    {
        if (!style.presentation || !style.presentation->foreground) return fallback;
        auto const& color = *style.presentation->foreground;
        constexpr float scale = 1.f / 255;
        return D2D1::ColorF(color.r * scale, color.g * scale, color.b * scale, color.a * scale);
    }

    std::string SvgColor(D2D1_COLOR_F color)
    {
        auto component = [](float value)
        {
            return static_cast<unsigned>((std::clamp)(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        };
        char result[8]{};
        std::snprintf(result, sizeof(result), "#%02X%02X%02X", component(color.r), component(color.g), component(color.b));
        return result;
    }

    std::optional<MermaidSvg> NormalizeMermaidSvg(MermaidSvg const& source, SvgNormalizer& normalizer, bool allowQueue)
    {
        auto normalized = normalizer.GetOrQueue(source.svg, 16.0f, allowQueue);
        if (!normalized) return std::nullopt;
        MermaidSvg result = source;
        if (static_cast<bool>(*normalized))
        {
            result.renderId = normalized->id;
            result.svg = *normalized->svg;
        }
        else
        {
            result.svg.clear();
            result.error = normalized->error;
        }
        return result;
    }

    bool IsMermaidLanguage(std::optional<std::string> const& language)
    {
        if (!language) return false;
        std::string normalized = *language;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        return normalized == "mermaid";
    }

}
