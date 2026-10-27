#pragma once

namespace winrt::Folia
{
    struct ImageDimensions
    {
        float width = 0.0f;
        float height = 0.0f;
    };

    // Image source interpretation has no cache, dispatcher or rendering state.
    namespace image_source
    {
        inline constexpr std::size_t MaximumEncodedImageBytes = 16u * 1024u * 1024u;
        std::optional<std::wstring> RasterImageKey(std::wstring const&, std::string_view);
        std::optional<std::vector<std::uint8_t>> DecodeImageDataUri(std::string_view);
        std::optional<ImageDimensions> ImageDimensionsFromHeader(std::uint8_t const*, std::size_t);
        std::optional<ImageDimensions> ProbeWicDimensions(IWICImagingFactory*, std::filesystem::path const&);
        std::optional<ImageDimensions> ProbeWicDimensions(IWICImagingFactory*, std::vector<std::uint8_t> const&);
        bool HasGifMagic(std::vector<std::uint8_t> const&);
        bool HasGifMagic(std::filesystem::path const&);
        bool HasGifExtension(std::filesystem::path const&);
        bool HasSvgHint(std::string_view);
        bool LooksLikeSvg(std::span<std::uint8_t const>);
    }
}
