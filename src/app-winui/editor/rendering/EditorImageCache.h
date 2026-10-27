#pragma once
#include "editor/rendering/EditorRenderResources.h"
#include "media/ImageResourceCache.h"

namespace winrt::Folia
{
    struct DecodedGifAnimation;
    struct GifInitialDecode;

    // UI-thread device images and GIF presentation. Encoded downloads and their
    // completion lifetime belong to ImageResourceCache, not to the renderer.
    class EditorImageCache
    {
    public:
        struct RasterImage
        {
            ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
            std::shared_ptr<DecodedGifAnimation> animation;
            float width = 0.0f, height = 0.0f;
            std::size_t bytes = 0;
        };
        struct SvgSource
        {
            std::optional<std::string> source;
            bool candidate = false, pending = false;
        };
        ~EditorImageCache();
        void Attach(Microsoft::UI::Dispatching::DispatcherQueue const&, std::function<void()>);
        void Detach();
        void ClearDeviceResources();
        bool HasPendingImages() const;
        std::uint64_t ResourceGeneration() const;
        std::optional<ImageDimensions> ProbeImageDimensions(EditorRenderResources const&, std::wstring const&, std::string_view);
        SvgSource LoadSvgSource(std::wstring const&, std::string_view, bool loadContent = true);
        std::optional<RasterImage> LoadRasterImage(EditorRenderResources const&, std::wstring const&, std::string_view);
        void ReleaseGifImage(std::wstring const&, std::string_view);
        ID2D1Bitmap1* CurrentBitmap(RasterImage const&, std::chrono::milliseconds&) const;
        void RequestAnimationFrame(std::chrono::milliseconds);

    private:
        struct PendingGifImage
        {
            std::shared_ptr<GifInitialDecode> decode;
            std::shared_ptr<std::vector<std::uint8_t>> encodedBacking;
            std::filesystem::path path;
        };
        void StopAnimationPump();
        ImageResourceCache sources;
        std::unordered_map<std::wstring, RasterImage> rasterImages;
        std::unordered_map<std::wstring, ImageDimensions> imageDimensions;
        std::deque<std::wstring> imageDimensionOrder;
        std::unordered_set<std::wstring> imageDimensionMisses;
        std::unordered_map<std::wstring, PendingGifImage> pendingGifImages;
        std::unordered_set<std::wstring> rasterImageFailures;
        std::deque<std::wstring> rasterImageOrder;
        std::size_t rasterImageBytes = 0;
        winrt::event_token animationRenderingToken{};
        std::optional<std::chrono::steady_clock::time_point> animationDeadline;
        bool animationPumpActive = false;
    };
}
