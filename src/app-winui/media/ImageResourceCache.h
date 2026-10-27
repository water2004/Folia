#pragma once
#include "media/ImageSource.h"

namespace winrt::Folia
{
    enum class ImageFetchKind { Content, Dimensions };
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
        using Completion = std::function<void(std::vector<std::uint8_t>)>;
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
