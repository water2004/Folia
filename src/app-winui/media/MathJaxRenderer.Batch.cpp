#include "pch.h"

import std;
#include "media/MathJaxRenderer.State.h"
#include "media/NativeMathSession.h"
#include "media/SvgNormalizer.h"

namespace winrt::Folia
{
    struct MathJaxRenderer::Batch::Impl
    {
        std::shared_ptr<State> state;
        std::vector<State::Job> items;

        ~Impl()
        {
            if (items.empty()) return;
            try
            {
                std::vector<std::shared_ptr<State::Entry>> canceled;
                canceled.reserve(items.size());
                {
                    std::scoped_lock lock(state->mutex);
                    for (auto const& job : items)
                    {
                        if (!job.entry->pending || !state->entriesById.contains(job.entry->id)) continue;
                        state->RemoveEntryLocked(job.entry, canceled);
                        // These jobs were staged, so removal did not find them in the queue.
                        if (state->pending) --state->pending;
                    }
                    state->idle.notify_all();
                }
                for (auto const& entry : canceled) entry->cancel.request_stop();
            }
            catch (...) { state->Stop(); }
        }
    };

    MathJaxRenderer::Batch::Batch(std::shared_ptr<State> state)
        : impl(std::make_unique<Impl>())
    {
        impl->state = std::move(state);
    }
    MathJaxRenderer::Batch::~Batch() = default;
    MathJaxRenderer::Batch::Batch(Batch&&) noexcept = default;
    MathJaxRenderer::Batch& MathJaxRenderer::Batch::operator=(Batch&&) noexcept = default;

    MathJaxRenderer::Batch MathJaxRenderer::BeginBatch()
    {
        auto current = CurrentState();
        if (!current) throw winrt::hresult_canceled();
        return Batch(std::move(current));
    }

    void MathJaxRenderer::Batch::Render(std::string_view tex, bool display, float em,
        float containerWidth, std::string_view color, std::stop_token stop)
    {
        if (!impl) throw std::logic_error("Math batch was moved");
        impl->state->Request(tex, display, em, containerWidth, color, stop,
            MathRenderPurpose::Document, &impl->items);
    }
    void MathJaxRenderer::Batch::Submit()
    {
        if (!impl) throw std::logic_error("Math batch was moved");
        impl->state->SubmitBatch(impl->items);
    }
    std::size_t MathJaxRenderer::Batch::Size() const noexcept
    {
        return impl ? impl->items.size() : 0;
    }

    void MathJaxRenderer::State::SubmitBatch(std::vector<Job>& items)
    {
        if (items.empty()) return;
        std::unique_lock lock(mutex);
        if (stopping || workerFinished) throw winrt::hresult_canceled();
        jobs.emplace_back(); // Allocate the queue node before transferring staged ownership.
        jobs.back().kind = Job::Kind::Batch;
        jobs.back().items = std::move(items);
        const bool start = !workerStarted;
        workerStarted = true;
        wake.notify_one();
        lock.unlock();
        if (start)
        {
            try { StartWorker(); }
            catch (...) { FailWorker(); throw; }
        }
    }

    void MathJaxRenderer::State::CompleteBatch(Job const& job, NativeMathSession& session,
        SvgNormalizer& normalizer, std::unique_ptr<MathSvgStore>& store,
        std::function<void()>& callback)
    {
        std::stop_source cancel;
        using CancelCallback = std::stop_callback<std::function<void()>>;
        std::vector<std::unique_ptr<CancelCallback>> callbacks;
        std::vector<NativeMathInput> inputs;
        callbacks.reserve(job.items.size() * 2);
        inputs.reserve(job.items.size());
        for (auto const& item : job.items)
        {
            auto stop = [&cancel] { cancel.request_stop(); };
            callbacks.push_back(std::make_unique<CancelCallback>(item.entry->cancel.get_token(), stop));
            callbacks.push_back(std::make_unique<CancelCallback>(item.callerStop, stop));
            inputs.push_back({item.tex, item.display, item.em, item.containerWidth});
        }
        std::vector<MathJaxSvg> prepared;
        {
            std::scoped_lock lock(mutex);
            ++nativeBatches;
        }
        try { prepared = session.RenderBatch(inputs, cancel.get_token()); }
        catch (winrt::hresult_canceled const&)
        {
            prepared.resize(job.items.size());
            for (auto const& item : job.items) item.entry->cancel.request_stop();
        }
        if (prepared.size() != job.items.size()) throw std::logic_error("Native math batch size mismatch");
        NormalizeResults(prepared, job.items, normalizer, cancel.get_token());
        if (cancel.stop_requested())
            for (auto const& item : job.items) item.entry->cancel.request_stop();
        for (std::size_t i = 0; i < job.items.size(); ++i)
            CompleteRender(job.items[i], session, normalizer, store, callback, &prepared[i]);
    }
}
