#pragma once
#include <cstddef>
#include <cstdint>

// UTF-8 in/out; output belongs to this DLL, including error text. The caller
// releases it with FoliaNativeMermaidFree. No renderer state crosses requests.
// 0 = SVG, 1 = input/render error, 2 = runtime failure, -1 = invalid ABI args.
extern "C" std::int32_t FoliaNativeMermaidRender(
    const std::uint8_t* source, std::size_t length, bool dark,
    std::uint8_t** output, std::size_t* output_length, float* width, float* height) noexcept;
// Named theme: one of the canonical UTF-8 names documented in themes.md.
// Theme is required and bounded to 32 bytes; unknown names are input errors.
// Source frontmatter/init may override it. Unlike the legacy bool entry point,
// this preserves each diagram's default look (or its explicit source setting).
// Output ownership and status codes are identical to FoliaNativeMermaidRender.
extern "C" std::int32_t FoliaNativeMermaidRenderWithTheme(
    const std::uint8_t* source, std::size_t length,
    const std::uint8_t* theme, std::size_t theme_length,
    std::uint8_t** output, std::size_t* output_length, float* width, float* height) noexcept;
extern "C" void FoliaNativeMermaidFree(std::uint8_t* output) noexcept;
