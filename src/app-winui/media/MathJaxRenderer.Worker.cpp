#include "pch.h"

import std;
#include "media/MathJaxRenderer.State.h"
#include "media/NativeMathSession.h"
#include "media/SvgNormalizer.h"

namespace winrt::Folia
{
    void MathJaxRenderer::State::CacheDocumentSvgLocked(
        std::uint64_t id, std::shared_ptr<std::string const> bytes)
    {
        auto found = svgRefs.find(id);
        if (found == svgRefs.end()) return;
        auto& ref = found->second;
        if (!NeedsSvgLocked(id, ref)) return;
        residentSvgIds.insert(id);
        if (ref.resident) residentSvgBytes -= ref.resident->size();
        ref.resident = std::move(bytes);
        if (ref.resident) residentSvgBytes += ref.resident->size();
    }

    void MathJaxRenderer::State::Run()
    {
        std::unique_ptr<NativeMathSession> session;
        std::shared_ptr<SvgNormalizer> normalizer;
        std::unique_ptr<MathSvgStore> store;
        {
            std::scoped_lock lock(mutex);
            if (stopping)
            {
                workerFinished = true;
                ready.notify_all();
                idle.notify_all();
                return;
            }
            normalizer = normalizerLease;
        }
        if (!normalizer)
        {
            FailQueuedInitialization();
            return;
        }
        for (;;)
        {
            Job job;
            MathSvgStore::Range readRange{};
            bool compact = false;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [&] { return stopping || !jobs.empty() || compactionRequested; });
                if (stopping) break;
                if (jobs.empty() && compactionRequested)
                {
                    compact = true;
                    compactionRequested = false;
                    workerBusy = true;
                }
                else
                {
                    // Editing previews run before pending disk reads and document work.
                    auto selected = std::find_if(jobs.begin(), jobs.end(), [](Job const& candidate)
                    {
                        return candidate.kind == Job::Kind::Render && candidate.entry
                            && candidate.entry->purpose == MathRenderPurpose::Preview;
                    });
                    if (selected == jobs.end()) selected = jobs.begin();
                    job = std::move(*selected);
                    jobs.erase(selected);
                    if (job.kind == Job::Kind::Render) activeRender = job.entry;
                    if (job.kind == Job::Kind::Read)
                    {
                        auto ref = svgRefs.find(job.renderId);
                        if (ref == svgRefs.end() || ref->second.generation != job.generation
                            || ref->second.entryId != job.entry->id
                            || !NeedsSvgLocked(job.renderId, ref->second))
                        {
                            if (ref != svgRefs.end()) ref->second.readQueued = false;
                            ++canceledReads;
                            if (pending) --pending;
                            ready.notify_all();
                            idle.notify_all();
                            continue;
                        }
                        readRange = ref->second.range;
                    }
                    workerBusy = true;
                }
            }
            if (compact)
            {
                if (store) MaybeCompact(*store);
                {
                    std::scoped_lock lock(mutex);
                    workerBusy = false;
                    idle.notify_all();
                }
                continue;
            }

            std::function<void()> callback;
            try
            {
                if (job.kind == Job::Kind::Read)
                    CompleteRead(job, readRange, store.get(), callback);
                else
                {
                    if (!session)
                    {
                        session = std::make_unique<NativeMathSession>();
                        std::scoped_lock lock(mutex);
                        nativeSessionActive = true;
                    }
                    if (job.kind == Job::Kind::Batch)
                        CompleteBatch(job, *session, *normalizer, store, callback);
                    else
                    {
                        { std::scoped_lock lock(mutex); ++nativeBatches; }
                        CompleteRender(job, *session, *normalizer, store, callback);
                    }
                }
            }
            catch (...)
            {
                // Allocation failures and other unexpected worker faults are
                // contained; they must not escape a detached thread.
                FailWorker();
                continue;
            }
            if (callback)
            {
                try { callback(); }
                catch (...) { /* Invalidation callbacks cannot terminate the worker. */ }
            }
        }

        // Thread-confined native state and delete-on-close storage end here.
        store.reset();
        session.reset();
        {
            std::scoped_lock lock(mutex);
            workerFinished = true;
            nativeSessionActive = false;
            workerBusy = false;
            ready.notify_all();
            idle.notify_all();
        }
    }

    void MathJaxRenderer::State::NormalizeResults(std::span<MathJaxSvg> results,
        std::span<Job const> items, SvgNormalizer& normalizer, std::stop_token stop)
    {
        std::vector<std::string> sources;
        std::vector<SvgNormalizationInput> inputs;
        std::vector<std::size_t> offsets{0};
        std::size_t count = 0;
        for (auto const& result : results) count += result.fragments.size();
        sources.reserve(count);
        inputs.reserve(count);
        offsets.reserve(results.size() + 1);
        for (std::size_t i = 0; i < results.size(); ++i)
        {
            for (auto const& fragment : results[i].fragments)
            {
                auto& colored = sources.emplace_back(fragment.svg ? *fragment.svg : std::string{});
                constexpr std::string_view marker = "currentColor";
                for (std::size_t at = 0; (at = colored.find(marker, at)) != std::string::npos;)
                {
                    colored.replace(at, marker.size(), items[i].color);
                    at += items[i].color.size();
                }
                inputs.push_back({colored, items[i].em,
                    stop.stop_possible() ? stop : items[i].entry->cancel.get_token()});
            }
            offsets.push_back(inputs.size());
        }
        auto normalized = normalizer.NormalizeBatchNow(inputs);
        for (std::size_t i = 0; i < results.size(); ++i)
        {
            auto& result = results[i];
            for (auto index = offsets[i]; index < offsets[i + 1]; ++index)
            {
                auto& value = normalized[index];
                if (!value)
                {
                    result.fragments.clear();
                    result.error = value.error;
                    result.errorKind = MathJaxErrorKind::Infrastructure;
                    break;
                }
                auto& fragment = result.fragments[index - offsets[i]];
                fragment.renderId = value.id;
                fragment.svg = std::move(value.svg);
            }
        }
    }

    void MathJaxRenderer::State::CompleteRender(Job const& job,
        NativeMathSession& session, SvgNormalizer& normalizer,
        std::unique_ptr<MathSvgStore>& store, std::function<void()>& callback, MathJaxSvg* prepared)
    {
        auto const& entry = job.entry;
        std::vector<std::shared_ptr<Entry>> cancelAfterUnlock;
        std::vector<MathSvgStore::Range> written;
        std::shared_ptr<MathJaxSvg const> full;
        std::shared_ptr<MathJaxSvg const> cached;
        bool canceled = false;
        try
        {
            std::stop_callback externalStop(job.callerStop, [entry]
                { entry->cancel.request_stop(); });
            auto stop = entry->cancel.get_token();
            if (stop.stop_requested()) throw winrt::hresult_canceled();
            auto result = prepared ? std::move(*prepared)
                : session.Render(job.tex, job.display, job.em, job.containerWidth, stop);
            {
                std::scoped_lock lock(mutex);
                ++nativeRenders;
            }
            if (stop.stop_requested()) throw winrt::hresult_canceled();
            if (!prepared) NormalizeResults(std::span{&result, std::size_t{1}},
                std::span{&job, std::size_t{1}}, normalizer, stop);
            if (stop.stop_requested()) throw winrt::hresult_canceled();
            if (entry->purpose == MathRenderPurpose::Document
                && result.errorKind != MathJaxErrorKind::Infrastructure
                && !result.fragments.empty())
            {
                written.reserve(result.fragments.size());
                if (!store)
                {
                    store = std::make_unique<MathSvgStore>();
                }
                for (auto const& fragment : result.fragments)
                {
                    if (stop.stop_requested()) throw winrt::hresult_canceled();
                    written.push_back(store->Append(*fragment.svg));
                    std::scoped_lock lock(mutex);
                    ++fileWrites;
                    backingPath = store->Path();
                    lastIoThread = std::this_thread::get_id();
                }
                auto geometry = std::make_shared<MathJaxSvg>(result);
                for (auto& fragment : geometry->fragments) fragment.svg.reset();
                cached = std::move(geometry);
                full = std::make_shared<MathJaxSvg const>(std::move(result));
            }
            else
            {
                full = std::make_shared<MathJaxSvg const>(std::move(result));
                cached = full;
            }
        }
        catch (winrt::hresult_canceled const&)
        {
            canceled = true;
        }
        catch (std::exception const& error)
        {
            auto failed = std::make_shared<MathJaxSvg const>(InfrastructureFailure(error.what(), job.display));
            full = failed;
            cached = std::move(failed);
        }
        catch (...)
        {
            auto failed = std::make_shared<MathJaxSvg const>(
                InfrastructureFailure("Unknown native math worker failure", job.display));
            full = failed;
            cached = std::move(failed);
        }

        {
            std::scoped_lock lock(mutex);
            auto current = index.find(entry->key);
            bool valid = !stopping && current != index.end()
                && current->second->id == entry->id && !entry->cancel.stop_requested();
            if (valid && !canceled && cached)
            {
                if (entry->purpose == MathRenderPurpose::Document
                    && full->errorKind != MathJaxErrorKind::Infrastructure)
                {
                    for (std::size_t i = 0; i < full->fragments.size(); ++i)
                    {
                        auto const id = full->fragments[i].renderId;
                        SvgRef ref;
                        ref.entryId = entry->id;
                        ref.generation = nextSvgGeneration++;
                        ref.range = written.at(i);
                        auto [inserted, ok] = svgRefs.emplace(id, std::move(ref));
                        if (!ok) throw std::logic_error("Duplicate normalized SVG render id");
                        entry->fragmentIds.push_back(id);
                        CacheDocumentSvgLocked(id, full->fragments[i].svg);
                        AddSaturated(writtenBytes, written[i].size);
                    }
                }
                else if (entry->purpose == MathRenderPurpose::Preview
                    && full->errorKind != MathJaxErrorKind::Infrastructure)
                {
                    for (auto const& fragment : full->fragments)
                    {
                        SvgRef ref;
                        ref.entryId = entry->id;
                        ref.generation = nextSvgGeneration++;
                        ref.preview = fragment.svg;
                        auto [inserted, ok] = svgRefs.emplace(fragment.renderId, std::move(ref));
                        if (!ok) throw std::logic_error("Duplicate normalized SVG render id");
                        entry->fragmentIds.push_back(fragment.renderId);
                        if (fragment.svg) previewSvgBytes += fragment.svg->size();
                    }
                }
                else if (!written.empty())
                {
                    for (auto const& range : written)
                    {
                        AddSaturated(writtenBytes, range.size);
                        AddSaturated(deadBytes, range.size);
                    }
                    compactionRequested = true;
                }
                entry->result = std::move(cached);
                entry->pending = false;
                callback = completion;
            }
            else
            {
                for (auto const& range : written)
                {
                    AddSaturated(writtenBytes, range.size);
                    AddSaturated(deadBytes, range.size);
                }
                if (!written.empty()) compactionRequested = true;
                if (!entry->removed && current != index.end() && current->second->id == entry->id)
                    RemoveEntryLocked(entry, cancelAfterUnlock);
                entry->pending = false;
                // Canceled/removed work publishes no new visible state. A
                // completion here would refresh and resubmit offscreen work.
            }
            if (pending) --pending;
            activeRender.reset();
            workerBusy = false;
            ready.notify_all();
            idle.notify_all();
            if (compactionRequested) wake.notify_one();
        }
        for (auto const& removed : cancelAfterUnlock) removed->cancel.request_stop();
    }

    void MathJaxRenderer::State::CompleteRead(Job const& job, MathSvgStore::Range range,
        MathSvgStore* store, std::function<void()>& callback)
    {
        std::shared_ptr<std::string const> bytes;
        bool attemptedRead = false;
        if (store)
        {
            attemptedRead = true;
            try { bytes = store->Read(range); }
            catch (...) { }
        }
        {
            std::scoped_lock lock(mutex);
            if (attemptedRead)
            {
                ++fileReads;
                lastIoThread = std::this_thread::get_id();
            }
            auto ref = svgRefs.find(job.renderId);
            if (!stopping && ref != svgRefs.end()
                && ref->second.generation == job.generation
                && ref->second.entryId == job.entry->id)
            {
                ref->second.readQueued = false;
                if (!NeedsSvgLocked(job.renderId, ref->second))
                {
                    // An already-started file read can finish after a scroll.
                    // Drop it without retaining bytes or scheduling a repaint.
                    ++discardedReads;
                }
                else if (bytes)
                {
                    CacheDocumentSvgLocked(job.renderId, std::move(bytes));
                    callback = completion;
                }
                else
                {
                    ref->second.failed = true;
                    if (auto entry = entriesById.find(ref->second.entryId); entry != entriesById.end())
                        entry->second->result = std::make_shared<MathJaxSvg const>(
                            InfrastructureFailure("Cached math SVG read failed", entry->second->result
                                ? entry->second->result->display : false));
                    callback = completion;
                }
            }
            if (pending) --pending;
            workerBusy = false;
            ready.notify_all();
            idle.notify_all();
        }
    }

    void MathJaxRenderer::State::MaybeCompact(MathSvgStore& store) noexcept
    {
        struct Copy
        {
            std::uint64_t id;
            std::uint64_t generation;
            MathSvgStore::Range oldRange;
            MathSvgStore::Range newRange;
        };
        try
        {
            std::vector<Copy> copies;
            {
                std::scoped_lock lock(mutex);
                if (deadBytes < 8ull * 1024 * 1024 || deadBytes <= writtenBytes / 2)
                {
                    compactionRequested = false;
                    return;
                }
                compactionRequested = false;
                copies.reserve(svgRefs.size());
                for (auto const& [id, ref] : svgRefs)
                    if (ref.range.size != 0) copies.push_back({ id, ref.generation, ref.range, {} });
            }

            MathSvgStore replacement;
            std::uint64_t copiedBytes = 0;
            for (auto& copy : copies)
            {
                auto bytes = store.Read(copy.oldRange);
                {
                    std::scoped_lock lock(mutex);
                    ++fileReads;
                    lastIoThread = std::this_thread::get_id();
                }
                copy.newRange = replacement.Append(*bytes);
                {
                    std::scoped_lock lock(mutex);
                    ++fileWrites;
                    lastIoThread = std::this_thread::get_id();
                }
                AddSaturated(copiedBytes, copy.newRange.size);
            }
            std::uint64_t staleCopiedBytes = 0;
            {
                std::scoped_lock lock(mutex);
                for (auto const& copy : copies)
                {
                    auto found = svgRefs.find(copy.id);
                    if (found != svgRefs.end() && found->second.generation == copy.generation)
                        found->second.range = copy.newRange;
                    else
                        AddSaturated(staleCopiedBytes, copy.newRange.size);
                }
                writtenBytes = copiedBytes;
                deadBytes = staleCopiedBytes;
                compactionRequested = deadBytes >= 8ull * 1024 * 1024 && deadBytes > writtenBytes / 2;
            }
            store = std::move(replacement);
            {
                std::scoped_lock lock(mutex);
                backingPath = store.Path();
            }
        }
        catch (...)
        {
            // Failed compaction leaves the original store and ranges usable.
        }
    }

    void MathJaxRenderer::State::FailQueuedInitialization() noexcept
    {
        auto failure = std::current_exception();
        std::function<void()> callback;
        {
            std::scoped_lock lock(mutex);
            for (auto& job : jobs)
            {
                if (!job.entry) continue;
                try
                {
                    job.entry->result = std::make_shared<MathJaxSvg const>(
                        InfrastructureFailure("Math SVG worker could not initialize", job.display));
                }
                catch (...)
                {
                    job.entry->removed = true;
                }
                job.entry->pending = false;
            }
            pending = 0;
            jobs.clear();
            workerFinished = true;
            workerFailure = failure;
            callback = std::move(completion);
        }
        ready.notify_all();
        idle.notify_all();
        if (callback)
        {
            try { callback(); }
            catch (...) { }
        }
    }

    void MathJaxRenderer::State::FailWorker() noexcept
    {
        auto failure = std::current_exception();
        std::function<void()> callback;
        {
            std::scoped_lock lock(mutex);
            stopping = true;
            for (auto& [key, entry] : index)
            {
                entry->removed = true;
                entry->pending = false;
            }
            index.clear();
            entriesById.clear();
            svgRefs.clear();
            visibleSvgIds.clear();
            residentSvgIds.clear();
            jobs.clear();
            residentSvgBytes = 0;
            previewSvgBytes = 0;
            pending = 0;
            workerBusy = false;
            workerFinished = true;
            workerFailure = failure;
            callback = std::move(completion);
        }
        ready.notify_all();
        idle.notify_all();
        wake.notify_all();
        if (callback)
        {
            try { callback(); }
            catch (...) { }
        }
    }
}
