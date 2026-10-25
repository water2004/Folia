#include "pch.h"

import std;
#include "media/MathJaxRenderer.State.h"
#include "media/SvgNormalizer.h"

namespace winrt::Folia
{
    void MathJaxRenderer::State::StartWorker()
    {
        auto self = shared_from_this();
        std::thread([self = std::move(self)]
        {
            try { self->Run(); }
            catch (...) { self->FailWorker(); }
        }).detach();
    }

    std::string MathJaxRenderer::State::MakeKey(std::string_view tex, bool display,
        float em, float width, std::string_view color, MathRenderPurpose purpose)
    {
        std::string key = std::to_string(tex.size()) + ':' + std::string(tex);
        key.push_back(display ? '1' : '0');
        key.append(reinterpret_cast<char const*>(&em), sizeof(em));
        key.append(reinterpret_cast<char const*>(&width), sizeof(width));
        key.push_back(purpose == MathRenderPurpose::Preview ? 'P' : 'D');
        key.append(color);
        return key;
    }

    std::shared_ptr<MathJaxRenderer::State::Entry> MathJaxRenderer::State::Request(
        std::string_view tex, bool display, float em, float width,
        std::string_view color, std::stop_token stop, MathRenderPurpose purpose, std::vector<Job>* batch)
    {
        if (stop.stop_requested()) throw winrt::hresult_canceled();
        if (purpose != MathRenderPurpose::Document && purpose != MathRenderPurpose::Preview)
            throw std::invalid_argument("Unknown math render purpose");
        std::unique_lock lock(mutex);
        if (stopping || workerFinished) throw winrt::hresult_canceled();
        if (!stop.stop_possible()) stop = preparationStop;
        if (stop.stop_requested()) throw winrt::hresult_canceled();
        auto key = MakeKey(tex, display, em, width, color, purpose);
        if (stop.stop_requested()) throw winrt::hresult_canceled();

        if (auto found = index.find(key); found != index.end())
        {
            auto entry = found->second;
            if (activeOwner) blockEntries.insert(entry->id);
            return entry;
        }

        // Acquire the runtime lease on the requester's thread before the
        // detached native worker can outlive application teardown.
        if (!normalizerLease) normalizerLease = std::make_shared<SvgNormalizer>();

        auto entry = std::make_shared<Entry>();
        entry->id = nextEntryId++;
        if (entry->id == 0) entry->id = nextEntryId++;
        entry->key = key;
        entry->purpose = purpose;
        entry->unowned = !activeOwner.has_value();
        bool inIndex = false;
        bool byId = false;
        bool inBlock = false;
        try
        {
            inIndex = index.emplace(key, entry).second;
            if (!inIndex)
            {
                auto existing = index.at(key);
                if (activeOwner) blockEntries.insert(existing->id);
                return existing;
            }
            byId = entriesById.emplace(entry->id, entry).second;
            if (!byId) throw std::logic_error("Duplicate MathJax cache entry id");
            if (activeOwner) inBlock = blockEntries.insert(entry->id).second;
            Job job;
            job.kind = Job::Kind::Render;
            job.entry = entry;
            job.tex.assign(tex);
            job.color.assign(color);
            job.display = display;
            job.em = em;
            job.containerWidth = width;
            job.callerStop = stop;
            if (batch) batch->push_back(std::move(job));
            else jobs.push_back(std::move(job));
        }
        catch (...)
        {
            if (inBlock) blockEntries.erase(entry->id);
            if (byId) entriesById.erase(entry->id);
            if (inIndex) index.erase(key);
            throw;
        }
        ++pending;
        if (batch) return entry;
        bool startWorker = !workerStarted;
        if (startWorker) workerStarted = true;
        wake.notify_one();
        lock.unlock();
        if (startWorker)
        {
            try { StartWorker(); }
            catch (...)
            {
                FailWorker();
                throw;
            }
        }
        return entry;
    }

    MathJaxSvg MathJaxRenderer::State::InfrastructureFailure(std::string message, bool display)
    {
        MathJaxSvg result;
        result.display = display;
        result.error = std::move(message);
        result.errorKind = MathJaxErrorKind::Infrastructure;
        return result;
    }

    void MathJaxRenderer::State::AddSaturated(std::uint64_t& value, std::uint64_t add) noexcept
    {
        auto const maximum = (std::numeric_limits<std::uint64_t>::max)();
        value = add > maximum - value ? maximum : value + add;
    }

    void MathJaxRenderer::State::RemoveSvgRefLocked(std::uint64_t id, std::uint64_t ownerEntry)
    {
        auto found = svgRefs.find(id);
        if (found == svgRefs.end() || found->second.entryId != ownerEntry) return;
        auto& ref = found->second;
        if (ref.range.size != 0)
        {
            AddSaturated(deadBytes, ref.range.size);
            compactionRequested = true;
        }
        visibleSvgIds.erase(id);
        residentSvgIds.erase(id);
        if (ref.resident) residentSvgBytes -= ref.resident->size();
        if (ref.preview) previewSvgBytes -= ref.preview->size();
        svgRefs.erase(found);
    }

    void MathJaxRenderer::State::RemoveEntryLocked(std::shared_ptr<Entry> const& entry,
        std::vector<std::shared_ptr<Entry>>& cancelAfterUnlock)
    {
        cancelAfterUnlock.push_back(entry);
        entry->removed = true;
        auto found = index.find(entry->key);
        if (found != index.end() && found->second->id == entry->id) index.erase(found);
        entriesById.erase(entry->id);
        for (auto owner : entry->owners)
            if (auto membership = ownerEntries.find(owner); membership != ownerEntries.end())
            {
                membership->second.erase(entry->id);
                if (membership->second.empty()) ownerEntries.erase(membership);
            }
        entry->owners.clear();
        for (auto id : entry->fragmentIds) RemoveSvgRefLocked(id, entry->id);
        entry->fragmentIds.clear();
        entry->result.reset();

        for (auto it = jobs.begin(); it != jobs.end();)
        {
            if (it->entry && it->entry->id == entry->id)
            {
                it = jobs.erase(it);
                if (pending) --pending;
            }
            else ++it;
        }
        // Active jobs stay counted until their worker completion path runs.
        entry->pending = false;
        ready.notify_all();
        idle.notify_all();
        wake.notify_one();
    }

    void MathJaxRenderer::State::RemoveIfOrphanLocked(std::uint64_t id,
        std::vector<std::shared_ptr<Entry>>& cancelAfterUnlock)
    {
        if (blockEntries.contains(id)) return;
        auto found = entriesById.find(id);
        if (found == entriesById.end()) return;
        auto entry = found->second;
        if (!entry->unowned && entry->owners.empty()) RemoveEntryLocked(entry, cancelAfterUnlock);
    }

    void MathJaxRenderer::State::FinishBlock(std::vector<std::shared_ptr<Entry>>& cancelAfterUnlock)
    {
        if (!activeOwner) throw std::logic_error("EndBlock called without BeginBlock");
        auto owner = *activeOwner;
        std::erase_if(blockEntries, [&](std::uint64_t id) { return !entriesById.contains(id); });
        auto& owned = ownerEntries[owner];
        if (owned == blockEntries)
        {
            blockEntries.clear();
            activeOwner.reset();
            if (owned.empty()) ownerEntries.erase(owner);
            return;
        }
        auto previous = std::move(owned);
        for (auto id : previous)
        {
            if (blockEntries.contains(id)) continue;
            if (auto found = entriesById.find(id); found != entriesById.end())
                found->second->owners.erase(owner);
        }
        for (auto id : blockEntries)
            if (auto found = entriesById.find(id); found != entriesById.end())
                found->second->owners.insert(owner);
        ownerEntries[owner] = std::move(blockEntries);
        if (ownerEntries[owner].empty()) ownerEntries.erase(owner);
        blockEntries.clear();
        activeOwner.reset();
        // Every newly acquired entry now has this owner. Only old membership
        // can have lost its last owner; do not copy/union both sets to scan it.
        for (auto id : previous) RemoveIfOrphanLocked(id, cancelAfterUnlock);
    }

    void MathJaxRenderer::State::Retain(std::span<std::uint64_t const> owners,
        std::vector<std::shared_ptr<Entry>>& cancelAfterUnlock)
    {
        std::unordered_set<std::uint64_t> retained(owners.begin(), owners.end());
        std::unordered_set<std::uint64_t> affected;
        for (auto it = ownerEntries.begin(); it != ownerEntries.end();)
        {
            if (retained.contains(it->first))
            {
                ++it;
                continue;
            }
            for (auto id : it->second)
            {
                affected.insert(id);
                if (auto found = entriesById.find(id); found != entriesById.end())
                    found->second->owners.erase(it->first);
            }
            it = ownerEntries.erase(it);
        }
        for (auto id : affected) RemoveIfOrphanLocked(id, cancelAfterUnlock);
    }

    void MathJaxRenderer::State::QueueReadLocked(std::uint64_t id, SvgRef& ref)
    {
        if (ref.readQueued || ref.failed || stopping || !NeedsSvgLocked(id, ref)) return;
        auto entry = entriesById.find(ref.entryId);
        if (entry == entriesById.end()) return;
        Job job;
        job.kind = Job::Kind::Read;
        job.entry = entry->second;
        job.renderId = id;
        job.generation = ref.generation;
        jobs.push_back(std::move(job));
        ref.readQueued = true;
        ++pending;
        wake.notify_one();
    }

    std::shared_ptr<std::string const> MathJaxRenderer::State::GetSvg(std::uint64_t id)
    {
        std::scoped_lock lock(mutex);
        auto found = svgRefs.find(id);
        if (found == svgRefs.end() || stopping) return {};
        auto& ref = found->second;
        if (ref.preview) return ref.preview;
        if (ref.resident)
        {
            return ref.resident;
        }
        return {};
    }

    bool MathJaxRenderer::State::NeedsSvgLocked(std::uint64_t id, SvgRef const& ref) const
    {
        return ref.fullReaders != 0 || visibleSvgIds.contains(id);
    }

    void MathJaxRenderer::State::ReleaseUnusedSvgLocked()
    {
        for (auto it = jobs.begin(); it != jobs.end();)
        {
            if (it->kind != Job::Kind::Read) { ++it; continue; }
            auto ref = svgRefs.find(it->renderId);
            if (ref != svgRefs.end() && NeedsSvgLocked(it->renderId, ref->second))
            {
                ++it;
                continue;
            }
            if (ref != svgRefs.end()) ref->second.readQueued = false;
            it = jobs.erase(it);
            if (pending) --pending;
            ++canceledReads;
        }
        for (auto it = residentSvgIds.begin(); it != residentSvgIds.end();)
        {
            auto ref = svgRefs.find(*it);
            if (ref != svgRefs.end() && NeedsSvgLocked(*it, ref->second))
            {
                ++it;
                continue;
            }
            if (ref != svgRefs.end() && ref->second.resident)
            {
                residentSvgBytes -= ref->second.resident->size();
                ref->second.resident.reset();
            }
            it = residentSvgIds.erase(it);
        }
        idle.notify_all();
    }

    void MathJaxRenderer::State::SetVisibleSvgIds(std::span<std::uint64_t const> ids)
    {
        std::scoped_lock lock(mutex);
        if (stopping) return;
        std::unordered_set<std::uint64_t> wanted;
        wanted.reserve(ids.size());
        for (auto id : ids)
            if (auto ref = svgRefs.find(id); ref != svgRefs.end() && !ref->second.preview)
                wanted.insert(id);
        visibleSvgIds.swap(wanted);
        ReleaseUnusedSvgLocked();
        for (auto id : visibleSvgIds)
            if (auto ref = svgRefs.find(id); ref != svgRefs.end() && !ref->second.resident)
                QueueReadLocked(id, ref->second);
    }

    bool MathJaxRenderer::State::HasReadyChangeLocked(Entry const& entry,
        std::vector<std::shared_ptr<std::string const>> const& payloads) const
    {
        for (std::size_t i = 0; i < entry.fragmentIds.size() && i < payloads.size(); ++i)
        {
            if (payloads[i]) continue;
            auto found = svgRefs.find(entry.fragmentIds[i]);
            if (found == svgRefs.end() || found->second.failed || found->second.resident)
                return true;
        }
        return false;
    }

    std::shared_ptr<MathJaxSvg const> MathJaxRenderer::State::AwaitFull(
        std::shared_ptr<Entry> const& entry, std::stop_token stop)
    {
        std::unique_lock lock(mutex);
        std::vector<std::shared_ptr<std::string const>> payloads;
        std::vector<std::uint64_t> readers;
        // Prepare is an explicit background consumer, independent of viewport
        // cancellation. Its temporary payload ownership ends with this call.
        auto release = [&](void*)
        {
            for (auto id : readers)
                if (auto ref = svgRefs.find(id); ref != svgRefs.end())
                    --ref->second.fullReaders;
            ReleaseUnusedSvgLocked();
        };
        std::unique_ptr<void, decltype(release)> lease(this, release);
        bool initialized = false;
        for (;;)
        {
            if (stop.stop_requested() || stopping || entry->removed)
                throw winrt::hresult_canceled();
            if (entry->pending)
            {
                if (!ready.wait(lock, stop, [&]
                    { return !entry->pending || stopping || entry->removed || workerFinished; }))
                    throw winrt::hresult_canceled();
                continue;
            }
            if (!entry->result) throw winrt::hresult_canceled();
            if (entry->result->errorKind == MathJaxErrorKind::Infrastructure
                || entry->purpose == MathRenderPurpose::Preview
                || entry->result->fragments.empty())
                return entry->result;
            if (!initialized)
            {
                payloads.resize(entry->result->fragments.size());
                readers.reserve(entry->fragmentIds.size());
                for (auto id : entry->fragmentIds)
                    if (auto ref = svgRefs.find(id); ref != svgRefs.end())
                    {
                        readers.push_back(id);
                        ++ref->second.fullReaders;
                    }
                initialized = true;
            }

            bool waiting = false;
            bool failed = false;
            for (std::size_t i = 0; i < entry->result->fragments.size(); ++i)
            {
                if (payloads[i]) continue;
                auto id = entry->result->fragments[i].renderId;
                auto ref = svgRefs.find(id);
                if (ref == svgRefs.end() || ref->second.entryId != entry->id || ref->second.failed)
                {
                    failed = true;
                    break;
                }
                if (ref->second.resident)
                {
                    payloads[i] = ref->second.resident;
                }
                else
                {
                    QueueReadLocked(id, ref->second);
                    waiting = true;
                }
            }
            if (failed)
            {
                auto failure = std::make_shared<MathJaxSvg const>(
                    InfrastructureFailure("A cached math SVG could not be read", entry->result->display));
                entry->result = std::move(failure);
                return entry->result;
            }
            bool complete = std::ranges::all_of(payloads, [](auto const& value) { return static_cast<bool>(value); });
            if (complete)
            {
                auto full = std::make_shared<MathJaxSvg>(*entry->result);
                for (std::size_t i = 0; i < payloads.size(); ++i)
                    full->fragments[i].svg = std::move(payloads[i]);
                return full;
            }
            if (waiting && !ready.wait(lock, stop, [&]
                { return stopping || entry->removed || HasReadyChangeLocked(*entry, payloads); }))
                throw winrt::hresult_canceled();
        }
    }

    void MathJaxRenderer::State::Stop() noexcept
    {
        std::unordered_map<std::string, std::shared_ptr<Entry>> oldEntries;
        {
            std::scoped_lock lock(mutex);
            if (stopping) return;
            stopping = true;
            completion = {};
            for (auto& [key, entry] : index)
            {
                entry->removed = true;
                entry->pending = false;
            }
            oldEntries.swap(index);
            entriesById.clear();
            svgRefs.clear();
            visibleSvgIds.clear();
            residentSvgIds.clear();
            jobs.clear();
            pending = 0;
            residentSvgBytes = 0;
            previewSvgBytes = 0;
        }
        for (auto& [key, entry] : oldEntries) entry->cancel.request_stop();
        wake.notify_all();
        ready.notify_all();
        idle.notify_all();
    }
}
