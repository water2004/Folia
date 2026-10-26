#pragma once

#include "editor/interaction/EditorDocumentController.h"

import folia.platform.document_reload;

namespace winrt::Folia
{
    namespace detail
    {
        struct PdfExportWork
        {
            std::stop_source stop;
            std::atomic_bool done = false;
            std::atomic_bool cancelled = false;
            std::atomic_size_t completedPages = 0;
            std::atomic_size_t totalPages = 0;
            std::exception_ptr failure;
        };
    }

    struct EditorDocumentController::State
    {
        std::atomic_uint64_t generation = 1;
        std::atomic_uint64_t documentGeneration = 1;
        bool presenting = false;
        std::optional<std::uint64_t> savedContentState;
        EditorSession* session = nullptr;
        EditorSurfaceRenderer* renderer = nullptr;
        EditorTextInputController* textInput = nullptr;
        ExecuteCommand executeCommand;
        SetStatus setStatus;
        SetProgress setProgress;
        DocumentChanged documentChanged;
        Render render;
        WindowHandle windowHandle;
        SetExternalChange setExternalChange;
        ConfirmUnsaved confirmUnsaved;
        PickSaveFile pickSaveFile;
        bool saving = false;
        bool fileRelocating = false;
        std::uint64_t fileRelocationSerial = 0;
        bool confirmingClose = false;
        bool pasting = false;
        winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher{ nullptr };
        folia::platform::reload::FileChangeMonitor fileMonitor;
        folia::platform::reload::DocumentReloadState reloadState;
        std::filesystem::path watchedPath;
        std::atomic_uint64_t watchSerial = 0;
        std::atomic_uint64_t reloadDebounceSerial = 0;
        std::atomic_uint64_t loadSerial = 0;
        std::uint64_t documentSerial = 0;
        std::atomic_bool documentLoading = false;
        std::stop_source loadStop;
        std::atomic_bool cancelRequested = false;
        std::atomic_bool pdfExporting = false;
        std::filesystem::path pdfOutputPath;
        std::shared_ptr<detail::PdfExportWork> pdfWork;
    };
}
