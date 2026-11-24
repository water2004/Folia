#pragma once
#include "workspace/WorkspaceDocuments.h"
#include "windows/storage/WorkspaceFileOperations.h"
namespace winrt::Folia {
    using folia::platform::WorkspaceFileAction;
    using folia::platform::WorkspaceFileProgressHandler;

    // Caller owns the workspace for this request and excludes new opens while
    // relocating. Existing document controllers lease their own save/watchers.
    Windows::Foundation::IAsyncOperation<winrt::hstring> ApplyWorkspaceDocumentFileActionAsync(
        WorkspaceDocuments& documents, WorkspaceFileAction action, WorkspaceFileProgressHandler progress = {});
}
