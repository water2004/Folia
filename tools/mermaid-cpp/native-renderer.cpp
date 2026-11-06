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
}

std::int32_t FoliaNativeMermaidRender(const std::uint8_t* source,
    std::size_t length, bool dark, std::uint8_t** output,
    std::size_t* output_length, float* width, float* height) noexcept {
    if (!source || !output || !output_length || !width || !height) return -1;
    *output = nullptr; *output_length = 0; *width = 0; *height = 0;
    try {
        auto result = folia::platform::render_mermaid_utf8(
            {reinterpret_cast<const char*>(source), length}, dark);
        if (!result) { copy(result.error(), output, output_length); return 1; }
        copy(result->svg, output, output_length);
        *width = static_cast<float>(result->width);
        *height = static_cast<float>(result->height);
        return 0;
    } catch (...) { return 2; }
}

void FoliaNativeMermaidFree(std::uint8_t* output) noexcept { delete[] output; }
