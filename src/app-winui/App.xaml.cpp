#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "settings/AppSettings.h"
#include "media/MathSvgStore.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::Folia::implementation
{
    namespace
    {
        winrt::fire_and_forget ReclaimMathCacheAsync()
        {
            try
            {
                co_await winrt::resume_background();
                auto result = winrt::Folia::MathSvgStore::ReclaimOrphans();
                if (result.failed != 0)
                    ::OutputDebugStringW(L"Folia: some unused math cache files could not be reclaimed.\n");
            }
            catch (...)
            {
                ::OutputDebugStringW(L"Folia: math cache startup maintenance could not be scheduled.\n");
            }
        }

    }

    App::App()
    {
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                ::OutputDebugStringW((L"Folia.Unhandled: " + errorMessage + L"\n").c_str());
                __debugbreak();
            }
        });
#endif
    }

    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& e)
    {
        LaunchAsync();
    }

    winrt::fire_and_forget App::LaunchAsync() {
        auto lifetime = get_strong();
        try {
            auto request = ReadWorkspaceCommandLine(::GetCommandLineW());
            if (!request.newInstance) {
                std::array<wchar_t, 32768> executable{};
                auto length = ::GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
                if (!length || length == executable.size()) winrt::throw_last_error();
                activationInstance = Microsoft::Windows::AppLifecycle::AppInstance::FindOrRegisterForKey(
                    WorkspaceInstanceKey(std::wstring(executable.data(), length)));
                if (!activationInstance.IsCurrent()) {
                    co_await activationInstance.RedirectActivationToAsync(MakeWorkspaceActivation(request));
                    Exit();
                    co_return;
                }
                auto dispatcher = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
                activationSubscription = activationInstance.Activated(winrt::auto_revoke,
                    [weak = get_weak(), dispatcher](auto const&, auto const& args) {
                        auto request = ReadWorkspaceActivation(args);
                        dispatcher.TryEnqueue([weak, request = std::move(request)] {
                            if (auto app = weak.get()) app->OpenWorkspaceWindows(request);
                        });
                    });
            }
            ReclaimMathCacheAsync();
            ApplyLanguageOverride(LoadAppSettings().settings.languageId);
            OpenWorkspaceWindows(request);
        } catch (winrt::hresult_error const& error) {
            ::OutputDebugStringW((L"Folia.ActivationFailed: " + error.message() + L"\n").c_str());
            Exit();
        } catch (std::exception const& error) {
            ::OutputDebugStringW((L"Folia.ActivationFailed: " + winrt::to_hstring(error.what()) + L"\n").c_str());
            Exit();
        }
    }
}
