#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "settings/AppSettings.h"
#include "media/MathSvgStore.h"
#include <shellapi.h>

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

        std::optional<winrt::hstring> InitialDocumentPath()
        {
            struct LocalArguments
            {
                wchar_t** value{};
                ~LocalArguments() { if (value) ::LocalFree(value); }
            };
            int argumentCount = 0;
            LocalArguments arguments{ ::CommandLineToArgvW(::GetCommandLineW(), &argumentCount) };
            if (!arguments.value) return std::nullopt;
            if (argumentCount < 2 || !arguments.value[1] || arguments.value[1][0] == L'\0')
                return std::nullopt;
            return winrt::hstring{ arguments.value[1] };
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
        ReclaimMathCacheAsync();
        ApplyLanguageOverride(LoadAppSettings().settings.languageId);
        auto initialDocument = InitialDocumentPath();
        window = initialDocument
            ? make<MainWindow>(*initialDocument)
            : make<MainWindow>();
        window.Activate();
        ::OutputDebugStringW(L"Folia.StartupComplete\n");
    }
}
