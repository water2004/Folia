#include "pch.h"
#include "editor/interaction/EditorDocumentControllerState.h"
#include "localization/Localization.h"

namespace winrt::Folia {
    namespace {
        struct PendingOperation {
            bool& active;
            explicit PendingOperation(bool& value) : active(value) { active = true; }
            ~PendingOperation() { active = false; }
        };

        winrt::Windows::Foundation::IAsyncAction WriteBytesAsync(
            winrt::Windows::Storage::StorageFile file, std::vector<std::uint8_t> bytes) {
            auto transaction = co_await file.OpenTransactedWriteAsync();
            auto stream = transaction.Stream();
            stream.Size(0);
            winrt::Windows::Storage::Streams::DataWriter writer(stream.GetOutputStreamAt(0));
            writer.WriteBytes(bytes);
            co_await writer.StoreAsync();
            co_await writer.FlushAsync();
            writer.DetachStream();
            co_await transaction.CommitAsync();
        }
    }

    void EditorDocumentController::SetSaveDialogs(ConfirmUnsaved confirm, PickSaveFile picker) {
        state_->confirmUnsaved = std::move(confirm);
        state_->pickSaveFile = std::move(picker);
    }

    bool EditorDocumentController::HasUnsavedChanges() const {
        return state_->session && state_->reloadState.LocallyModified(
            folia::platform::reload::fingerprint_document_content(state_->session->TextUtf8()));
    }

    winrt::Windows::Foundation::IAsyncOperation<bool> EditorDocumentController::SaveAsync(bool saveAs) {
        return SaveDocumentAsync(state_, state_->documentGeneration.load(), saveAs);
    }

    winrt::Windows::Foundation::IAsyncOperation<bool> EditorDocumentController::ConfirmCloseAsync() {
        return ConfirmCloseAsync(state_, state_->documentGeneration.load());
    }

    winrt::Windows::Foundation::IAsyncOperation<bool> EditorDocumentController::ConfirmCloseAsync(
        std::shared_ptr<State> state, std::uint64_t generation) {
        if (!OwnsDocument(state, generation) || state->confirmingClose || state->fileRelocating) co_return false;
        PendingOperation pending(state->confirmingClose);
        auto document = state->loadSerial.load();
        auto identity = state->documentSerial;
        auto current = [&] { return OwnsDocument(state, generation) && state->loadSerial.load() == document && state->documentSerial == identity; };
        winrt::apartment_context ui;
        try {
            while (current() && state->reloadState.LocallyModified(
                folia::platform::reload::fingerprint_document_content(state->session->TextUtf8()))) {
                if (!state->confirmUnsaved) co_return false;
                auto confirm = state->confirmUnsaved;
                auto choice = co_await confirm();
                co_await ui;
                if (!current()) co_return false;
                using winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult;
                if (choice == ContentDialogResult::Secondary) co_return true;
                if (choice != ContentDialogResult::Primary) co_return false;
                if (!co_await SaveDocumentAsync(state, generation)) co_return false;
                co_await ui;
                // Edits made while an asynchronous save was writing still need
                // a decision; only the exact written snapshot becomes clean.
            }
            co_return current();
        } catch (winrt::hresult_error const& error) {
            if (current() && state->setStatus) state->setStatus(LocalizeFormat(L"StatusSaveFailed", {error.message()}));
        } catch (std::exception const& error) {
            if (current() && state->setStatus) state->setStatus(LocalizeFormat(L"StatusSaveFailed", {winrt::to_hstring(error.what())}));
        }
        co_return false;
    }

    winrt::Windows::Foundation::IAsyncOperation<bool> EditorDocumentController::SaveDocumentAsync(
        std::shared_ptr<State> state, std::uint64_t generation, bool saveAs,
        std::optional<DocumentEncoding> requestedEncoding) {
        if (!OwnsDocument(state, generation) || state->saving || state->documentLoading || state->fileRelocating) co_return false;
        PendingOperation pending(state->saving);
        auto document = state->loadSerial.load();
        auto identity = state->documentSerial;
        auto current = [&] { return OwnsDocument(state, generation) && state->loadSerial.load() == document && state->documentSerial == identity; };
        winrt::apartment_context ui;
        try {
            auto newFile = saveAs || !state->session->HasFile();
            auto file = state->session->File();
            if (newFile) {
                if (auto chooseFile = state->pickSaveFile) file = co_await chooseFile();
                else {
                    if (!state->windowHandle) co_return false;
                    auto picker = winrt::Windows::Storage::Pickers::FileSavePicker();
                    picker.DefaultFileExtension(L".md");
                    picker.SuggestedFileName(Localize(L"UntitledMarkdown"));
                    picker.FileTypeChoices().Insert(Localize(L"MarkdownFileType"), winrt::single_threaded_vector<winrt::hstring>({L".md"}));
                    picker.FileTypeChoices().Insert(Localize(L"TextFileType"), winrt::single_threaded_vector<winrt::hstring>({L".txt"}));
                    winrt::check_hresult(picker.as<IInitializeWithWindow>()->Initialize(state->windowHandle()));
                    file = co_await picker.PickSaveFileAsync();
                }
                co_await ui;
                if (!current()) co_return false;
                if (!file) {
                    if (state->setStatus) state->setStatus(Localize(L"StatusSaveCancelled"));
                    co_return false;
                }
            }
            auto text = state->session->TextUtf8();
            const auto contentState = state->session->ContentState();
            auto encoding = DocumentEncodingService::NormalizeForSave(requestedEncoding.value_or(state->session->Encoding()));
            std::exception_ptr failure;
            folia::platform::reload::WatchedFileVersion version;
            co_await winrt::resume_background();
            try {
                co_await WriteBytesAsync(file, DocumentEncodingService::Encode(text, encoding));
                version = folia::platform::reload::query_watched_file_version(std::filesystem::path(file.Path().c_str()));
            } catch (...) { failure = std::current_exception(); }
            co_await ui;
            if (!current()) co_return false;
            if (failure) std::rethrow_exception(failure);
            if (newFile) state->session->SaveAs(file);
            if (requestedEncoding) state->session->SetEncoding(std::move(encoding));
            MarkCurrentDocumentSaved(state, version, text, contentState);
            if (newFile) StartWatching(state, state->generation.load());
            if (state->documentChanged) state->documentChanged();
            if (state->setStatus) state->setStatus(LocalizeFormat(L"StatusSavedFile", {file.Name()}));
            co_return true;
        } catch (winrt::hresult_error const& error) {
            if (current() && state->setStatus) state->setStatus(LocalizeFormat(L"StatusSaveFailed", {error.message()}));
        } catch (std::exception const& error) {
            if (current() && state->setStatus) state->setStatus(LocalizeFormat(L"StatusSaveFailed", {winrt::to_hstring(error.what())}));
        }
        co_return false;
    }
}
