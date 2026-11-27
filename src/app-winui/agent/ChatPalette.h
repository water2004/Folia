#pragma once
import folia.core.theme;

namespace winrt::Folia::chat {
    // Share the same brush instances with the detached picker flyout and the composer.
    struct ChatPalette {
        using Brush = Microsoft::UI::Xaml::Media::SolidColorBrush;
        Brush accent, onAccent, surface, track, border, muted;

        void Apply(folia::ThemeColors const& colors) {
            auto set = [](Brush const& brush, folia::Color c) { brush.Color({c.a, c.r, c.g, c.b}); };
            auto channel = [](double c) {
                c /= 255.;
                return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4);
            };
            auto luminance = [&](folia::Color c) { return .2126 * channel(c.r) + .7152 * channel(c.g) + .0722 * channel(c.b); };
            auto contrast = [&](folia::Color a, folia::Color b) {
                auto x = luminance(a), y = luminance(b);
                return ((std::max)(x,y) + .05) / ((std::min)(x,y) + .05);
            };
            auto foreground = colors.shell_fg, background = colors.shell_layer_bg;
            auto ink = contrast(foreground, colors.shell_accent) >= contrast(background, colors.shell_accent) ? foreground : background;
            if (contrast(ink, colors.shell_accent) < 4.5)
                ink = luminance(colors.shell_accent) > .179 ? folia::Color{0,0,0} : folia::Color{255,255,255};
            auto blend = [](unsigned a, unsigned b) { return static_cast<std::uint8_t>((a * 7 + b) / 8); };
            set(accent, colors.shell_accent);
            set(onAccent, ink);
            set(surface, background);
            set(track, {blend(background.r, foreground.r), blend(background.g, foreground.g), blend(background.b, foreground.b)});
            set(border, colors.shell_border);
            set(muted, colors.shell_muted_fg);
        }
    };
}
