export module folia.app.text_layout_options;
import std;

export namespace winrt::Folia
{
    struct TextLayoutOptions
    {
        int font = 100;
        int lineSpacing = 100;
        int margin = 100;

        bool operator==(TextLayoutOptions const&) const = default;
    };

    enum class TextLayoutTarget { Reading, Chat };

    struct TextLayoutProfiles
    {
        TextLayoutOptions reading;
        TextLayoutOptions chat;

        bool operator==(TextLayoutProfiles const&) const = default;
    };

    struct TextLayoutRange
    {
        int TextLayoutOptions::* value;
        int minimum, maximum;
    };

    inline constexpr int textLayoutStep = 5;
    inline constexpr std::array textLayoutRanges{
        TextLayoutRange{&TextLayoutOptions::font, 75, 160},
        TextLayoutRange{&TextLayoutOptions::lineSpacing, 90, 170},
        TextLayoutRange{&TextLayoutOptions::margin, 0, 200},
    };

    inline int TextLayoutPercent(double value, TextLayoutRange const& range)
    {
        if (!std::isfinite(value)) return 100;
        auto bounded = std::clamp(value, double(range.minimum), double(range.maximum));
        return static_cast<int>(std::lround(bounded / textLayoutStep)) * textLayoutStep;
    }
}
