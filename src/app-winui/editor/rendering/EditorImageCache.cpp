#include "pch.h"
#include "editor/rendering/EditorImageCache.h"
#include "media/EditorGifDecoder.h"

namespace winrt::Folia
{
    using namespace image_source;

    EditorImageCache::~EditorImageCache() { Detach(); }
    void EditorImageCache::Attach(Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
                                  std::function<void()> invalidate)
    {
        Detach();
        sources.Attach(dispatcher, std::move(invalidate));
    }
    void EditorImageCache::Detach()
    {
        StopAnimationPump();
        sources.Detach();
        ClearDeviceResources();
        imageDimensions.clear();
        imageDimensionOrder.clear();
        imageDimensionMisses.clear();
    }
    void EditorImageCache::ClearDeviceResources()
    {
        rasterImages.clear();
        for (auto const& [key, pending] : pendingGifImages) pending.decode->Cancel();
        pendingGifImages.clear();
        rasterImageFailures.clear();
        rasterImageOrder.clear();
        rasterImageBytes = 0;
    }
    bool EditorImageCache::HasPendingImages() const
    {
        return !pendingGifImages.empty() || sources.HasPendingContent();
    }
    std::uint64_t EditorImageCache::ResourceGeneration() const { return sources.Generation(); }

    std::optional<ImageDimensions> EditorImageCache::ProbeImageDimensions(
        EditorRenderResources const& resources,
        std::wstring const& baseDirectory,
        std::string_view source)
    {
        auto key = RasterImageKey(baseDirectory, source);
        if (!key) return std::nullopt;
        if (auto found = imageDimensions.find(*key); found != imageDimensions.end()) return found->second;
        if (imageDimensionMisses.contains(*key)) return std::nullopt;
        if (auto found = rasterImages.find(*key); found != rasterImages.end())
            return ImageDimensions{found->second.width, found->second.height};

        std::optional<ImageDimensions> dimensions;
        if (source.starts_with("data:image/"))
        {
            if (auto bytes = DecodeImageDataUri(source))
            {
                dimensions = ImageDimensionsFromHeader(bytes->data(), bytes->size());
                if (!dimensions) dimensions = ProbeWicDimensions(resources.wicFactory.Get(), *bytes);
            }
        }
        else if (source.starts_with("https://") || source.starts_with("http://"))
        {
            auto remoteSource = std::string(source);
            dimensions = sources.Dimensions(remoteSource);
            if (!dimensions)
            {
                auto content = sources.Content(remoteSource);
                if (content.bytes)
                {
                    dimensions = ImageDimensionsFromHeader(content.bytes->data(), content.bytes->size());
                    if (!dimensions) dimensions = ProbeWicDimensions(resources.wicFactory.Get(), *content.bytes);
                }
            }
            if (!dimensions) sources.Request(std::move(remoteSource), ImageFetchKind::Dimensions);
        }
        else
        {
            dimensions = ProbeWicDimensions(resources.wicFactory.Get(), std::filesystem::path(*key));
        }
        if (!dimensions)
        {
            // Remote metadata is asynchronous; unlike local/data misses it
            // must remain probeable after the range request completes.
            if (!source.starts_with("https://") && !source.starts_with("http://"))
            {
                if (imageDimensionMisses.size() >= 2048) imageDimensionMisses.clear();
                imageDimensionMisses.insert(*key);
            }
            return std::nullopt;
        }
        constexpr std::size_t dimensionLimit = 1024;
        while (imageDimensionOrder.size() >= dimensionLimit)
        {
            imageDimensions.erase(imageDimensionOrder.front());
            imageDimensionOrder.pop_front();
        }
        imageDimensionOrder.push_back(*key);
        imageDimensions.emplace(*key, *dimensions);
        return dimensions;
    }

    EditorImageCache::SvgSource EditorImageCache::LoadSvgSource(
        std::wstring const& baseDirectory,
        std::string_view source,
        bool loadContent)
    {
        SvgSource result;
        result.candidate = HasSvgHint(source);
        if (!loadContent) return result;
        auto key = RasterImageKey(baseDirectory, source);
        if (!key) return result;

        std::vector<std::uint8_t> bytes;
        if (source.starts_with("data:image/"))
        {
            auto decoded = DecodeImageDataUri(source);
            if (!decoded) return result;
            bytes = std::move(*decoded);
        }
        else if (source.starts_with("https://") || source.starts_with("http://"))
        {
            auto remoteSource = std::string(source);
            auto content = sources.Content(remoteSource);
            if (content.bytes) bytes = *content.bytes;
            if (bytes.empty())
            {
                if (content.status != ImageResourceStatus::Failed)
                {
                    sources.Request(std::move(remoteSource), ImageFetchKind::Content);
                    result.pending = true;
                }
                return result;
            }
        }
        else
        {
            auto path = std::filesystem::path(*key);
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            if (!stream) return result;
            auto length = stream.tellg();
            if (length <= 0 || static_cast<std::uint64_t>(length) > MaximumEncodedImageBytes)
                return result;
            bytes.resize(static_cast<std::size_t>(length));
            stream.seekg(0);
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) return result;
        }

        auto svg = LooksLikeSvg(bytes);
        result.candidate = result.candidate || svg;
        if (result.candidate)
            result.source.emplace(reinterpret_cast<char const*>(bytes.data()), bytes.size());
        return result;
    }

    ID2D1Bitmap1* EditorImageCache::CurrentBitmap(
        RasterImage const& image,
        std::chrono::milliseconds& untilNextFrame) const
    {
        untilNextFrame = std::chrono::milliseconds{0};
        return image.animation
            ? image.animation->CurrentBitmap(untilNextFrame)
            : image.bitmap.Get();
    }

    void EditorImageCache::RequestAnimationFrame(std::chrono::milliseconds delay)
    {
        if (delay.count() <= 0) return;
        delay = (std::clamp)(delay, std::chrono::milliseconds{1}, std::chrono::milliseconds{10000});
        auto deadline = std::chrono::steady_clock::now() + delay;
        if (!animationDeadline || deadline < *animationDeadline) animationDeadline = deadline;
        if (animationPumpActive) return;
        animationPumpActive = true;
        animationRenderingToken = winrt::Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(
            [this](auto const&, auto const&)
            {
                if (!animationDeadline)
                {
                    StopAnimationPump();
                    return;
                }
                if (std::chrono::steady_clock::now() < *animationDeadline) return;
                animationDeadline.reset();
                sources.Invalidate();
                // A visible animated image requests its following deadline
                // synchronously while the invalidated frame is drawn. If no
                // image did so, it has left the viewport and the pump can stop.
                if (!animationDeadline) StopAnimationPump();
            });
    }

    void EditorImageCache::StopAnimationPump()
    {
        if (animationPumpActive)
            winrt::Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(animationRenderingToken);
        animationRenderingToken = {};
        animationDeadline.reset();
        animationPumpActive = false;
    }

    std::optional<EditorImageCache::RasterImage> EditorImageCache::LoadRasterImage(EditorRenderResources const& resources, std::wstring const& baseDirectory, std::string_view source)
    {
        if (!resources.wicFactory || !resources.d2dContext || source.empty()) return std::nullopt;
        auto resolvedKey = RasterImageKey(baseDirectory, source);
        if (!resolvedKey) return std::nullopt;
        auto key = std::move(*resolvedKey);
        std::vector<std::uint8_t> encoded;
        auto remote = source.starts_with("https://") || source.starts_with("http://");
        auto data = source.starts_with("data:image/");
        std::filesystem::path path;
        if (remote)
        {
            if (auto found = rasterImages.find(key); found != rasterImages.end()) return found->second;
            if (rasterImageFailures.contains(key)) return std::nullopt;
            if (auto content = sources.Content(std::string(source)); content.bytes)
                encoded = *content.bytes;
            if (encoded.empty())
            {
                sources.Request(std::string(source), ImageFetchKind::Content);
                return std::nullopt;
            }
        }
        else if (data)
        {
            if (auto found = rasterImages.find(key); found != rasterImages.end()) return found->second;
            if (rasterImageFailures.contains(key)) return std::nullopt;
            auto decoded = DecodeImageDataUri(source);
            if (!decoded || decoded->empty()) return std::nullopt;
            encoded = std::move(*decoded);
        }
        else
        {
            path = std::filesystem::path(key);
        }
        if (auto found = rasterImages.find(key); found != rasterImages.end()) return found->second;
        if (rasterImageFailures.contains(key)) return std::nullopt;
        auto fail = [&]() -> std::optional<RasterImage>
        {
            rasterImageFailures.insert(key);
            return std::nullopt;
        };

        std::shared_ptr<std::vector<std::uint8_t>> encodedBacking;
        std::shared_ptr<GifInitialFrame const> gifInitial;
        auto gifCandidate = !encoded.empty()
            ? HasGifMagic(encoded)
            : HasGifExtension(path) || HasGifMagic(path);
        if (gifCandidate)
        {
            imageDimensionMisses.erase(key);
            auto pending = pendingGifImages.find(key);
            if (pending == pendingGifImages.end())
            {
                if (!encoded.empty())
                    encodedBacking = std::make_shared<std::vector<std::uint8_t>>(std::move(encoded));
                auto completion = sources.CompletionCallback();
                auto decode = QueueGifInitialDecode(
                    path,
                    encodedBacking,
                    std::move(completion),
                    64u * 1024u * 1024u);
                pendingGifImages.emplace(key, PendingGifImage{ std::move(decode), std::move(encodedBacking), path });
                return std::nullopt;
            }
            if (!pending->second.decode->Complete()) return std::nullopt;
            if (pending->second.decode->Failed())
            {
                pendingGifImages.erase(pending);
                return fail();
            }
            gifInitial = pending->second.decode->Result();
            encodedBacking = pending->second.encodedBacking;
            path = pending->second.path;
            pendingGifImages.erase(pending);
        }

        ::Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        ::Microsoft::WRL::ComPtr<IWICStream> stream;
        if (!encodedBacking && !encoded.empty())
            encodedBacking = std::make_shared<std::vector<std::uint8_t>>(std::move(encoded));
        if (encodedBacking && !encodedBacking->empty())
        {
            if (encodedBacking->size() > (std::numeric_limits<DWORD>::max)()) return fail();
            if (FAILED(resources.wicFactory->CreateStream(stream.GetAddressOf()))
                || FAILED(stream->InitializeFromMemory(encodedBacking->data(), static_cast<DWORD>(encodedBacking->size())))) return fail();
            if (FAILED(resources.wicFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()))) return fail();
        }
        else if (FAILED(resources.wicFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()))) return fail();
        RasterImage image;
        if (auto decoded = DecodeGifAnimation(
                resources.wicFactory.Get(),
                resources.d2dContext.Get(),
                decoder.Get(),
                encodedBacking,
                gifInitial,
                path,
                sources.CompletionCallback(false),
                64u * 1024u * 1024u))
        {
            image.bitmap = decoded->Bitmap();
            image.animation = std::move(decoded);
            image.width = static_cast<float>(image.animation->Width());
            image.height = static_cast<float>(image.animation->Height());
            image.bytes = image.animation->MemoryCost();
        }
        else
        {
            ::Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
            UINT width = 0;
            UINT height = 0;
            if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())) || !frame
                || FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0) return fail();
            auto pixels = static_cast<std::uint64_t>(width) * height;
            if (pixels > 16ull * 1024ull * 1024ull) return fail();
            ::Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
            if (FAILED(resources.wicFactory->CreateFormatConverter(converter.GetAddressOf()))
                || FAILED(converter->Initialize(
                    frame.Get(),
                    GUID_WICPixelFormat32bppPBGRA,
                    WICBitmapDitherTypeNone,
                    nullptr,
                    0.0,
                    WICBitmapPaletteTypeMedianCut))
                || FAILED(resources.d2dContext->CreateBitmapFromWicBitmap(
                    converter.Get(),
                    nullptr,
                    image.bitmap.GetAddressOf())) || !image.bitmap) return fail();
            image.width = static_cast<float>(width);
            image.height = static_cast<float>(height);
            image.bytes = static_cast<std::size_t>(pixels * 4);
        }

        while (!rasterImageOrder.empty() && (rasterImages.size() >= 32 || rasterImageBytes + image.bytes > 64 * 1024 * 1024))
        {
            auto oldest = std::move(rasterImageOrder.front());
            rasterImageOrder.pop_front();
            auto found = rasterImages.find(oldest);
            if (found == rasterImages.end()) continue;
            rasterImageBytes -= found->second.bytes;
            rasterImages.erase(found);
        }
        if (image.bytes <= 64 * 1024 * 1024)
        {
            rasterImageBytes += image.bytes;
            rasterImageOrder.push_back(key);
            rasterImages.emplace(key, image);
        }
        imageDimensionMisses.erase(key);
        if (!imageDimensions.contains(key))
        {
            constexpr std::size_t dimensionLimit = 1024;
            while (imageDimensionOrder.size() >= dimensionLimit)
            {
                imageDimensions.erase(imageDimensionOrder.front());
                imageDimensionOrder.pop_front();
            }
            imageDimensionOrder.push_back(key);
        }
        imageDimensions.insert_or_assign(key, ImageDimensions{image.width, image.height});
        return image;
    }

    void EditorImageCache::ReleaseGifImage(
        std::wstring const& baseDirectory,
        std::string_view source)
    {
        auto key = RasterImageKey(baseDirectory, source);
        if (!key) return;
        if (auto pending = pendingGifImages.find(*key); pending != pendingGifImages.end())
        {
            pending->second.decode->Cancel();
            pendingGifImages.erase(pending);
        }
        auto found = rasterImages.find(*key);
        if (found == rasterImages.end() || !found->second.animation) return;
        // The cache owns one reference. Other prepared blocks keep the shared
        // animation alive and prevent a duplicate decoder from being created.
        if (found->second.animation.use_count() != 1) return;
        rasterImageBytes -= found->second.bytes;
        rasterImages.erase(found);
        std::erase(rasterImageOrder, *key);
    }
}
