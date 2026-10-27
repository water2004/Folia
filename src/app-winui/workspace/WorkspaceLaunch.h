#pragma once
#include <winrt/Microsoft.Windows.AppLifecycle.h>

namespace winrt::Folia {
    struct WorkspaceLaunchRequest {
        bool newInstance = false;
        std::vector<std::filesystem::path> documents;
    };
    WorkspaceLaunchRequest ReadWorkspaceCommandLine(std::wstring_view commandLine);
    WorkspaceLaunchRequest ReadWorkspaceActivation(Microsoft::Windows::AppLifecycle::AppActivationArguments const& args);
    Microsoft::Windows::AppLifecycle::AppActivationArguments MakeWorkspaceActivation(WorkspaceLaunchRequest const& request);
    winrt::hstring WorkspaceInstanceKey(std::filesystem::path const& executable);
}
