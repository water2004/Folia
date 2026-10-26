#pragma once

#include "media/MathJaxRenderer.h"
#include "media/MathSvgStore.h"

namespace winrt::Folia
{
    class NativeMathSession;
    class SvgNormalizer;

    struct MathJaxRenderer::State : std::enable_shared_from_this<State>
    {
        struct Entry
        {
            std::uint64_t id = 0;
            std::string key;
            MathRenderPurpose purpose = MathRenderPurpose::Document;
            std::unordered_set<std::uint64_t> owners;
            std::vector<std::uint64_t> fragmentIds;
            std::stop_source cancel;
            std::shared_ptr<MathJaxSvg const> result;
            bool pending = true;
            bool unowned = false;
            bool removed = false;
        };

        struct SvgRef
        {
            std::uint64_t entryId = 0;
            std::uint64_t generation = 0;
            MathSvgStore::Range range{};
            std::shared_ptr<std::string const> resident;
            std::shared_ptr<std::string const> preview;
            std::size_t fullReaders = 0;
            bool readQueued = false;
            bool failed = false;
        };

        struct Job
        {
            enum class Kind { Render, Read, Batch } kind = Kind::Render;
            std::vector<Job> items;
            std::shared_ptr<Entry> entry;
            std::uint64_t renderId = 0;
            std::uint64_t generation = 0;
            std::string tex;
            std::string color;
            bool display = false;
            float em = 0.0f;
            float containerWidth = 0.0f;
            std::stop_token callerStop;
        };

        void StartWorker();
        static std::string MakeKey(std::string_view tex, bool display, float em,
            float width, std::string_view color, MathRenderPurpose purpose);
        std::shared_ptr<Entry> Request(std::string_view tex, bool display, float em,
            float width, std::string_view color, std::stop_token stop,
            MathRenderPurpose purpose, std::vector<Job>* batch = nullptr);
        void SubmitBatch(std::vector<Job>& items);
        void CompleteBatch(Job const& job, NativeMathSession& session,
            SvgNormalizer& normalizer, std::unique_ptr<MathSvgStore>& store,
            std::function<void()>& callback);
        static void NormalizeResults(std::span<MathJaxSvg> results, std::span<Job const> jobs,
            SvgNormalizer& normalizer, std::stop_token stop = {});
        static MathJaxSvg InfrastructureFailure(std::string message, bool display);
        static void AddSaturated(std::uint64_t& value, std::uint64_t add) noexcept;

        void CacheDocumentSvgLocked(std::uint64_t id,
            std::shared_ptr<std::string const> bytes);
        bool NeedsSvgLocked(std::uint64_t id, SvgRef const& ref) const;
        void ReleaseUnusedSvgLocked();
        void SetVisibleSvgIds(std::span<std::uint64_t const> ids);
        void RemoveSvgRefLocked(std::uint64_t id, std::uint64_t ownerEntry);
        void RemoveEntryLocked(std::shared_ptr<Entry> const& entry,
            std::vector<std::shared_ptr<Entry>>& cancelAfterUnlock);
        void RemoveIfOrphanLocked(std::uint64_t id,
            std::vector<std::shared_ptr<Entry>>& cancelAfterUnlock);
        void FinishBlock(std::vector<std::shared_ptr<Entry>>& cancelAfterUnlock);
        void Retain(std::span<std::uint64_t const> owners,
            std::vector<std::shared_ptr<Entry>>& cancelAfterUnlock);
        void QueueReadLocked(std::uint64_t id, SvgRef& ref);
        std::shared_ptr<std::string const> GetSvg(std::uint64_t id);
        std::shared_ptr<MathJaxSvg const> AwaitFull(
            std::shared_ptr<Entry> const& entry, std::stop_token stop);
        bool HasReadyChangeLocked(Entry const& entry,
            std::vector<std::shared_ptr<std::string const>> const& payloads) const;
        void Run();
        void CompleteRender(Job const& job, NativeMathSession& session,
            SvgNormalizer& normalizer, std::unique_ptr<MathSvgStore>& store,
            std::function<void()>& callback, MathJaxSvg* prepared = nullptr);
        void CompleteRead(Job const& job, MathSvgStore::Range range,
            MathSvgStore* store, std::function<void()>& callback);
        void MaybeCompact(MathSvgStore& store) noexcept;
        void FailQueuedInitialization() noexcept;
        void FailWorker() noexcept;
        void Stop() noexcept;

        mutable std::mutex mutex;
        std::condition_variable_any wake;
        std::condition_variable_any ready;
        std::condition_variable_any idle;
        std::unordered_map<std::string, std::shared_ptr<Entry>> index;
        std::unordered_map<std::uint64_t, std::shared_ptr<Entry>> entriesById;
        std::unordered_map<std::uint64_t, SvgRef> svgRefs;
        std::unordered_set<std::uint64_t> visibleSvgIds;
        std::unordered_set<std::uint64_t> residentSvgIds;
        std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> ownerEntries;
        std::optional<std::uint64_t> activeOwner;
        std::unordered_set<std::uint64_t> blockEntries;
        std::deque<Job> jobs;
        std::shared_ptr<Entry> activeRender;
        std::function<void()> completion;
        std::shared_ptr<SvgNormalizer> normalizerLease;
        std::stop_token preparationStop;
        std::filesystem::path backingPath;
        std::thread::id lastIoThread;
        std::exception_ptr workerFailure;
        std::size_t residentSvgBytes = 0;
        std::size_t previewSvgBytes = 0;
        std::size_t nativeRenders = 0;
        std::size_t nativeBatches = 0;
        std::uint64_t fileReads = 0;
        std::uint64_t fileWrites = 0;
        std::uint64_t canceledReads = 0;
        std::uint64_t discardedReads = 0;
        std::size_t pending = 0;
        std::uint64_t writtenBytes = 0;
        std::uint64_t deadBytes = 0;
        std::uint64_t nextEntryId = 1;
        std::uint64_t nextSvgGeneration = 1;
        bool stopping = false;
        bool workerFinished = false;
        bool workerBusy = false;
        bool workerStarted = false;
        bool compactionRequested = false;
        bool nativeSessionActive = false;
    };
}
