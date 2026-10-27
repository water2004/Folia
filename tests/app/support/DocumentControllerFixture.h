#pragma once

#include "editor/interaction/EditorDocumentController.h"
#include "localization/Localization.h"
#include <winrt/Windows.Graphics.Imaging.h>

namespace document_tests {
    using namespace winrt::Folia;
    using winrt::Windows::Storage::StorageFile;
    using winrt::Windows::Foundation::IAsyncOperation;
    using winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult;

    template<class Done> void Pump(Done done) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!done() && std::chrono::steady_clock::now() < deadline) {
            MSG message{};
            while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                ::TranslateMessage(&message);
                ::DispatchMessageW(&message);
            }
            ::MsgWaitForMultipleObjectsEx(0, nullptr, 1, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
        if (!done()) throw std::runtime_error("document operation exceeded ten seconds");
    }
    template<class Operation> auto Await(Operation const& operation) {
        Pump([&] { return operation.Status() != winrt::Windows::Foundation::AsyncStatus::Started; });
        return operation.GetResults();
    }

    struct TestDirectory {
        std::filesystem::path root = std::filesystem::temp_directory_path() / L"folia-editor-document-tests";
        std::filesystem::path path;
        TestDirectory() {
            GUID id{};
            winrt::check_hresult(CoCreateGuid(&id));
            wchar_t name[40]{};
            winrt::check_bool(StringFromGUID2(id, name, 40) != 0);
            path = root / name;
            std::filesystem::create_directories(root);
            if (!std::filesystem::create_directory(path)) throw std::runtime_error("test directory already exists");
        }
        ~TestDirectory() {
            // Delete only this freshly created run directory, never the shared root.
            if (path.parent_path() == root && path.filename().wstring().starts_with(L"{")) {
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            }
        }
    };

    struct Fixture {
        TestDirectory directory;
        EditorSession session;
        EditorSurfaceRenderer renderer;
        EditorTextInputController input;
        EditorDocumentController controller;
        StorageFile saveTarget{nullptr};
        ContentDialogResult choice = ContentDialogResult::None;
        int prompts = 0, pickers = 0;
        winrt::hstring status;
        std::function<void()> saved;
        DWORD thread = ::GetCurrentThreadId();
        bool wrongThread = false;
        bool loading = false;
        bool loadingOverlay = false;
        std::vector<bool> loadingAtCompletion;
        std::vector<bool> loadingAtDocumentChange;
        std::function<void(bool)> progress;

        StorageFile File(std::wstring_view name, std::string_view source = {}) {
            auto path = directory.path / name;
            std::ofstream file(path, std::ios::binary);
            file << source;
            file.close();
            return Await(StorageFile::GetFileFromPathAsync(path.wstring()));
        }
        explicit Fixture(bool hasFile = true, winrt::hstring const& text = L"initial\n", bool initializeRenderer = true) {
            if (hasFile) {
                auto file = File(L"document 中文.md", winrt::to_string(text));
                session.Open(file, text, {}, {});
                saveTarget = file;
            } else session.SetText(text);
            if (initializeRenderer) renderer.InitializeForPdf();
            controller.Attach(session, {renderer, input,
                [this](auto const& command) {
                    wrongThread |= ::GetCurrentThreadId() != thread;
                    return session.ExecuteCommand(command);
                }, [this](auto const& value) {
                    wrongThread |= ::GetCurrentThreadId() != thread;
                    status = value;
                    if (saved && value == winrt::Folia::LocalizeFormat(L"StatusSavedFile", {session.DisplayName()})) {
                        auto action = saved;
                        action();
                    }
                }, [this](bool active, auto, bool) {
                    wrongThread |= ::GetCurrentThreadId() != thread;
                    loading = active;
                    // MainWindow refreshes its overlay from IsLoading inside
                    // this notification, rather than after the coroutine ends.
                    loadingOverlay = controller.IsLoading();
                    if (!active) loadingAtCompletion.push_back(loadingOverlay);
                    if (progress) progress(active);
                }, [this] { loadingAtDocumentChange.push_back(controller.IsLoading()); }, {}, {}, {}});
            controller.SetSaveDialogs([this]() -> IAsyncOperation<ContentDialogResult> {
                ++prompts;
                co_return choice;
            }, [this]() -> IAsyncOperation<StorageFile> {
                ++pickers;
                co_return saveTarget;
            });
        }
        ~Fixture() { controller.Detach(); }
        std::vector<std::filesystem::path> Images() const {
            std::vector<std::filesystem::path> images;
            for (auto const& entry : std::filesystem::directory_iterator(directory.path))
                if (entry.path().extension() == L".png" && entry.path().filename().wstring().starts_with(L"image-")) images.push_back(entry.path());
            return images;
        }
        std::string DiskText() const {
            std::ifstream file(session.Path().c_str(), std::ios::binary);
            return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
        }
        void Type(std::u32string text) {
            if (!session.ExecuteCommand(folia::Command::InsertText(std::move(text)))) throw std::runtime_error("test edit was rejected");
        }
    };

    inline IAsyncOperation<winrt::Windows::Storage::Streams::IRandomAccessStream> Image() {
        using namespace winrt::Windows::Graphics::Imaging;
        winrt::Windows::Storage::Streams::InMemoryRandomAccessStream stream;
        auto encoder = co_await BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(), stream);
        std::array<std::uint8_t, 24> pixels{};
        for (std::size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i] = 32; pixels[i+1] = 64; pixels[i+2] = 128; pixels[i+3] = 255;
        }
        encoder.SetPixelData(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Straight, 2, 3, 96, 96, pixels);
        co_await encoder.FlushAsync();
        stream.Seek(0);
        co_return stream;
    }
    inline auto Package(winrt::Windows::Storage::Streams::IRandomAccessStream const& image) {
        winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
        package.SetBitmap(winrt::Windows::Storage::Streams::RandomAccessStreamReference::CreateFromStream(image));
        package.SetText(L"Text fallback must not replace an image.");
        return package;
    }
}
