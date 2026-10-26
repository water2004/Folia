#include "pch.h"
#include "support/folia_test.hpp"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "visual_window.h"

namespace {
    winrt::Microsoft::UI::Xaml::Window visualWindow{nullptr};
    int visualResult = 1;
}
namespace visual_tests {
    winrt::Microsoft::UI::Xaml::Window Window() { return visualWindow; }
}

// Use the production XAML application bootstrap and MainWindow. This test
// entry point owns the one window and closes it normally after the assertions.
namespace winrt::Folia::implementation {
    App::App() {}
    void App::OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&) {
        window = winrt::make<MainWindow>();
        visualWindow = window;
        window.Activate();
        window.DispatcherQueue().TryEnqueue([this] {
            std::vector<std::string> utf8Arguments;
            for (int i = 0; i < __argc; ++i) utf8Arguments.push_back(winrt::to_string(__wargv[i]));
            std::vector<char const*> arguments;
            for (auto const& argument : utf8Arguments) arguments.push_back(argument.c_str());
            visualResult = boost::ut::cfg<>.run({.argc = __argc, .argv = arguments.data()});
            std::array<wchar_t, 32768> path{};
            if (::GetEnvironmentVariableW(L"FOLIA_VISUAL_TEST_RESULT", path.data(), static_cast<DWORD>(path.size())))
                std::ofstream(path.data()) << visualResult;
            visualWindow = nullptr;
            window.Close();
        });
    }
}
