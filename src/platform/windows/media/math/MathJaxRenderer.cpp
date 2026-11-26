#include <winrt/base.h>
#include "windows/media/math/MathJaxRenderer.h"
#include "windows/media/math/MathJaxRenderer.State.h"

import std;

namespace folia::platform
{
    MathJaxRenderer::MathJaxRenderer() : state(std::make_shared<State>()) {}

    MathJaxRenderer::~MathJaxRenderer()
    {
        auto old = CurrentState();
        if (old) old->Stop();
    }

    MathJaxRenderer::MathJaxRenderer(MathJaxRenderer&& other) noexcept
    {
        std::scoped_lock lock(other.stateMutex);
        state = std::move(other.state);
    }

    MathJaxRenderer& MathJaxRenderer::operator=(MathJaxRenderer&& other) noexcept
    {
        if (this != &other)
        {
            std::shared_ptr<State> old;
            {
                std::scoped_lock lock(stateMutex, other.stateMutex);
                old = std::move(state);
                state = std::move(other.state);
            }
            if (old) old->Stop();
        }
        return *this;
    }

    std::shared_ptr<MathJaxRenderer::State> MathJaxRenderer::CurrentState() const
    {
        std::scoped_lock lock(stateMutex);
        return state;
    }

    std::shared_ptr<MathJaxSvg const> MathJaxRenderer::Render(
        std::string_view tex, bool display, float em, float containerWidth,
        std::string_view color, std::stop_token stop, MathRenderPurpose purpose)
    {
        auto current = CurrentState();
        if (!current) throw winrt::hresult_canceled();
        auto entry = current->Request(tex, display, em, containerWidth, color, stop, purpose);
        std::scoped_lock lock(current->mutex);
        return entry->result;
    }

    std::shared_ptr<MathJaxSvg const> MathJaxRenderer::Lookup(
        std::string_view tex, bool display, float em, float containerWidth,
        std::string_view color, MathRenderPurpose purpose)
    {
        auto current = CurrentState();
        if (!current) return {};
        std::scoped_lock lock(current->mutex);
        auto found = current->index.find(State::MakeKey(tex, display, em, color, purpose));
        if (found == current->index.end()) return {};
        auto entry = State::FindCompatible(found->second, containerWidth);
        if (!entry) return {};
        if (current->activeOwner) current->blockEntries.insert(entry->id);
        return entry->result;
    }

    MathJaxLayoutResult MathJaxRenderer::Layout(std::string_view tex, bool display, float em,
        float width, std::string_view color, bool visible, MathRenderPurpose purpose)
    {
        auto current = CurrentState();
        if (!current) return {};
        auto requested = visible ? current->Request(tex, display, em, width, color, {}, purpose, nullptr, true) : nullptr;
        std::scoped_lock lock(current->mutex);
        auto found = requested ? current->index.find(requested->key)
            : current->index.find(State::MakeKey(tex, display, em, color, purpose));
        if (found == current->index.end()) return {{}, true};
        auto desired = State::FindCompatible(found->second, width);
        auto retain = [&](const std::shared_ptr<State::Entry>& entry, bool pending) {
            if (current->activeOwner) current->blockEntries.insert(entry->id);
            return MathJaxLayoutResult{entry->result, pending};
        };
        if (desired && desired->result && (!visible || current->ReadyToDraw(*desired)
            || desired->result->errorKind == MathJaxErrorKind::Infrastructure)) return retain(desired, false);

        std::shared_ptr<State::Entry> drawn;
        for (auto const& variant : found->second | std::views::values)
            if (variant != desired && current->ReadyToDraw(*variant)
                && (!drawn || std::abs(variant->width - width) < std::abs(drawn->width - width))) drawn = variant;
        if (!drawn) return desired ? retain(desired, !desired->result) : MathJaxLayoutResult{{}, true};
        if (auto replacement = desired ? desired : requested; visible && replacement)
        {
            // Keep drawing the old SVG until the replacement payload arrives.
            // The replacement may still be rendering or already on disk.
            if (current->activeOwner) current->blockEntries.insert(replacement->id);
            replacement->viewportDemand = current->viewportGeneration + 1;
            for (auto id : replacement->fragmentIds)
                if (auto svg = current->svgRefs.find(id); svg != current->svgRefs.end() && !svg->second.resident)
                    current->QueueReadLocked(id, svg->second);
        }
        return retain(drawn, true);
    }

    std::shared_ptr<MathJaxSvg const> MathJaxRenderer::Prepare(
        std::string_view tex, bool display, float em, float containerWidth,
        std::string_view color, std::stop_token stop, MathRenderPurpose purpose)
    {
        auto current = CurrentState();
        if (!current) throw winrt::hresult_canceled();
        if (!stop.stop_possible())
        {
            std::scoped_lock lock(current->mutex);
            stop = current->preparationStop;
        }
        auto entry = current->Request(tex, display, em, containerWidth, color, stop, purpose);
        {
            std::unique_lock lock(current->mutex);
            if (!current->ready.wait(lock, stop, [&]
                { return !entry->pending || current->stopping || entry->removed || current->workerFinished; }))
                throw winrt::hresult_canceled();
            if (stop.stop_requested() || entry->removed || current->stopping)
                throw winrt::hresult_canceled();
            if (!entry->result) throw winrt::hresult_canceled();
            if (entry->result->errorKind == MathJaxErrorKind::Infrastructure
                || purpose == MathRenderPurpose::Preview)
                return entry->result;
        }
        return current->AwaitFull(entry, stop);
    }

    std::shared_ptr<std::string const> MathJaxRenderer::GetSvg(std::uint64_t renderId)
    {
        auto current = CurrentState();
        return current ? current->GetSvg(renderId) : std::shared_ptr<std::string const>{};
    }

    void MathJaxRenderer::SetVisibleSvgIds(std::span<std::uint64_t const> ids)
    {
        auto current = CurrentState();
        if (current) current->SetVisibleSvgIds(ids);
    }

    void MathJaxRenderer::CancelOffscreenRenders(std::span<std::uint64_t const> visibleBlocks)
    {
        auto current = CurrentState();
        if (!current) return;
        std::unordered_set<std::uint64_t> visible(visibleBlocks.begin(), visibleBlocks.end());
        std::vector<std::shared_ptr<State::Entry>> canceled;
        {
            std::scoped_lock lock(current->mutex);
            std::vector<std::shared_ptr<State::Entry>> obsolete;
            auto consider = [&](auto const& entry)
            {
                if (!entry) return;
                if (entry->pending && !entry->unowned
                    && std::ranges::none_of(entry->owners,
                        [&](auto owner) { return visible.contains(owner); }))
                    obsolete.push_back(entry);
            };
            consider(current->activeRender);
            for (auto const& job : current->jobs)
                if (job.kind == State::Job::Kind::Render) consider(job.entry);
            for (auto const& entry : obsolete) current->RemoveEntryLocked(entry, canceled);
        }
        for (auto const& entry : canceled) entry->cancel.request_stop();
    }

    void MathJaxRenderer::WaitForIdle(std::stop_token stop)
    {
        auto current = CurrentState();
        if (!current) return;
        std::unique_lock lock(current->mutex);
        if (!current->idle.wait(lock, stop, [&]
            { return (current->pending == 0 && !current->workerBusy) || current->workerFinished; }))
            throw winrt::hresult_canceled();
        if (stop.stop_requested()) throw winrt::hresult_canceled();
        if (current->workerFailure) std::rethrow_exception(current->workerFailure);
    }

    void MathJaxRenderer::SetCompletionCallback(std::function<void()> callback)
    {
        auto current = CurrentState();
        if (!current) return;
        std::scoped_lock lock(current->mutex);
        if (!current->stopping && !current->workerFinished)
            current->completion = std::move(callback);
    }

    MathJaxRenderer::Statistics MathJaxRenderer::Snapshot() const
    {
        Statistics result;
        auto current = CurrentState();
        if (!current) return result;
        std::scoped_lock lock(current->mutex);
        result.entries = current->entriesById.size();
        result.svgBytes = current->residentSvgBytes + current->previewSvgBytes;
        result.nativeRenders = current->nativeRenders;
        result.nativeBatches = current->nativeBatches;
        result.pending = current->pending;
        result.fileReads = current->fileReads;
        result.fileWrites = current->fileWrites;
        result.residentSvgBytes = current->residentSvgBytes;
        result.previewSvgBytes = current->previewSvgBytes;
        result.queuedReads = static_cast<std::size_t>(std::ranges::count_if(current->jobs,
            [](auto const& job) { return job.kind == State::Job::Kind::Read; }));
        result.canceledReads = current->canceledReads;
        result.discardedReads = current->discardedReads;
        result.nativeSessionActive = current->nativeSessionActive;
        result.backingPath = current->backingPath;
        result.lastIoThread = current->lastIoThread;
        return result;
    }

    void MathJaxRenderer::SetPreparationStopToken(std::stop_token stop)
    {
        auto current = CurrentState();
        if (!current) return;
        std::scoped_lock lock(current->mutex);
        current->preparationStop = stop;
    }

    void MathJaxRenderer::BeginBlock(std::uint64_t owner)
    {
        auto current = CurrentState();
        if (!current) return;
        std::vector<std::shared_ptr<State::Entry>> cancelAfterUnlock;
        {
            std::scoped_lock lock(current->mutex);
            auto previous = std::move(current->blockEntries);
            current->blockEntries.clear();
            for (auto id : previous) current->RemoveIfOrphanLocked(id, cancelAfterUnlock);
            current->activeOwner = owner;
        }
        for (auto const& entry : cancelAfterUnlock) entry->cancel.request_stop();
    }

    void MathJaxRenderer::EndBlock()
    {
        auto current = CurrentState();
        if (!current) return;
        std::vector<std::shared_ptr<State::Entry>> cancelAfterUnlock;
        {
            std::scoped_lock lock(current->mutex);
            current->FinishBlock(cancelAfterUnlock);
        }
        for (auto const& entry : cancelAfterUnlock) entry->cancel.request_stop();
    }

    void MathJaxRenderer::RetainBlocks(std::span<std::uint64_t const> owners)
    {
        auto current = CurrentState();
        if (!current) return;
        std::vector<std::shared_ptr<State::Entry>> cancelAfterUnlock;
        {
            std::scoped_lock lock(current->mutex);
            current->Retain(owners, cancelAfterUnlock);
        }
        for (auto const& entry : cancelAfterUnlock) entry->cancel.request_stop();
    }

    void MathJaxRenderer::Clear()
    {
        auto replacement = std::make_shared<State>();
        std::shared_ptr<State> old;
        {
            std::scoped_lock lock(stateMutex);
            old = std::move(state);
            state = std::move(replacement);
        }
        if (old) old->Stop();
    }
}
