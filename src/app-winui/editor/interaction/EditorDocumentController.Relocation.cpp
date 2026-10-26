#include "pch.h"
#include "editor/interaction/EditorDocumentControllerState.h"

namespace winrt::Folia {
    std::optional<std::uint64_t> EditorDocumentController::BeginFileRelocation() {
        if (!state_->session || !state_->session->HasFile() || state_->fileRelocating
            || state_->saving || state_->pasting || state_->confirmingClose
            || state_->documentLoading || state_->pdfExporting) return std::nullopt;
        StopWatching(state_);
        state_->fileRelocating = true;
        return ++state_->fileRelocationSerial;
    }

    void EditorDocumentController::FinishFileRelocation(std::uint64_t token, Windows::Storage::StorageFile const& file) {
        if (!state_->fileRelocating || state_->fileRelocationSerial != token) return;
        state_->fileRelocating = false;
        if (!state_->session) return;
        // SaveAs here changes file identity/base-directory only. The document
        // source, CST, undo transactions, selection and saved state stay intact.
        if (file) state_->session->SaveAs(file);
        if (state_->presenting) StartWatching(state_, state_->generation.load());
        if (file && state_->documentChanged) state_->documentChanged();
    }
}
