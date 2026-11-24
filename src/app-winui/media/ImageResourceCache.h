#pragma once
#include "windows/media/images/ImageSource.h"
#include "windows/media/images/ImageDownload.h"

namespace winrt::Folia
{
    using folia::platform::ImageDimensions;
    using folia::platform::ImageFetchKind;
    namespace image_source = folia::platform::image_source;

    enum class ImageResourceStatus { Missing, Pending, Ready, Failed };
    using EncodedImage = std::shared_ptr<std::vector<std::uint8_t> const>;

    struct ImageResource
    {
        EncodedImage bytes;
        ImageResourceStatus status = ImageResourceStatus::Missing;
    };

    // Owns encoded HTTP content and metadata, never WIC/D2D objects. Each Attach
    // starts a new publication lifetime, including decoder completion callbacks.
    class ImageResourceCache
    {
        struct State;
    public:
        using Completion = folia::platform::ImageDownloadCompletion;
        using Fetch = std::function<void(std::string, ImageFetchKind, Completion)>;
        explicit ImageResourceCache(Fetch fetch = {});
        ~ImageResourceCache();
        ImageResourceCache(ImageResourceCache const&) = delete;
        ImageResourceCache& operator=(ImageResourceCache const&) = delete;
        void Attach(Microsoft::UI::Dispatching::DispatcherQueue const&, std::function<void()>);
        void Detach();
        ImageResource Content(std::string const&) const;
        std::optional<ImageDimensions> Dimensions(std::string const&) const;
        void Request(std::string source, ImageFetchKind);
        bool HasPendingContent() const;
        std::uint64_t Generation() const;
        std::function<void()> CompletionCallback(bool dimensionsChanged = true) const;
        void Invalidate() const;

    private:
        static void Publish(std::shared_ptr<State> const&, std::string const&,
                            ImageFetchKind, std::vector<std::uint8_t>);
        static void Notify(std::shared_ptr<State> const&, bool dimensionsChanged);
        Fetch fetch;
        std::shared_ptr<State> state;
    };
}
