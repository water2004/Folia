#include "pch.h"
#include "media/ImageSource.h"

import folia.core.image_metadata;
import folia.markdown.slug;

namespace winrt::Folia::image_source
{
    std::optional<std::vector<std::uint8_t>> DecodeBase64(std::string_view source)
    {
        static constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        if (source.size() > 24 * 1024 * 1024) return std::nullopt;
        std::vector<std::uint8_t> result;
        result.reserve(source.size() * 3 / 4);
        std::uint32_t accumulator = 0;
        unsigned bits = 0;
        for (auto ch : source)
        {
            if (ch == '=') break;
            if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t') continue;
            auto position = alphabet.find(ch);
            if (position == std::string_view::npos) return std::nullopt;
            accumulator = (accumulator << 6) | static_cast<std::uint32_t>(position);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                result.push_back(static_cast<std::uint8_t>((accumulator >> bits) & 0xff));
                if (result.size() > 16 * 1024 * 1024) return std::nullopt;
            }
        }
        return result;
    }

    std::optional<ImageDimensions> ValidDimensions(std::uint32_t width, std::uint32_t height)
    {
        if (width == 0 || height == 0
            || static_cast<std::uint64_t>(width) * height > 16ull * 1024ull * 1024ull)
            return std::nullopt;
        return ImageDimensions{static_cast<float>(width), static_cast<float>(height)};
    }

    bool AsciiEqualsIgnoreCase(char left, char right)
    {
        if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
        if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
        return left == right;
    }

    bool AsciiEndsWithIgnoreCase(std::string_view value, std::string_view suffix)
    {
        if (value.size() < suffix.size()) return false;
        return std::ranges::equal(
            value.substr(value.size() - suffix.size()),
            suffix,
            AsciiEqualsIgnoreCase);
    }

    bool HasGifMagic(std::vector<std::uint8_t> const& bytes)
    {
        if (bytes.size() < 6) return false;
        auto header = std::string_view(reinterpret_cast<char const*>(bytes.data()), 6);
        return header == "GIF87a" || header == "GIF89a";
    }

    bool HasGifExtension(std::filesystem::path const& path)
    {
        auto extension = path.extension().wstring();
        return _wcsicmp(extension.c_str(), L".gif") == 0;
    }

    bool HasGifMagic(std::filesystem::path const& path)
    {
        std::array<std::uint8_t, 6> header{};
        std::ifstream stream(path, std::ios::binary);
        if (!stream) return false;
        stream.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
        if (stream.gcount() != static_cast<std::streamsize>(header.size())) return false;
        auto signature = std::string_view(reinterpret_cast<char const*>(header.data()), header.size());
        return signature == "GIF87a" || signature == "GIF89a";
    }

    std::optional<ImageDimensions> ImageDimensionsFromHeader(
        std::uint8_t const* bytes,
        std::size_t size)
    {
        if (!bytes) return std::nullopt;
        auto metadata = folia::probe_encoded_image_metadata(std::span(bytes, size));
        if (!metadata) return std::nullopt;
        return ImageDimensions{
            static_cast<float>(metadata->width),
            static_cast<float>(metadata->height),
        };
    }

    std::optional<std::vector<std::uint8_t>> DecodeImageDataUri(std::string_view source)
    {
        if (!source.starts_with("data:image/")) return std::nullopt;
        auto comma = source.find(',');
        if (comma == std::string_view::npos) return std::nullopt;
        auto metadata = source.substr(0, comma);
        if (metadata.find(";base64") != std::string_view::npos)
            return DecodeBase64(source.substr(comma + 1));
        std::vector<std::uint8_t> decoded;
        decoded.reserve((std::min)(source.size() - comma - 1, MaximumEncodedImageBytes));
        auto payload = source.substr(comma + 1);
        auto hex = [](char value) -> int
        {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10;
            if (value >= 'A' && value <= 'F') return value - 'A' + 10;
            return -1;
        };
        for (std::size_t index = 0; index < payload.size(); ++index)
        {
            if (decoded.size() >= MaximumEncodedImageBytes) return std::nullopt;
            if (payload[index] == '%' && index + 2 < payload.size())
            {
                auto high = hex(payload[index + 1]);
                auto low = hex(payload[index + 2]);
                if (high < 0 || low < 0) return std::nullopt;
                decoded.push_back(static_cast<std::uint8_t>((high << 4) | low));
                index += 2;
            }
            else
            {
                decoded.push_back(static_cast<std::uint8_t>(payload[index]));
            }
        }
        return decoded;
    }

    bool LooksLikeSvg(std::span<std::uint8_t const> bytes)
    {
        auto limit = (std::min)(bytes.size(), std::size_t{8192});
        auto text = std::string_view(reinterpret_cast<char const*>(bytes.data()), limit);
        if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t'
            || text.front() == '\r' || text.front() == '\n')) text.remove_prefix(1);
        if (text.empty() || text.front() != '<' || text.find('\0') != std::string_view::npos) return false;
        for (std::size_t index = 0; index + 4 <= text.size(); ++index)
        {
            if (text[index] == '<'
                && AsciiEqualsIgnoreCase(text[index + 1], 's')
                && AsciiEqualsIgnoreCase(text[index + 2], 'v')
                && AsciiEqualsIgnoreCase(text[index + 3], 'g'))
            {
                auto boundary = index + 4 == text.size() ? ' ' : text[index + 4];
                return boundary == ' ' || boundary == '\t' || boundary == '\r'
                    || boundary == '\n' || boundary == '>' || boundary == '/';
            }
        }
        return false;
    }

    bool HasSvgHint(std::string_view source)
    {
        if (source.starts_with("data:image/svg+xml")) return true;
        auto end = source.find_first_of("?#");
        auto path = source.substr(0, end);
        return AsciiEndsWithIgnoreCase(path, ".svg");
    }

    std::optional<std::wstring> RasterImageKey(
        std::wstring const& baseDirectory,
        std::string_view source)
    {
        if (source.empty()) return std::nullopt;
        if (source.starts_with("https://") || source.starts_with("http://"))
            return L"url:" + std::wstring(winrt::to_hstring(std::string(source)));
        if (source.starts_with("data:image/"))
            return L"data:" + std::to_wstring(source.size()) + L":"
                + std::to_wstring(std::hash<std::string_view>{}(source));
        const auto suffix = source.find_first_of("?#");
        const auto encodedPath = source.substr(0, suffix);
        const auto decodedPath = folia::percent_decode_url_component(encodedPath);
        if (decodedPath.empty()) return std::nullopt;
        auto sourceText = winrt::to_hstring(decodedPath);
        auto path = std::filesystem::path(sourceText.c_str());
        if (path.has_root_directory() && !path.has_root_name() && !baseDirectory.empty())
            path = std::filesystem::path(baseDirectory) / path.relative_path();
        if (path.is_relative())
        {
            if (baseDirectory.empty()) return std::nullopt;
            path = std::filesystem::path(baseDirectory) / path;
        }
        std::error_code error;
        auto absolute = std::filesystem::weakly_canonical(path, error);
        if (error) absolute = path.lexically_normal();
        return absolute.wstring();
    }

    std::optional<ImageDimensions> ProbeWicDimensions(
        IWICImagingFactory* factory,
        std::filesystem::path const& path)
    {
        if (!factory || path.empty()) return std::nullopt;
        ::Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(factory->CreateDecoderFromFilename(
                path.c_str(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnDemand,
                decoder.GetAddressOf()))) return std::nullopt;
        ::Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        UINT width = 0;
        UINT height = 0;
        if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())) || !frame
            || FAILED(frame->GetSize(&width, &height))) return std::nullopt;
        return ValidDimensions(width, height);
    }

    std::optional<ImageDimensions> ProbeWicDimensions(
        IWICImagingFactory* factory,
        std::vector<std::uint8_t> const& bytes)
    {
        if (!factory || bytes.empty() || bytes.size() > (std::numeric_limits<DWORD>::max)())
            return std::nullopt;
        ::Microsoft::WRL::ComPtr<IWICStream> stream;
        ::Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(factory->CreateStream(stream.GetAddressOf()))
            || FAILED(stream->InitializeFromMemory(
                const_cast<std::uint8_t*>(bytes.data()),
                static_cast<DWORD>(bytes.size())))
            || FAILED(factory->CreateDecoderFromStream(
                stream.Get(),
                nullptr,
                WICDecodeMetadataCacheOnDemand,
                decoder.GetAddressOf()))) return std::nullopt;
        ::Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        UINT width = 0;
        UINT height = 0;
        if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())) || !frame
            || FAILED(frame->GetSize(&width, &height))) return std::nullopt;
        return ValidDimensions(width, height);
    }
}
