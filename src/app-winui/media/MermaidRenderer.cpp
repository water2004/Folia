#include "pch.h"
#include "media/MermaidRenderer.h"
#include "media/SvgNormalizer.h"
#include "../../../tools/mermaid-cpp/native-renderer.h"
#include <cmath>

namespace
{
    std::wstring ModuleDirectory()
    {
        std::wstring path(32768, L'\0');
        auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        path.resize(length);
        auto separator = path.find_last_of(L"\\/");
        return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator + 1);
    }

    struct NativeLibrary
    {
        NativeLibrary()
        {
            module = LoadLibraryExW((ModuleDirectory() + L"FoliaNativeMermaid.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (!module)
            {
                error = "Native Mermaid library could not be loaded";
                return;
            }
            render = reinterpret_cast<Render>(GetProcAddress(module, "FoliaNativeMermaidRender"));
            renderWithTheme = reinterpret_cast<RenderWithTheme>(GetProcAddress(module, "FoliaNativeMermaidRenderWithTheme"));
            themeName = reinterpret_cast<ThemeName>(GetProcAddress(module, "FoliaNativeMermaidThemeName"));
            freeBuffer = reinterpret_cast<FreeBuffer>(GetProcAddress(module, "FoliaNativeMermaidFree"));
            if (!render || !freeBuffer)
                error = "Native Mermaid library has an incompatible ABI";
        }

        ~NativeLibrary()
        {
            if (module) FreeLibrary(module);
        }

        using Render = decltype(&FoliaNativeMermaidRender);
        using RenderWithTheme = decltype(&FoliaNativeMermaidRenderWithTheme);
        using ThemeName = decltype(&FoliaNativeMermaidThemeName);
        using FreeBuffer = decltype(&FoliaNativeMermaidFree);

        HMODULE module = nullptr;
        Render render = nullptr;
        RenderWithTheme renderWithTheme = nullptr;
        ThemeName themeName = nullptr;
        FreeBuffer freeBuffer = nullptr;
        std::string error;
    };

    NativeLibrary const& Library()
    {
        static const NativeLibrary library;
        return library;
    }

    class Runtime
    {
    public:
        winrt::Folia::MermaidSvg Process(std::string const& source,
            std::string const& theme, bool dark, std::uint64_t id)
        {
            winrt::Folia::MermaidSvg result;
            result.renderId = id;
            auto const& library = Library();
            if (!library.error.empty())
            {
                result.error = library.error;
                return result;
            }
            if (!theme.empty() && !library.renderWithTheme)
            {
                result.error = "Native Mermaid library does not support named themes";
                return result;
            }
            std::uint8_t* output = nullptr;
            std::size_t outputLength = 0;
            auto bytes = reinterpret_cast<std::uint8_t const*>(source.data());
            auto status = theme.empty()
                ? library.render(bytes, source.size(), dark, &output, &outputLength, &result.width, &result.height)
                : library.renderWithTheme(bytes, source.size(),
                    reinterpret_cast<std::uint8_t const*>(theme.data()), theme.size(),
                    &output, &outputLength, &result.width, &result.height);
            auto release = [&library](std::uint8_t* buffer) { if (buffer) library.freeBuffer(buffer); };
            std::unique_ptr<std::uint8_t, decltype(release)> ownedOutput(output, release);
            std::string value;
            if (output && outputLength > 0) value.assign(reinterpret_cast<char const*>(output), outputLength);
            if (status != 0)
            {
                result.error = value.empty() ? "Native Mermaid rendering failed" : std::move(value);
                return result;
            }
            if (!std::isfinite(result.width) || !std::isfinite(result.height)
                || result.width <= 0.0f || result.height <= 0.0f)
            {
                result.error = "Native Mermaid returned an invalid viewport";
                return result;
            }
            // usvg silently omits HTML subtrees. Require the native painter's
            // portable SVG labels instead of publishing an incomplete diagram.
            if (value.find("<foreignObject") != std::string::npos)
            {
                result.error = "Native Mermaid output contains unsupported HTML labels";
                return result;
            }
            // The ABI owns intrinsic size. A numeric outer viewport also handles
            // Info's percentage-only SVG without changing the inner viewBox or
            // any measured coordinates. Nested SVG resolves percentages here.
            auto width = std::to_string(result.width);
            auto height = std::to_string(result.height);
            value = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + width
                + "\" height=\"" + height + "\" viewBox=\"0 0 " + width + " " + height
                + "\">" + value + "</svg>";
            // Direct2D's SVG subset needs text/CSS converted to paths. Reuse
            // the independent SVG normalizer on this worker before publishing
            // a complete result; preview and PDF share the same ready state.
            auto normalized = normalizer.NormalizeNow(value, 16.0f);
            if (normalized)
            {
                result.svg = *normalized.svg;
            }
            else result.error = normalized.error.empty() ? "Mermaid SVG normalization failed" : std::move(normalized.error);
            return result;
        }

    private:
        winrt::Folia::SvgNormalizer normalizer;
    };
}

namespace winrt::Folia
{
    struct MermaidRenderer::State
    {
        struct Request
        {
            std::string key;
            std::string source;
            std::string theme;
            bool dark = false;
            std::uint64_t id = 0;
            std::uint64_t generation = 0;
        };

        State() : worker([this] { Run(); }) {}

        ~State()
        {
            {
                std::scoped_lock lock(mutex);
                stopping = true;
                completion = {};
            }
            wake.notify_one();
            if (worker.joinable()) worker.join();
        }

        void Run()
        {
            Runtime runtime;
            for (;;)
            {
                Request request;
                {
                    std::unique_lock lock(mutex);
                    wake.wait(lock, [this] { return stopping || !pending.empty(); });
                    if (stopping) return;
                    request = std::move(pending.front());
                    pending.pop_front();
                }
                auto result = runtime.Process(request.source, request.theme, request.dark, request.id);
                std::function<void()> callback;
                {
                    std::scoped_lock lock(mutex);
                    if (request.generation == generation)
                    {
                        queued.erase(request.key);
                        Store(request.key, std::move(result));
                        if (!stopping) callback = completion;
                    }
                }
                if (callback) callback();
            }
        }

        static std::size_t ResultBytes(MermaidSvg const& result)
        {
            return sizeof(result) + result.svg.size() + result.error.size();
        }

        void Clear()
        {
            ++generation;
            pending.clear();
            queued.clear();
            cache.clear();
            cacheOrder.clear();
            cacheBytes = 0;
        }

        void Store(std::string const& key, MermaidSvg result)
        {
            constexpr std::size_t budget = 16 * 1024 * 1024;
            constexpr std::size_t limit = 64;
            auto bytes = key.size() + ResultBytes(result);
            while (!cacheOrder.empty() && (cacheBytes + bytes > budget || cache.size() >= limit))
            {
                auto oldest = std::move(cacheOrder.front());
                cacheOrder.pop_front();
                auto found = cache.find(oldest);
                if (found == cache.end()) continue;
                cacheBytes -= oldest.size() + ResultBytes(found->second);
                cache.erase(found);
            }
            if (bytes <= budget)
            {
                cacheBytes += bytes;
                cacheOrder.push_back(key);
                cache.emplace(key, std::move(result));
            }
        }

        std::mutex mutex;
        std::condition_variable wake;
        bool stopping = false;
        std::deque<Request> pending;
        std::unordered_set<std::string> queued;
        std::unordered_map<std::string, MermaidSvg> cache;
        std::deque<std::string> cacheOrder;
        std::size_t cacheBytes = 0;
        std::uint64_t nextId = 1;
        std::uint64_t generation = 0;
        std::string theme;
        std::function<void()> completion;
        // Start only after every queue/cache member has been initialized.
        std::thread worker;
    };

    MermaidRenderer::MermaidRenderer() : state(std::make_unique<State>()) {}
    MermaidRenderer::~MermaidRenderer() = default;

    std::span<const std::string> MermaidRenderer::ThemeNames()
    {
        static const auto names = [] {
            std::vector<std::string> result;
            auto const& library = Library();
            if (library.error.empty() && library.renderWithTheme && library.themeName)
            {
                for (std::size_t i = 0; auto name = library.themeName(i); ++i)
                {
                    std::u16string_view text(name);
                    result.push_back(winrt::to_string(std::wstring(text.begin(), text.end())));
                }
            }
            return result;
        }();
        return names;
    }

    void MermaidRenderer::SetTheme(std::string theme)
    {
        std::scoped_lock lock(state->mutex);
        if (state->theme == theme) return;
        state->theme = std::move(theme);
        state->Clear();
    }

    void MermaidRenderer::SetCompletionCallback(std::function<void()> completion)
    {
        std::scoped_lock lock(state->mutex);
        state->completion = std::move(completion);
    }

    std::optional<MermaidSvg> MermaidRenderer::GetOrQueue(std::string_view source, bool dark, bool allowQueue)
    {
        if (source.empty()) return std::nullopt;
        std::scoped_lock lock(state->mutex);
        // A fixed theme has one cache entry regardless of the Folia theme.
        auto key = std::string(source) + '\x1f' + (state->theme.empty() && dark ? "1" : "0");
        if (auto found = state->cache.find(key); found != state->cache.end()) return found->second;
        if (!allowQueue) return std::nullopt;
        if (state->queued.insert(key).second)
        {
            while (state->pending.size() >= 128)
            {
                state->queued.erase(state->pending.front().key);
                state->pending.pop_front();
            }
            state->pending.push_back(State::Request{ key, std::string(source), state->theme,
                dark, state->nextId++, state->generation });
            state->wake.notify_one();
        }
        return std::nullopt;
    }

    void MermaidRenderer::Clear()
    {
        std::scoped_lock lock(state->mutex);
        state->Clear();
    }
}
