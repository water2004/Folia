#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <functional>
#include <condition_variable>
#include <exception>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <stop_token>
#include <thread>
#include <vector>

namespace winrt::Folia
{
    enum class MathJaxErrorKind
    {
        None,
        Formula,
        Infrastructure,
    };

    enum class MathRenderPurpose
    {
        Document,
        Preview,
    };

    struct MathJaxSvgFragment
    {
        std::uint64_t renderId = 0;
        std::shared_ptr<std::string const> svg;
        float width = 0.0f;
        float height = 0.0f;
        float verticalAlign = 0.0f;
        float breakSpace = 0.0f;
        bool breakBefore = false;
    };

    struct MathJaxSvg
    {
        std::vector<MathJaxSvgFragment> fragments;
        std::string error;
        MathJaxErrorKind errorKind = MathJaxErrorKind::None;
        float width = 0.0f;
        float height = 0.0f;
        float verticalAlign = 0.0f;
        bool display = false;

        explicit operator bool() const
        {
            return errorKind != MathJaxErrorKind::Infrastructure
                && !fragments.empty() && width > 0.0f && height > 0.0f;
        }
    };

    // Document-owned, asynchronous native math cache. All native rendering
    // and SVG normalization run on its private worker.
    class MathJaxRenderer
    {
        struct State;
    public:
        class Batch
        {
            struct Impl;
            std::unique_ptr<Impl> impl;
            explicit Batch(std::shared_ptr<State> state);
            friend class MathJaxRenderer;
        public:
            ~Batch();
            Batch(Batch&&) noexcept;
            Batch& operator=(Batch&&) noexcept;
            void Render(std::string_view tex, bool display, float em, float containerWidth,
                std::string_view color, std::stop_token stop = {});
            void Submit();
            std::size_t Size() const noexcept;
        };
        MathJaxRenderer();
        ~MathJaxRenderer();
        MathJaxRenderer(MathJaxRenderer const&) = delete;
        MathJaxRenderer& operator=(MathJaxRenderer const&) = delete;

        MathJaxRenderer(MathJaxRenderer&&) noexcept;
        MathJaxRenderer& operator=(MathJaxRenderer&&) noexcept;
        Batch BeginBatch();

        std::shared_ptr<MathJaxSvg const> Render(
            std::string_view tex,
            bool display,
            float em,
            float containerWidth,
            std::string_view color,
            std::stop_token stop = {},
            MathRenderPurpose purpose = MathRenderPurpose::Document);
        // Reuse geometry while laying out offscreen blocks, without starting
        // rendering or disk reads. Participates in BeginBlock/EndBlock ownership.
        std::shared_ptr<MathJaxSvg const> Lookup(
            std::string_view tex, bool display, float em, float containerWidth,
            std::string_view color, MathRenderPurpose purpose = MathRenderPurpose::Document);
        std::shared_ptr<MathJaxSvg const> Prepare(
            std::string_view tex,
            bool display,
            float em,
            float containerWidth,
            std::string_view color,
            std::stop_token stop = {},
            MathRenderPurpose purpose = MathRenderPurpose::Document);
        // Lookup only: geometry/layout never implicitly schedules disk reads.
        std::shared_ptr<std::string const> GetSvg(std::uint64_t renderId);
        // Replace the viewport's demand, canceling queued reads that left it.
        void SetVisibleSvgIds(std::span<std::uint64_t const> ids);
        void CancelOffscreenRenders(std::span<std::uint64_t const> visibleBlocks);
        void WaitForIdle(std::stop_token stop = {});
        void SetCompletionCallback(std::function<void()> callback);

        struct Statistics
        {
            std::size_t entries = 0;
            std::size_t svgBytes = 0;
            std::size_t nativeRenders = 0;
            std::size_t nativeBatches = 0;
            std::size_t pending = 0;
            std::uint64_t fileReads = 0;
            std::uint64_t fileWrites = 0;
            std::size_t residentSvgBytes = 0;
            std::size_t previewSvgBytes = 0;
            std::size_t queuedReads = 0;
            std::uint64_t canceledReads = 0;
            std::uint64_t discardedReads = 0;
            bool nativeSessionActive = false;
            std::filesystem::path backingPath;
            std::thread::id lastIoThread;
        };
        Statistics Snapshot() const;

        void SetPreparationStopToken(std::stop_token stop);
        void BeginBlock(std::uint64_t owner);
        void EndBlock();
        void RetainBlocks(std::span<std::uint64_t const> owners);
        void Clear();

    private:
        std::shared_ptr<State> CurrentState() const;

        mutable std::mutex stateMutex;
        std::shared_ptr<State> state;
    };
}
