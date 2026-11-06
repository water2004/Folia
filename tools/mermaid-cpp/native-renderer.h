#pragma once
#include <cstddef>
#include <cstdint>

// UTF-8 in/out; output belongs to this DLL, including error text. The caller
// releases it with FoliaNativeMermaidFree. No renderer state crosses requests.
// 0 = SVG, 1 = input/render error, 2 = runtime failure, -1 = invalid ABI args.
extern "C" std::int32_t FoliaNativeMermaidRender(
    const std::uint8_t* source, std::size_t length, bool dark,
    std::uint8_t** output, std::size_t* output_length, float* width, float* height) noexcept;
extern "C" void FoliaNativeMermaidFree(std::uint8_t* output) noexcept;
