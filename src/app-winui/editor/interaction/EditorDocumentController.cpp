#include "pch.h"
#include "editor/interaction/EditorDocumentController.h"
#include "editor/interaction/EditorDocumentControllerState.h"
#include "localization/Localization.h"
#include "storage/DocumentEncodingService.h"

import folia.core.command;
import folia.markdown.slug;
import folia.markdown.utf;

namespace winrt::Folia
{
    EditorDocumentController::EditorDocumentController() : state_(std::make_shared<State>())
    {
    }

    EditorDocumentController::~EditorDocumentController()
    {
        Detach();
    }

    void EditorDocumentController::Attach(EditorSession& session, Presentation presentation)
    {
        Detach();
        state_->session = &session;
        state_->presenting = true;
        BindPresentation(std::move(presentation));
        state_->dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        const auto source = session.TextUtf8();
        state_->reloadState.Reset({}, folia::platform::reload::fingerprint_document_content(
            session.HasFile() ? std::string_view(source) : std::string_view{}));
        state_->savedContentState = session.HasFile() || source.empty()
            ? std::optional{session.ContentState()} : std::nullopt;
    }

    void EditorDocumentController::BindPresentation(Presentation value) {
        state_->renderer = &value.renderer;
        state_->textInput = &value.textInput;
        state_->executeCommand = std::move(value.executeCommand);
        state_->setStatus = std::move(value.setStatus);
        state_->setProgress = std::move(value.setProgress);
        state_->documentChanged = std::move(value.documentChanged);
        state_->render = std::move(value.render);
        state_->windowHandle = std::move(value.windowHandle);
        state_->setExternalChange = std::move(value.setExternalChange);
    }

    void EditorDocumentController::RebindPresentation(Presentation value) {
        if (!state_->session || state_->presenting)
            throw std::logic_error("Suspend document presentation before rebinding its window");
        BindPresentation(std::move(value));
        ResumePresentation(*state_->renderer, *state_->textInput);
    }

    void EditorDocumentController::Detach()
    {
        StopWatching(state_);
        state_->loadStop.request_stop();
        state_->cancelRequested = true;
        state_->pdfExporting = false;
        if (state_->pdfWork) state_->pdfWork->stop.request_stop();
        if (!state_->pdfWork && !state_->pdfOutputPath.empty())
        {
            std::error_code ignored;
            std::filesystem::remove(state_->pdfOutputPath, ignored);
            state_->pdfOutputPath.clear();
        }
        state_->generation.fetch_add(1);
        state_->documentGeneration.fetch_add(1);
        state_->presenting = false;
        state_->fileRelocating = false;
        ++state_->fileRelocationSerial;
        state_->session = nullptr;
        state_->renderer = nullptr;
        state_->textInput = nullptr;
        state_->executeCommand = {};
        state_->setStatus = {};
        state_->setProgress = {};
        state_->documentChanged = {};
        state_->render = {};
        state_->windowHandle = {};
        state_->setExternalChange = {};
        state_->confirmUnsaved = {};
        state_->pickSaveFile = {};
        state_->dispatcher = nullptr;
    }

    void EditorDocumentController::OpenDocument()
    {
        OpenDocumentAsync(state_, state_->generation.load());
    }

    void EditorDocumentController::OpenDocumentPath(winrt::hstring const& path)
    {
        if (path.empty()) return;
        OpenDocumentPathAsync(state_, state_->generation.load(), path);
    }

    void EditorDocumentController::ReopenDocumentWithEncoding(std::string encoding)
    {
        if (encoding.empty()) return;
        ReopenDocumentWithEncodingAsync(state_, state_->generation.load(), std::move(encoding));
    }

    void EditorDocumentController::SaveDocumentWithEncoding(DocumentEncoding encoding)
    {
        if (encoding.name.empty()) return;
        SaveDocumentAsync(state_, state_->documentGeneration.load(), false, std::move(encoding));
    }

    void EditorDocumentController::SaveDocument()
    {
        SaveDocumentAsync(state_, state_->documentGeneration.load());
    }

    void EditorDocumentController::SaveDocumentAs()
    {
        SaveDocumentAsync(state_, state_->documentGeneration.load(), true);
    }

    void EditorDocumentController::InsertImage()
    {
        InsertImageAsync(state_, state_->generation.load());
    }

    void EditorDocumentController::OpenLink(std::string href)
    {
        while (!href.empty() && static_cast<unsigned char>(href.front()) <= 0x20) href.erase(href.begin());
        while (!href.empty() && static_cast<unsigned char>(href.back()) <= 0x20) href.pop_back();
        if (href.empty() || !state_->session) return;
        if (href.front() == '#')
        {
            if (auto item = state_->session->RenderModel().outline.find_item_by_url_fragment(href))
            {
                state_->session->SetSelection(item->position, item->position);
                if (state_->textInput) state_->textInput->NotifySelectionChanged();
                if (state_->renderer) state_->renderer->ScrollToPosition(item->position);
                if (state_->render) state_->render();
            }
            return;
        }
        OpenLinkAsync(state_, state_->generation.load(), std::move(href));
    }

    void EditorDocumentController::CopySelection()
    {
        if (!state_->session || !state_->session->HasSelection()) return;
        auto package = winrt::Windows::ApplicationModel::DataTransfer::DataPackage();
        package.SetText(winrt::to_hstring(state_->session->SelectedTextUtf8()));
        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
        if (state_->setStatus) state_->setStatus(Localize(L"StatusCopiedSelection"));
    }

    void EditorDocumentController::CutSelection()
    {
        if (!state_->session || !state_->session->HasSelection()) return;
        CopySelection();
        folia::Command command;
        command.kind = folia::CommandKind::DeleteSelection;
        if (state_->executeCommand) state_->executeCommand(command);
    }

    void EditorDocumentController::PasteClipboard()
    {
        PasteClipboardAsync(state_, state_->generation.load());
    }

    bool EditorDocumentController::Active(std::shared_ptr<State> const& state, std::uint64_t generation)
    {
        return state && state->generation.load() == generation && state->session && state->presenting && !state->fileRelocating;
    }

    bool EditorDocumentController::OwnsDocument(std::shared_ptr<State> const& state, std::uint64_t generation)
    {
        return state && state->documentGeneration.load() == generation && state->session;
    }

    void EditorDocumentController::SuspendPresentation()
    {
        if (!state_->presenting) return;
        StopWatching(state_);
        state_->loadStop.request_stop();
        state_->generation.fetch_add(1);
        state_->presenting = false;
        state_->renderer = nullptr;
        state_->textInput = nullptr;
    }

    void EditorDocumentController::ResumePresentation(EditorSurfaceRenderer& renderer, EditorTextInputController& textInput)
    {
        if (!state_->session) throw std::logic_error("Cannot resume a detached document");
        if (state_->presenting) throw std::logic_error("Document presentation is already active");
        state_->renderer = &renderer;
        state_->textInput = &textInput;
        state_->presenting = true;
        state_->loadStop = std::stop_source{};
        renderer.SetMathPreparationStopToken(state_->loadStop.get_token());
        StartWatching(state_, state_->generation.load());
        QueueReloadCheck(state_, state_->generation.load());
        if (auto pending = state_->reloadState.PendingVersion(); pending && state_->setExternalChange)
            state_->setExternalChange(true, !pending->exists);
    }

    bool EditorDocumentController::IsModified() const noexcept
    {
        return state_->session && state_->savedContentState != state_->session->ContentState();
    }

    bool EditorDocumentController::IsLoading() const noexcept
    {
        return state_->documentLoading.load();
    }

    winrt::Windows::Foundation::IAsyncOperation<bool> EditorDocumentController::OpenFileAsync(
        winrt::Windows::Storage::StorageFile file)
    {
        return OpenFileAsync(state_, state_->generation.load(), std::move(file));
    }

    winrt::Windows::Foundation::IAsyncOperation<bool> EditorDocumentController::OpenFileAsync(
        std::shared_ptr<State> state, std::uint64_t generation, winrt::Windows::Storage::StorageFile file)
    {
        const auto identity = state->documentSerial;
        try { co_await LoadDocumentAsync(state, generation, file); }
        catch (winrt::hresult_error const& error) {
            if (Active(state, generation)) {
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(LocalizeFormat(L"StatusOpenFailed", {error.message()}));
            }
        }
        catch (std::exception const& error) {
            if (Active(state, generation)) {
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(LocalizeFormat(L"StatusOpenFailed", {winrt::to_hstring(error.what())}));
            }
        }
        co_return Active(state, generation) && state->documentSerial != identity;
    }

    winrt::fire_and_forget EditorDocumentController::OpenDocumentAsync(std::shared_ptr<State> state, std::uint64_t generation)
    {
        try
        {
            if (!Active(state, generation) || !state->windowHandle) co_return;
            auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
            picker.FileTypeFilter().Append(L".md");
            picker.FileTypeFilter().Append(L".markdown");
            picker.FileTypeFilter().Append(L".txt");
            auto initializeWithWindow = picker.as<IInitializeWithWindow>();
            winrt::check_hresult(initializeWithWindow->Initialize(state->windowHandle()));
            auto file = co_await picker.PickSingleFileAsync();
            if (!Active(state, generation)) co_return;
            if (!file)
            {
                if (state->setStatus) state->setStatus(Localize(L"StatusOpenCancelled"));
                co_return;
            }
            co_await LoadDocumentAsync(state, generation, file);
        }
        catch (winrt::hresult_error const& error)
        {
            if (Active(state, generation))
            {
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(LocalizeFormat(L"StatusOpenFailed", { error.message() }));
            }
        }
        catch (std::exception const& error)
        {
            if (Active(state, generation))
            {
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(LocalizeFormat(
                    L"StatusOpenFailed", { winrt::to_hstring(error.what()) }));
            }
        }
    }

    winrt::fire_and_forget EditorDocumentController::OpenDocumentPathAsync(
        std::shared_ptr<State> state,
        std::uint64_t generation,
        winrt::hstring path)
    {
        try
        {
            if (!Active(state, generation)) co_return;
            auto normalized = std::filesystem::path(path.c_str()).lexically_normal();
            if (normalized.is_relative()) normalized = std::filesystem::absolute(normalized);
            auto file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(normalized.wstring());
            if (!Active(state, generation)) co_return;
            co_await LoadDocumentAsync(state, generation, file);
        }
        catch (winrt::hresult_error const& error)
        {
            if (Active(state, generation))
            {
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(LocalizeFormat(L"StatusOpenFailed", { error.message() }));
            }
        }
        catch (std::exception const& error)
        {
            if (Active(state, generation))
            {
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(LocalizeFormat(
                    L"StatusOpenFailed", { winrt::to_hstring(error.what()) }));
            }
        }
    }

    winrt::fire_and_forget EditorDocumentController::ReopenDocumentWithEncodingAsync(
        std::shared_ptr<State> state,
        std::uint64_t generation,
        std::string encoding)
    {
        try
        {
            if (!Active(state, generation) || !state->session->HasFile()) co_return;
            co_await LoadDocumentAsync(
                state,
                generation,
                state->session->File(),
                std::move(encoding),
                LoadReason::EncodingReopen,
                true);
        }
        catch (winrt::hresult_error const& error)
        {
            if (Active(state, generation))
            {
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(LocalizeFormat(L"StatusOpenFailed", { error.message() }));
            }
        }
        catch (std::exception const& error)
        {
            if (Active(state, generation))
            {
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(LocalizeFormat(
                    L"StatusOpenFailed", { winrt::to_hstring(error.what()) }));
            }
        }
    }

    winrt::Windows::Foundation::IAsyncAction EditorDocumentController::LoadDocumentAsync(
        std::shared_ptr<State> state,
        std::uint64_t generation,
        winrt::Windows::Storage::StorageFile file,
        std::optional<std::string> requestedEncoding,
        LoadReason reason,
        bool preserveView)
    {
        if (!Active(state, generation) || !file) co_return;
        // Timer awaits resume on a pool thread. Keep the entry apartment for
        // every retry before touching the session, renderer or UI callbacks.
        winrt::apartment_context uiContext;
        if (reason != LoadReason::ExternalReload) {
            if (!co_await ConfirmCloseAsync(state, state->documentGeneration.load())) co_return;
            co_await uiContext;
            if (!Active(state, generation)) co_return;
        }
        auto loadTicket = state->loadSerial.fetch_add(1) + 1;
        state->loadStop.request_stop();
        state->loadStop = std::stop_source{};
        auto loadStop = state->loadStop.get_token();
        state->documentLoading.store(true, std::memory_order_release);
        struct LoadingGuard
        {
            std::atomic_bool* loading;
            std::atomic_uint64_t* serial;
            std::uint64_t ticket;

            void Complete() noexcept
            {
                if (serial->load(std::memory_order_acquire) == ticket)
                    loading->store(false, std::memory_order_release);
            }

            ~LoadingGuard() { Complete(); }
        } loadingGuard{ &state->documentLoading, &state->loadSerial, loadTicket };
        auto viewState = preserveView
            ? std::optional{state->session->CaptureReloadViewState()}
            : std::nullopt;
        auto scrollOffset = preserveView && state->renderer
            ? state->renderer->ScrollOffset()
            : 0.0f;
        if (state->setStatus) state->setStatus(LocalizeFormat(L"StatusReadingFile", { file.Name() }));
        if (state->setProgress) state->setProgress(true, std::nullopt, true);
        std::vector<std::uint8_t> bytes;
        folia::platform::reload::WatchedFileVersion readVersion;
        for (std::size_t attempt = 0; attempt < 5; ++attempt)
        {
            readVersion = folia::platform::reload::query_watched_file_version(
                std::filesystem::path(file.Path().c_str()));
            if (!readVersion.exists) throw std::runtime_error("document no longer exists");
            auto buffer = co_await winrt::Windows::Storage::FileIO::ReadBufferAsync(file);
            if (!Active(state, generation) || state->loadSerial.load() != loadTicket) co_return;
            auto afterRead = folia::platform::reload::query_watched_file_version(
                std::filesystem::path(file.Path().c_str()));
            if (afterRead == readVersion)
            {
                winrt::Windows::Storage::Streams::DataReader reader =
                    winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer);
                bytes.resize(buffer.Length());
                reader.ReadBytes(bytes);
                break;
            }
            if (attempt == 4) throw std::runtime_error("document kept changing while it was read");
            co_await winrt::resume_after(std::chrono::milliseconds(100));
            co_await uiContext;
            if (!Active(state, generation) || state->loadSerial.load() != loadTicket) co_return;
        }
        if (state->setStatus) state->setStatus(LocalizeFormat(L"StatusParsingFile", { file.Name() }));
        if (state->setProgress) state->setProgress(true, 0.0, true);
        auto dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        auto reportedPercent = std::make_shared<std::atomic_size_t>(0);
        auto progressActive = std::make_shared<std::atomic_bool>(true);
        std::exception_ptr loadFailure;
        std::optional<EditorSession> loaded;
        folia::platform::reload::DocumentContentFingerprint loadedFingerprint;
        co_await winrt::resume_background();
        try
        {
            auto decoded = DocumentEncodingService::Decode(
                bytes,
                requestedEncoding
                    ? std::optional<std::string_view>{ *requestedEncoding }
                    : std::nullopt);
            loaded.emplace();
            loaded->Open(
                file,
                winrt::to_hstring(decoded.utf8),
                std::move(decoded.encoding),
                std::move(decoded.candidates), [
                state,
                generation,
                loadTicket,
                loadStop,
                dispatcher,
                reportedPercent,
                progressActive](
                std::size_t consumed,
                std::size_t total)
            {
                if (loadStop.stop_requested()) throw winrt::hresult_canceled();
                if (!progressActive->load()) return;
                auto percent = total == 0
                    ? std::size_t{100}
                    : (std::min)(std::size_t{100}, consumed * 100 / total);
                auto previous = reportedPercent->load();
                while (percent > previous
                    && !reportedPercent->compare_exchange_weak(previous, percent)) {}
                if (percent <= previous) return;
                dispatcher.TryEnqueue([state, generation, loadTicket, percent, progressActive]
                {
                    if (!progressActive->load()
                        || !Active(state, generation)
                        || state->loadSerial.load() != loadTicket
                        || !state->setProgress) return;
                    state->setProgress(true, static_cast<double>(percent) / 100.0, true);
                });
            }, requestedEncoding.has_value());
            // Compare subsequent edits with the loaded editor representation,
            // not differences in untouched file-level separator formatting.
            loadedFingerprint = folia::platform::reload::fingerprint_document_content(loaded->TextUtf8());
            if (viewState) loaded->RestoreReloadViewState(*viewState);
        }
        catch (...)
        {
            loadFailure = std::current_exception();
        }
        progressActive->store(false);
        co_await uiContext;
        if (!Active(state, generation) || state->loadSerial.load() != loadTicket) co_return;
        if (loadStop.stop_requested())
        {
            loadingGuard.Complete();
            if (state->setProgress) state->setProgress(false, std::nullopt, false);
            if (state->setStatus) state->setStatus(Localize(L"StatusOpenCancelled"));
            co_return;
        }
        if (loadFailure) std::rethrow_exception(loadFailure);
        std::unique_ptr<EditorMathPreparation> preparation;
        for (;;)
        {
            if (!Active(state, generation) || state->loadSerial.load() != loadTicket) co_return;
            if (loadStop.stop_requested())
            {
                loadingGuard.Complete();
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(Localize(L"StatusOpenCancelled"));
                co_return;
            }
            auto context = state->renderer->CaptureMathPreparationContext();
            if (!context.ready)
            {
                co_await winrt::resume_after(std::chrono::milliseconds(32));
                co_await uiContext;
                continue;
            }
            if (state->setStatus) state->setStatus(LocalizeFormat(L"StatusPreparingMath", {file.Name()}));
            if (state->setProgress) state->setProgress(true, 0.0, true);
            auto mathProgressActive = std::make_shared<std::atomic_bool>(true);
            auto mathPercent = std::make_shared<std::atomic_size_t>(0);
            co_await winrt::resume_background();
            try
            {
                loaded->SetTheme(context.theme);
                preparation = EditorSurfaceRenderer::PrepareMath(
                    loaded->RenderFrame(), context, loadStop,
                    [state, generation, loadTicket, dispatcher, mathProgressActive, mathPercent]
                    (std::size_t completed, std::size_t total)
                    {
                        auto percent = total == 0 ? std::size_t{100}
                            : (std::min)(std::size_t{100}, completed * 100 / total);
                        if (mathPercent->exchange(percent) == percent) return;
                        dispatcher.TryEnqueue([state, generation, loadTicket, mathProgressActive, percent]
                        {
                            if (!mathProgressActive->load() || !Active(state, generation)
                                || state->loadSerial.load() != loadTicket || !state->setProgress) return;
                            state->setProgress(true, static_cast<double>(percent) / 100.0, true);
                        });
                    });
            }
            catch (...) { loadFailure = std::current_exception(); }
            mathProgressActive->store(false);
            co_await uiContext;
            if (!Active(state, generation) || state->loadSerial.load() != loadTicket) co_return;
            if (loadStop.stop_requested())
            {
                loadingGuard.Complete();
                if (state->setProgress) state->setProgress(false, std::nullopt, false);
                if (state->setStatus) state->setStatus(Localize(L"StatusOpenCancelled"));
                co_return;
            }
            if (loadFailure) std::rethrow_exception(loadFailure);
            if (state->renderer->MatchesMathPreparationContext(context)) break;
            // Only formula inputs depend on this context; ordinary blocks
            // continue through the existing viewport materialization path.
            preparation.reset();
        }
        auto versionAfterParse = folia::platform::reload::query_watched_file_version(
            std::filesystem::path(file.Path().c_str()));
        *state->session = std::move(*loaded);
        ++state->documentSerial;
        state->savedContentState = state->session->ContentState();
        if (reason == LoadReason::ExternalReload)
            state->reloadState.Accept(readVersion, loadedFingerprint);
        else
            state->reloadState.Reset(readVersion, loadedFingerprint);
        if (state->renderer)
        {
            state->renderer->AdoptMathPreparation(std::move(preparation));
            state->renderer->SetScrollOffset(scrollOffset);
        }
        // UI notifications synchronously query IsLoading and may render the
        // replacement document. Publish the completed state before notifying.
        loadingGuard.Complete();
        if (state->setProgress) state->setProgress(false, std::nullopt, false);
        if (state->textInput) state->textInput->NotifyTextChanged();
        if (state->documentChanged) state->documentChanged();
        if (state->setExternalChange) state->setExternalChange(false, false);
        StartWatching(state, generation);
        if (versionAfterParse != readVersion) QueueReloadCheck(state, generation);
        if (state->setStatus) state->setStatus(LocalizeFormat(
            reason == LoadReason::ExternalReload ? L"StatusReloadedFile" : L"StatusOpenedFile",
            { file.Name() }));
    }

    winrt::fire_and_forget EditorDocumentController::InsertImageAsync(std::shared_ptr<State> state, std::uint64_t generation)
    {
        try
        {
            if (!Active(state, generation) || !state->windowHandle) co_return;
            auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
            for (auto extension : {
                L".svg",
                L".png",
                L".jpg",
                L".jpeg",
                L".gif",
                L".bmp",
                L".tif",
                L".tiff",
                L".ico",
                L".webp",
                L".heic",
                L".heif",
                L".avif",
                L".jxr",
                L".wdp",
            }) picker.FileTypeFilter().Append(extension);
            auto initializeWithWindow = picker.as<IInitializeWithWindow>();
            winrt::check_hresult(initializeWithWindow->Initialize(state->windowHandle()));
            auto file = co_await picker.PickSingleFileAsync();
            if (!Active(state, generation) || !file) co_return;
            std::filesystem::path path(file.Path().c_str());
            if (state->session->HasFile())
            {
                std::error_code error;
                auto base = std::filesystem::path(state->session->Path().c_str()).parent_path();
                auto relative = std::filesystem::relative(path, base, error);
                if (!error && !relative.empty()) path = std::move(relative);
            }
            folia::Command command;
            command.kind = folia::CommandKind::InsertImage;
            command.path = folia::utf8_to_cps(winrt::to_string(winrt::hstring(path.generic_wstring())));
            if (state->executeCommand) state->executeCommand(command);
        }
        catch (winrt::hresult_error const& error)
        {
            if (Active(state, generation) && state->setStatus)
                state->setStatus(LocalizeFormat(L"StatusImageInsertFailed", { error.message() }));
        }
    }

    winrt::fire_and_forget EditorDocumentController::OpenLinkAsync(std::shared_ptr<State> state, std::uint64_t generation, std::string href)
    {
        auto lower = href;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        try
        {
            if (!Active(state, generation)) co_return;
            if (lower.starts_with("https://") || lower.starts_with("http://") || lower.starts_with("mailto:"))
            {
                co_await winrt::Windows::System::Launcher::LaunchUriAsync(winrt::Windows::Foundation::Uri(winrt::to_hstring(href)));
                co_return;
            }
            auto colon = lower.find(':');
            auto boundary = lower.find_first_of("/?#");
            if (colon != std::string::npos && (boundary == std::string::npos || colon < boundary)) co_return;
            const auto suffix = href.find_first_of("?#");
            const auto encodedPath = suffix == std::string::npos
                ? std::string_view{href}
                : std::string_view{href}.substr(0, suffix);
            const auto decodedPath = folia::percent_decode_url_component(encodedPath);
            if (decodedPath.empty()) co_return;
            auto path = std::filesystem::path(winrt::to_hstring(decodedPath).c_str());
            if (path.is_relative()) path = std::filesystem::path(state->session->BaseDirectory()) / path;
            auto file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path.lexically_normal().wstring());
            if (!Active(state, generation)) co_return;
            co_await winrt::Windows::System::Launcher::LaunchFileAsync(file);
        }
        catch (winrt::hresult_error const& error)
        {
            if (Active(state, generation) && state->setStatus)
                state->setStatus(LocalizeFormat(L"StatusOpenLinkFailed", { error.message() }));
        }
    }

}
