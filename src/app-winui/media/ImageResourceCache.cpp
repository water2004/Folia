#include "pch.h"
#include "media/ImageResourceCache.h"

namespace
{
    using namespace winrt::Folia;
    constexpr std::uint32_t MetadataBytes = 64u * 1024u;

    winrt::fire_and_forget DownloadImage(std::string source, ImageFetchKind kind,
                                       ImageResourceCache::Completion complete)
    {
        std::vector<std::uint8_t> bytes;
        try
        {
            co_await winrt::resume_background();
            auto client = winrt::Windows::Web::Http::HttpClient{};
            auto uri = winrt::Windows::Foundation::Uri(winrt::to_hstring(source));
            if (kind == ImageFetchKind::Content)
            {
                auto buffer = co_await client.GetBufferAsync(uri);
                if (buffer.Length() <= image_source::MaximumEncodedImageBytes)
                {
                    bytes.resize(buffer.Length());
                    auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer);
                    reader.ReadBytes(winrt::array_view<std::uint8_t>(bytes));
                }
            }
            else
            {
                auto request = winrt::Windows::Web::Http::HttpRequestMessage{
                    winrt::Windows::Web::Http::HttpMethod::Get(), uri};
                request.Headers().TryAppendWithoutValidation(L"Range", L"bytes=0-65535");
                auto response = co_await client.SendRequestAsync(request,
                    winrt::Windows::Web::Http::HttpCompletionOption::ResponseHeadersRead);
                if (response.IsSuccessStatusCode())
                {
                    auto reader = winrt::Windows::Storage::Streams::DataReader{
                        co_await response.Content().ReadAsInputStreamAsync()};
                    reader.InputStreamOptions(winrt::Windows::Storage::Streams::InputStreamOptions::Partial);
                    auto count = co_await reader.LoadAsync(MetadataBytes);
                    bytes.resize((std::min)(count, MetadataBytes));
                    reader.ReadBytes(winrt::array_view<std::uint8_t>(bytes));
                }
            }
        }
        catch (...) { bytes.clear(); }
        complete(std::move(bytes));
    }
}

namespace winrt::Folia
{
    struct ImageResourceCache::State
    {
        std::mutex mutex;
        Microsoft::UI::Dispatching::DispatcherQueue dispatcher{ nullptr };
        std::function<void()> invalidate;
        std::unordered_map<std::string, EncodedImage> data;
        std::unordered_map<std::string, ImageDimensions> dimensions;
        std::unordered_set<std::string> pending, failed, dimensionPending, dimensionFailed;
        std::deque<std::string> order, dimensionOrder;
        std::size_t bytes = 0;
        std::atomic_uint64_t generation = 0;
        bool active = false;
    };

    ImageResourceCache::ImageResourceCache(Fetch loader)
        : fetch(loader ? std::move(loader) : Fetch{DownloadImage}), state(std::make_shared<State>()) {}
    ImageResourceCache::~ImageResourceCache() { Detach(); }

    void ImageResourceCache::Attach(Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
                                   std::function<void()> invalidate)
    {
        Detach();
        auto next = std::make_shared<State>();
        next->generation = state->generation.load();
        next->dispatcher = dispatcher;
        next->invalidate = std::move(invalidate);
        next->active = true;
        state = std::move(next);
    }

    void ImageResourceCache::Detach()
    {
        std::scoped_lock lock(state->mutex);
        state->active = false;
        state->invalidate = {};
        state->dispatcher = nullptr;
        state->data.clear();
        state->dimensions.clear();
        state->pending.clear();
        state->failed.clear();
        state->dimensionPending.clear();
        state->dimensionFailed.clear();
        state->order.clear();
        state->dimensionOrder.clear();
        state->bytes = 0;
        ++state->generation;
    }

    ImageResource ImageResourceCache::Content(std::string const& source) const
    {
        std::scoped_lock lock(state->mutex);
        if (auto found = state->data.find(source); found != state->data.end())
            return {found->second, ImageResourceStatus::Ready};
        return {{}, state->pending.contains(source) ? ImageResourceStatus::Pending
            : state->failed.contains(source) ? ImageResourceStatus::Failed : ImageResourceStatus::Missing};
    }

    std::optional<ImageDimensions> ImageResourceCache::Dimensions(std::string const& source) const
    {
        std::scoped_lock lock(state->mutex);
        auto found = state->dimensions.find(source);
        return found == state->dimensions.end() ? std::nullopt : std::optional(found->second);
    }

    void ImageResourceCache::Request(std::string source, ImageFetchKind kind)
    {
        auto requestState = state;
        {
            std::scoped_lock lock(requestState->mutex);
            auto& pending = kind == ImageFetchKind::Content ? requestState->pending : requestState->dimensionPending;
            auto& failed = kind == ImageFetchKind::Content ? requestState->failed : requestState->dimensionFailed;
            const bool ready = kind == ImageFetchKind::Content ? requestState->data.contains(source)
                                                              : requestState->dimensions.contains(source);
            if (!requestState->active || ready || pending.contains(source) || failed.contains(source)) return;
            pending.insert(source);
        }
        auto complete = [requestState, source, kind](std::vector<std::uint8_t> bytes)
        {
            Publish(requestState, source, kind, std::move(bytes));
        };
        try { fetch(std::move(source), kind, complete); }
        catch (...) { complete({}); }
    }

    void ImageResourceCache::Publish(std::shared_ptr<State> const& state, std::string const& source,
                                    ImageFetchKind kind, std::vector<std::uint8_t> bytes)
    {
        {
            std::scoped_lock lock(state->mutex);
            auto& pending = kind == ImageFetchKind::Content ? state->pending : state->dimensionPending;
            if (!state->active || pending.erase(source) == 0) return;
            if (kind == ImageFetchKind::Content)
            {
                if (bytes.empty() || bytes.size() > image_source::MaximumEncodedImageBytes)
                    state->failed.insert(source);
                else
                {
                    constexpr std::size_t budget = 32u * 1024u * 1024u;
                    while (!state->order.empty() && (state->data.size() >= 16 || state->bytes + bytes.size() > budget))
                    {
                        auto oldest = std::move(state->order.front());
                        state->order.pop_front();
                        auto found = state->data.find(oldest);
                        if (found == state->data.end()) continue;
                        state->bytes -= found->second->size();
                        state->data.erase(found);
                    }
                    state->bytes += bytes.size();
                    state->order.push_back(source);
                    state->data.emplace(source, std::make_shared<std::vector<std::uint8_t> const>(std::move(bytes)));
                }
            }
            else if (auto dimensions = image_source::ImageDimensionsFromHeader(bytes.data(), bytes.size()))
            {
                while (state->dimensionOrder.size() >= 1024)
                {
                    state->dimensions.erase(state->dimensionOrder.front());
                    state->dimensionOrder.pop_front();
                }
                state->dimensionOrder.push_back(source);
                state->dimensions.emplace(source, *dimensions);
            }
            else state->dimensionFailed.insert(source);
        }
        Notify(state, true);
    }

    void ImageResourceCache::Notify(std::shared_ptr<State> const& state, bool dimensionsChanged)
    {
        Microsoft::UI::Dispatching::DispatcherQueue dispatcher{ nullptr };
        {
            std::scoped_lock lock(state->mutex);
            if (!state->active) return;
            if (dimensionsChanged) ++state->generation;
            dispatcher = state->dispatcher;
        }
        if (dispatcher) dispatcher.TryEnqueue([state]
        {
            std::function<void()> invalidate;
            {
                std::scoped_lock lock(state->mutex);
                if (state->active) invalidate = state->invalidate;
            }
            if (invalidate) invalidate();
        });
    }

    std::function<void()> ImageResourceCache::CompletionCallback(bool dimensionsChanged) const
    {
        return [state = state, dimensionsChanged] { Notify(state, dimensionsChanged); };
    }

    void ImageResourceCache::Invalidate() const
    {
        std::function<void()> invalidate;
        {
            std::scoped_lock lock(state->mutex);
            if (state->active) invalidate = state->invalidate;
        }
        if (invalidate) invalidate();
    }

    std::uint64_t ImageResourceCache::Generation() const { return state->generation.load(); }
    bool ImageResourceCache::HasPendingContent() const
    {
        std::scoped_lock lock(state->mutex);
        return !state->pending.empty(); // Metadata requests do not hold PDF export.
    }
}
