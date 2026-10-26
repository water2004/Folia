#include "pch.h"
import std;
#include "workspace/WorkspaceFileCoordinator.h"

namespace winrt::Folia {
    Windows::Foundation::IAsyncOperation<winrt::hstring> ApplyWorkspaceDocumentFileActionAsync(
        WorkspaceDocuments& documents, WorkspaceFileAction action, WorkspaceFileProgressHandler progress) {
        struct Lease {
            std::shared_ptr<WorkspaceDocument> document;
            std::filesystem::path target;
            std::uint64_t token;
            Windows::Storage::StorageFile file{nullptr};
        };
        std::vector<Lease> leases;
        struct Release {
            std::vector<Lease>& leases;
            ~Release() { for (auto& lease : leases) lease.document->controller.FinishFileRelocation(lease.token); }
        } release{leases};
        for (auto const& document : documents.Items()) if (auto target = RelocatedWorkspacePath(action, document->path)) {
            if (document->opening || !document->loaded) throw winrt::hresult_illegal_method_call(L"Wait for the document to finish opening");
            auto token = document->controller.BeginFileRelocation();
            if (!token) throw winrt::hresult_illegal_method_call(L"The document is busy; try again after it finishes");
            leases.push_back({document, *target, *token});
        }
        auto result = co_await ApplyWorkspaceFileActionAsync(action, std::move(progress));
        std::exception_ptr failure;
        try {
            for (auto& lease : leases) lease.file = co_await Windows::Storage::StorageFile::GetFileFromPathAsync(lease.target.native());
        } catch (...) { failure = std::current_exception(); }
        if (failure) {
            // No document metadata has been committed. Restore the original
            // paths without overwriting any file created in the meantime.
            WorkspaceFileAction rollback{WorkspaceFileActionKind::Move, action.target, action.source, action.directory};
            co_await ApplyWorkspaceFileActionAsync(rollback);
            std::rethrow_exception(failure);
        }
        for (auto& lease : leases) {
            lease.document->controller.FinishFileRelocation(lease.token, lease.file);
            documents.RefreshPath(lease.document->id);
        }
        co_return result;
    }
}
