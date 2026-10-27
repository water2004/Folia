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
    void SavePng(std::filesystem::path const& path, std::uint32_t width, std::uint32_t height,
                 winrt::Windows::Storage::Streams::IBuffer const& pixels, std::uint32_t strideWidth, std::uint32_t firstPixel) {
        std::filesystem::create_directories(path.parent_path());
        ::Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        winrt::check_hresult(::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
        ::Microsoft::WRL::ComPtr<IWICStream> stream;
        winrt::check_hresult(factory->CreateStream(&stream));
        winrt::check_hresult(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE));
        ::Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
        winrt::check_hresult(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
        winrt::check_hresult(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
        ::Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        winrt::check_hresult(encoder->CreateNewFrame(&frame, nullptr));
        winrt::check_hresult(frame->Initialize(nullptr));
        winrt::check_hresult(frame->SetSize(width, height));
        auto format = GUID_WICPixelFormat32bppBGRA;
        winrt::check_hresult(frame->SetPixelFormat(&format));
        winrt::check_hresult(frame->WritePixels(height, (strideWidth ? strideWidth : width) * 4, pixels.Length() - firstPixel * 4, pixels.data() + firstPixel * 4));
        winrt::check_hresult(frame->Commit());
        winrt::check_hresult(encoder->Commit());
    }
}

// Use the production XAML application bootstrap and MainWindow. This test
// entry point owns the one window and closes it normally after the assertions.
namespace winrt::Folia::implementation {
    App::App() {}
    void App::OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&) {
        auto window = CreateMainWindow();
        visualWindow = window;
        window.Activate();
        window.DispatcherQueue().TryEnqueue([this, window] {
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
