#pragma once
namespace visual_tests {
    winrt::Microsoft::UI::Xaml::Window Window();
    void SavePng(std::filesystem::path const& path, std::uint32_t width, std::uint32_t height,
                 winrt::Windows::Storage::Streams::IBuffer const& pixels, std::uint32_t strideWidth = 0, std::uint32_t firstPixel = 0);
}
