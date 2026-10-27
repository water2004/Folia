#pragma once

#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/interaction/EditorTextInputController.h"

import folia.platform.document_reload;

namespace winrt::Folia
{
    struct EditorDocumentController
    {
        using ExecuteCommand = std::function<bool(folia::Command const&)>;
        using SetStatus = std::function<void(winrt::hstring const&)>;
        using SetProgress = std::function<void(
            bool active,
            std::optional<double> value,
            bool cancellable)>;
        using DocumentChanged = std::function<void()>;
        using Render = std::function<void()>;
        using WindowHandle = std::function<HWND()>;
        using SetExternalChange = std::function<void(bool visible, bool fileDeleted)>;
        using ConfirmUnsaved = std::function<winrt::Windows::Foundation::IAsyncOperation<
            winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult>()>;
        using PickSaveFile = std::function<winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFile>()>;

        EditorDocumentController();
        ~EditorDocumentController();
        EditorDocumentController(EditorDocumentController const&) = delete;
        EditorDocumentController& operator=(EditorDocumentController const&) = delete;

        struct Presentation {
            EditorSurfaceRenderer& renderer;
            EditorTextInputController& textInput;
            ExecuteCommand executeCommand;
            SetStatus setStatus;
            SetProgress setProgress;
            DocumentChanged documentChanged;
            Render render;
            WindowHandle windowHandle;
            SetExternalChange setExternalChange;
        };
        void Attach(EditorSession& session, Presentation presentation);
        void RebindPresentation(Presentation presentation);
        void Detach();
        void SuspendPresentation();
        void ResumePresentation(EditorSurfaceRenderer& renderer, EditorTextInputController& textInput);
        bool IsModified() const noexcept;
        bool IsLoading() const noexcept;
        std::optional<std::uint64_t> BeginFileRelocation();
        void FinishFileRelocation(std::uint64_t token, winrt::Windows::Storage::StorageFile const& file = nullptr);
        winrt::Windows::Foundation::IAsyncOperation<bool> OpenFileAsync(
            winrt::Windows::Storage::StorageFile file);
        void SetSaveDialogs(ConfirmUnsaved confirm, PickSaveFile picker = {});
        bool HasUnsavedChanges() const;
        winrt::Windows::Foundation::IAsyncOperation<bool> ConfirmCloseAsync();
        winrt::Windows::Foundation::IAsyncOperation<bool> SaveAsync(bool saveAs = false);
        winrt::Windows::Foundation::IAsyncOperation<bool> PasteAsync(
            winrt::Windows::ApplicationModel::DataTransfer::DataPackageView content);
        void OpenDocument();
        void OpenDocumentPath(winrt::hstring const& path);
        void ReopenDocumentWithEncoding(std::string encoding);
        void SaveDocumentWithEncoding(DocumentEncoding encoding);
        void SaveDocument();
        void SaveDocumentAs();
        void ReloadDocument();
        void AcceptExternalReload();
        void KeepLocalVersion();
        void ExportPdf();
        void CancelOperation();
        void InsertImage();
        void OpenLink(std::string href);
        void CopySelection();
        void CutSelection();
        void PasteClipboard();

    private:
        struct State;
        void BindPresentation(Presentation presentation);
        enum class LoadReason { Open, EncodingReopen, ExternalReload };
        static bool Active(std::shared_ptr<State> const& state, std::uint64_t generation);
        static bool OwnsDocument(std::shared_ptr<State> const& state, std::uint64_t generation);
        static winrt::Windows::Foundation::IAsyncOperation<bool> OpenFileAsync(
            std::shared_ptr<State> state, std::uint64_t generation,
            winrt::Windows::Storage::StorageFile file);
        static winrt::fire_and_forget OpenDocumentAsync(std::shared_ptr<State> state, std::uint64_t generation);
        static winrt::fire_and_forget OpenDocumentPathAsync(
            std::shared_ptr<State> state,
            std::uint64_t generation,
            winrt::hstring path);
        static winrt::fire_and_forget ReopenDocumentWithEncodingAsync(
            std::shared_ptr<State> state,
            std::uint64_t generation,
            std::string encoding);
        static winrt::Windows::Foundation::IAsyncAction LoadDocumentAsync(
            std::shared_ptr<State> state,
            std::uint64_t generation,
            winrt::Windows::Storage::StorageFile file,
            std::optional<std::string> requestedEncoding = std::nullopt,
            LoadReason reason = LoadReason::Open,
            bool preserveView = false);
        static void StartWatching(std::shared_ptr<State> const& state, std::uint64_t generation);
        static void StopWatching(std::shared_ptr<State> const& state);
        static void QueueReloadCheck(std::shared_ptr<State> const& state, std::uint64_t generation);
        static winrt::fire_and_forget DebounceReloadCheckAsync(
            std::shared_ptr<State> state,
            std::uint64_t generation,
            std::uint64_t debounceTicket);
        static void EvaluateReload(std::shared_ptr<State> const& state, std::uint64_t generation, bool manual);
        static winrt::fire_and_forget EvaluateReloadAsync(std::shared_ptr<State> state, std::uint64_t generation, bool manual);
        static void BeginPendingReload(std::shared_ptr<State> const& state, std::uint64_t generation);
        static winrt::fire_and_forget ReloadPendingAsync(
            std::shared_ptr<State> state,
            std::uint64_t generation,
            std::optional<std::string> requestedEncoding);
        static void MarkCurrentDocumentSaved(
            std::shared_ptr<State> const& state,
            folia::platform::reload::WatchedFileVersion version,
            std::string_view source, std::uint64_t contentState);
        static winrt::Windows::Foundation::IAsyncOperation<bool> SaveDocumentAsync(
            std::shared_ptr<State> state, std::uint64_t generation, bool saveAs = false,
            std::optional<DocumentEncoding> encoding = std::nullopt);
        static winrt::Windows::Foundation::IAsyncOperation<bool> ConfirmCloseAsync(
            std::shared_ptr<State> state, std::uint64_t generation);
        static winrt::fire_and_forget ExportPdfAsync(std::shared_ptr<State> state, std::uint64_t generation);
        static winrt::fire_and_forget InsertImageAsync(std::shared_ptr<State> state, std::uint64_t generation);
        static winrt::fire_and_forget OpenLinkAsync(std::shared_ptr<State> state, std::uint64_t generation, std::string href);
        static winrt::Windows::Foundation::IAsyncOperation<bool> PasteClipboardAsync(
            std::shared_ptr<State> state, std::uint64_t generation,
            winrt::Windows::ApplicationModel::DataTransfer::DataPackageView content = nullptr);

        std::shared_ptr<State> state_;
    };
}
