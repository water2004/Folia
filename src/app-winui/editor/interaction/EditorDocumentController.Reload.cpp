#include "pch.h"
#include "editor/interaction/EditorDocumentController.h"
#include "editor/interaction/EditorDocumentControllerState.h"
#include "localization/Localization.h"

namespace winrt::Folia
{
    void EditorDocumentController::ReloadDocument()
    {
        EvaluateReload(state_, state_->generation.load(), true);
    }

    void EditorDocumentController::AcceptExternalReload()
    {
        if (!Active(state_, state_->generation.load())) return;
        auto pending = state_->reloadState.PendingVersion();
        if (!pending || !pending->exists) return;
        if (state_->setExternalChange) state_->setExternalChange(false, false);
        BeginPendingReload(state_, state_->generation.load());
    }

    void EditorDocumentController::KeepLocalVersion()
    {
        if (!Active(state_, state_->generation.load())) return;
        state_->reloadState.KeepPending();
        if (state_->setExternalChange) state_->setExternalChange(false, false);
        if (state_->setStatus) state_->setStatus(Localize(L"StatusKeptLocalChanges"));
    }

    void EditorDocumentController::StartWatching(
        std::shared_ptr<State> const& state,
        std::uint64_t generation)
    {
        StopWatching(state);
        if (!Active(state, generation) || !state->session->HasFile()) return;

        state->watchedPath = std::filesystem::path(state->session->File().Path().c_str());
        auto serial = state->watchSerial.load();
        auto weak = std::weak_ptr<State>(state);
        std::error_code error;
        state->fileMonitor.Start(state->watchedPath, [weak, generation, serial]
        {
            auto state = weak.lock();
            if (!state || state->watchSerial.load() != serial || !state->dispatcher) return;
            state->dispatcher.TryEnqueue([weak, generation, serial]
            {
                auto state = weak.lock();
                if (!state || state->watchSerial.load() != serial
                    || !Active(state, generation)) return;
                QueueReloadCheck(state, generation);
            });
        }, error);
    }

    void EditorDocumentController::StopWatching(std::shared_ptr<State> const& state)
    {
        if (!state) return;
        state->watchSerial.fetch_add(1);
        state->reloadDebounceSerial.fetch_add(1);
        state->fileMonitor.Stop();
        state->watchedPath.clear();
    }

    void EditorDocumentController::QueueReloadCheck(
        std::shared_ptr<State> const& state,
        std::uint64_t generation)
    {
        if (!Active(state, generation)) return;
        auto ticket = state->reloadDebounceSerial.fetch_add(1) + 1;
        DebounceReloadCheckAsync(state, generation, ticket);
    }

    winrt::fire_and_forget EditorDocumentController::DebounceReloadCheckAsync(
        std::shared_ptr<State> state,
        std::uint64_t generation,
        std::uint64_t debounceTicket)
    {
        co_await winrt::resume_after(std::chrono::milliseconds(400));
        if (!state || !state->dispatcher) co_return;
        state->dispatcher.TryEnqueue([state, generation, debounceTicket]
        {
            if (!Active(state, generation)
                || state->reloadDebounceSerial.load() != debounceTicket) return;
            EvaluateReload(state, generation, false);
        });
    }

    void EditorDocumentController::EvaluateReload(
        std::shared_ptr<State> const& state,
        std::uint64_t generation,
        bool manual)
    {
        EvaluateReloadAsync(state, generation, manual);
    }

    winrt::fire_and_forget EditorDocumentController::EvaluateReloadAsync(
        std::shared_ptr<State> state, std::uint64_t generation, bool manual)
    {
        auto dispatcher = state->dispatcher;
        try {
        if (!Active(state, generation) || !state->session->HasFile()
            || state->documentLoading.load(std::memory_order_acquire)) co_return;
        auto path = std::filesystem::path(state->session->File().Path().c_str());
        auto identity = state->documentSerial;
        auto ticket = state->loadSerial.load();
        winrt::apartment_context ui;
        co_await winrt::resume_background();
        auto version = folia::platform::reload::query_watched_file_version(path);
        co_await ui;
        if (!Active(state, generation) || state->documentSerial != identity || state->loadSerial.load() != ticket
            || state->documentLoading || (!manual && state->reloadState.IsKnown(version))) co_return;

        auto source = folia::platform::reload::fingerprint_document_content(
            state->session->TextUtf8());
        auto action = manual
            ? state->reloadState.StageManual(version, source)
            : state->reloadState.Observe(version, source);
        if (action == folia::platform::reload::DocumentReloadAction::None) co_return;
        if (action == folia::platform::reload::DocumentReloadAction::Prompt)
        {
            if (state->setExternalChange) state->setExternalChange(true, !version.exists);
            co_return;
        }
        BeginPendingReload(state, generation);
        } catch (winrt::hresult_error const& error) {
            if (dispatcher && dispatcher.HasThreadAccess() && Active(state, generation) && state->setStatus)
                state->setStatus(LocalizeFormat(L"StatusReloadFailed", {error.message()}));
        } catch (std::exception const& error) {
            if (dispatcher && dispatcher.HasThreadAccess() && Active(state, generation) && state->setStatus)
                state->setStatus(LocalizeFormat(L"StatusReloadFailed", {winrt::to_hstring(error.what())}));
        }
    }

    void EditorDocumentController::BeginPendingReload(
        std::shared_ptr<State> const& state,
        std::uint64_t generation)
    {
        if (!Active(state, generation)) return;
        auto pending = state->reloadState.PendingVersion();
        if (!pending || !pending->exists) return;
        std::optional<std::string> encoding;
        if (state->session->EncodingWasExplicit()) encoding = state->session->Encoding().name;
        ReloadPendingAsync(state, generation, std::move(encoding));
    }

    winrt::fire_and_forget EditorDocumentController::ReloadPendingAsync(
        std::shared_ptr<State> state,
        std::uint64_t generation,
        std::optional<std::string> requestedEncoding)
    {
        try
        {
            if (!Active(state, generation) || !state->session->HasFile()) co_return;
            co_await LoadDocumentAsync(
                state,
                generation,
                state->session->File(),
                std::move(requestedEncoding),
                LoadReason::ExternalReload,
                true);
        }
        catch (winrt::hresult_error const& error)
        {
            if (Active(state, generation))
            {
                state->reloadState.DiscardPending();
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(LocalizeFormat(
                    L"StatusReloadFailed", { error.message() }));
            }
        }
        catch (std::exception const& error)
        {
            if (Active(state, generation))
            {
                state->reloadState.DiscardPending();
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(LocalizeFormat(
                    L"StatusReloadFailed", { winrt::to_hstring(error.what()) }));
            }
        }
    }

    void EditorDocumentController::MarkCurrentDocumentSaved(
        std::shared_ptr<State> const& state,
        folia::platform::reload::WatchedFileVersion version,
        std::string_view source, std::uint64_t contentState)
    {
        if (!state) return;
        state->savedContentState = contentState;
        state->reloadState.MarkSaved(
            version,
            folia::platform::reload::fingerprint_document_content(source));
        if (state->setExternalChange) state->setExternalChange(false, false);
    }
}
