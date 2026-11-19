#include "native-renderer.h"
import std;
import folia.platform.mermaid_host;

namespace {
void copy(std::string_view text, std::uint8_t** output, std::size_t* length) {
    auto buffer = std::make_unique<std::uint8_t[]>(text.size());
    std::memcpy(buffer.get(), text.data(), text.size());
    *length = text.size();
    *output = buffer.release();
}
template<class Render>
std::int32_t render_request(const std::uint8_t* source, std::uint8_t** output,
    std::size_t* output_length, float* width, float* height, Render&& render) noexcept {
    if (!source || !output || !output_length || !width || !height) return -1;
    *output = nullptr; *output_length = 0; *width = 0; *height = 0;
    try {
        auto result = render();
        if (!result) { copy(result.error(), output, output_length); return 1; }
        copy(result->svg, output, output_length);
        *width = static_cast<float>(result->width);
        *height = static_cast<float>(result->height);
        return 0;
    } catch (...) { return 2; }
}
}

std::int32_t FoliaNativeMermaidRender(const std::uint8_t* source,
    std::size_t length, bool dark, std::uint8_t** output,
    std::size_t* output_length, float* width, float* height) noexcept {
    return render_request(source, output, output_length, width, height, [&] {
        return folia::platform::render_mermaid_utf8(
            {reinterpret_cast<const char*>(source), length}, dark);
    });
}

std::int32_t FoliaNativeMermaidRenderWithTheme(const std::uint8_t* source,
    std::size_t length, const std::uint8_t* theme, std::size_t theme_length,
    std::uint8_t** output, std::size_t* output_length, float* width, float* height) noexcept {
    if (!theme) return -1;
    return render_request(source, output, output_length, width, height,
        [&]() -> std::expected<folia::mermaid::DiagramSvg, std::string> {
            if (theme_length == 0 || theme_length > 32)
                return std::unexpected("Unsupported native theme");
            std::u16string name;
            for (std::size_t i = 0; i < theme_length; ++i) name += theme[i];
            const auto selected = folia::mermaid::find_mermaid_theme(name);
            if (!selected) return std::unexpected("Unsupported native theme");
            return folia::platform::render_mermaid_utf8(
                {reinterpret_cast<const char*>(source), length},
                folia::mermaid::ThemeOptions{.theme = *selected});
        });
}

void FoliaNativeMermaidFree(std::uint8_t* output) noexcept { delete[] output; }
