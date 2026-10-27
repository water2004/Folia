#include "pch.h"
import std;
#include "workspace/WorkspaceLaunch.h"
#include <shellapi.h>

namespace winrt::Folia {
    namespace {
        // Standard WinRT activation data carries absolute paths by value. The
        // receiving UI never needs the launching process's working directory.
        struct WorkspaceActivationArguments : winrt::implements<WorkspaceActivationArguments,
            Microsoft::Windows::AppLifecycle::IAppActivationArguments> {
            Windows::Foundation::Collections::PropertySet data;
            explicit WorkspaceActivationArguments(WorkspaceLaunchRequest const& request) {
                std::vector<winrt::hstring> paths;
                for (auto const& path : request.documents) paths.emplace_back(path.native());
                data.Insert(L"folia.workspace", winrt::box_value(std::uint32_t{1}));
                data.Insert(L"documents", Windows::Foundation::PropertyValue::CreateStringArray(paths));
            }
            Microsoft::Windows::AppLifecycle::ExtendedActivationKind Kind() const {
                return Microsoft::Windows::AppLifecycle::ExtendedActivationKind::Launch;
            }
            Windows::Foundation::IInspectable Data() const { return data; }
        };
    }
    WorkspaceLaunchRequest ReadWorkspaceCommandLine(std::wstring_view commandLine) {
        WorkspaceLaunchRequest request;
        if (commandLine.empty()) return request;
        int count = 0;
        auto args = ::CommandLineToArgvW(std::wstring(commandLine).c_str(), &count);
        if (!args) winrt::throw_last_error();
        struct Release { wchar_t** args; ~Release() { ::LocalFree(args); } } release{args};
        bool options = true;
        for (int i = 1; i < count; ++i) {
            std::wstring_view value{args[i]};
            if (options && value == L"--new-instance") { request.newInstance = true; continue; }
            if (options && value == L"--") { options = false; continue; }
            if (!value.empty()) request.documents.push_back(std::filesystem::absolute(value).lexically_normal());
        }
        return request;
    }
    WorkspaceLaunchRequest ReadWorkspaceActivation(Microsoft::Windows::AppLifecycle::AppActivationArguments const& args) {
        WorkspaceLaunchRequest request;
        if (auto properties = args.Data().try_as<Windows::Foundation::Collections::IPropertySet>();
            properties && properties.HasKey(L"folia.workspace")) {
            if (winrt::unbox_value<std::uint32_t>(properties.Lookup(L"folia.workspace")) != 1)
                throw std::invalid_argument("Unsupported workspace activation version");
            winrt::com_array<winrt::hstring> paths;
            properties.Lookup(L"documents").as<Windows::Foundation::IPropertyValue>().GetStringArray(paths);
            for (auto const& path : paths) {
                std::filesystem::path file{path.c_str()};
                if (!file.is_absolute()) throw std::invalid_argument("Workspace activation requires absolute file paths");
                request.documents.push_back(std::move(file));
            }
        } else if (args.Kind() == Microsoft::Windows::AppLifecycle::ExtendedActivationKind::Launch) {
            request = ReadWorkspaceCommandLine(args.Data().as<Windows::ApplicationModel::Activation::ILaunchActivatedEventArgs>().Arguments());
        } else if (args.Kind() == Microsoft::Windows::AppLifecycle::ExtendedActivationKind::File) {
            for (auto const& file : args.Data().as<Windows::ApplicationModel::Activation::IFileActivatedEventArgs>().Files())
                request.documents.emplace_back(file.Path().c_str());
        }
        return request;
    }
    Microsoft::Windows::AppLifecycle::AppActivationArguments MakeWorkspaceActivation(WorkspaceLaunchRequest const& request) {
        return winrt::make<WorkspaceActivationArguments>(request).as<Microsoft::Windows::AppLifecycle::AppActivationArguments>();
    }
    winrt::hstring WorkspaceInstanceKey(std::filesystem::path const& executable) {
        auto path = std::filesystem::absolute(executable).lexically_normal().native();
        ::CharLowerBuffW(path.data(), static_cast<DWORD>(path.size()));
        // Keep kernel-object names bounded even for a long installation path.
        std::uint64_t key = 14695981039346656037ull;
        for (auto character : path) { key ^= static_cast<std::uint16_t>(character); key *= 1099511628211ull; }
        return L"Folia.Workspace." + winrt::to_hstring(key);
    }
}
