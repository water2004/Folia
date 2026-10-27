#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/interaction/EditorDocumentController.h"
#include "localization/Localization.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    void PumpUntil(std::function<bool()> const& done)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!done() && std::chrono::steady_clock::now() < deadline)
        {
            MSG message{};
            while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                ::TranslateMessage(&message);
                ::DispatchMessageW(&message);
            }
            ::MsgWaitForMultipleObjectsEx(0, nullptr, 2, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
        if (!done()) throw std::runtime_error("document load exceeded 10 seconds");
    }

    struct LoadingFixture
    {
        EditorSession session;
        EditorSurfaceRenderer renderer;
        EditorTextInputController textInput;
        EditorDocumentController controller;
        DWORD thread = ::GetCurrentThreadId();
        std::atomic_size_t wrongThread{0};
        bool finished = false;
        bool changed = false;
        bool timerStarted = false;
        winrt::hstring status;
        std::filesystem::path path;
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer timer{nullptr};

        explicit LoadingFixture(std::function<void(LoadingFixture&)> afterParsing = {},
            std::string_view source = "# Thread test\n\nInline $x+1$ and **text**.\n")
        {
            wchar_t temp[MAX_PATH]{};
            winrt::check_bool(::GetTempFileNameW(std::filesystem::temp_directory_path().c_str(), L"fol", 0, temp));
            path = temp;
            std::ofstream(path, std::ios::binary) << source;
            auto queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
            if (afterParsing)
            {
                timer = queue.CreateTimer();
                timer.Interval(std::chrono::milliseconds(200));
                timer.IsRepeating(false);
                timer.Tick([this, action = std::move(afterParsing)](auto const&, auto const&) { action(*this); });
            }
            controller.Attach(session, {renderer, textInput, {},
                [this](auto const& value)
                {
                    CheckThread();
                    status = value;
                    if (timer && !timerStarted && value == LocalizeFormat(L"StatusParsingFile",
                        {winrt::hstring(path.filename().wstring())}))
                    {
                        // Keep this tiny document's surface unavailable across
                        // several readiness retries, then finish on the UI queue.
                        timerStarted = true;
                        timer.Start();
                    }
                },
                [this](bool active, std::optional<double>, bool)
                {
                    CheckThread();
                    if (!active) finished = true;
                },
                [this] { CheckThread(); changed = true; }, {}, {}, {}});
        }

        ~LoadingFixture()
        {
            if (timer) timer.Stop();
            controller.Detach();
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }

        void CheckThread()
        {
            if (::GetCurrentThreadId() != thread)
            {
                ++wrongThread;
                // Match the thread-affinity check performed by XAML controls.
                throw winrt::hresult_wrong_thread();
            }
        }

        void Open()
        {
            controller.OpenDocumentPath(winrt::hstring(path.wstring()));
            PumpUntil([this] { return finished; });
            expect(wrongThread.load() == 0_u);
        }
    };

    suite document_loading_tests = []
    {
        "document load resumes on UI after waiting for surface readiness"_test = []
        {
            LoadingFixture fixture([](auto& value) { value.renderer.InitializeForPdf(); });
            fixture.Open();
            expect(fixture.timerStarted);
            expect(fixture.changed);
            expect(fixture.session.HasFile());
            expect(fixture.session.TextUtf8() == "# Thread test\n\nInline $x+1$ and **text**.\n");
        };

        "document load cancellation resumes on UI while surface is unavailable"_test = []
        {
            LoadingFixture fixture([](auto& value) { value.controller.CancelOperation(); });
            fixture.Open();
            expect(fixture.timerStarted);
            expect(!fixture.changed);
            expect(!fixture.session.HasFile());
            expect(fixture.status == Localize(L"StatusOpenCancelled"));
        };

        "plain document load resumes on UI after waiting for surface readiness"_test = []
        {
            LoadingFixture fixture([](auto& value) { value.renderer.InitializeForPdf(); },
                "# Plain document\n\nNo formulas.\n");
            fixture.Open();
            expect(fixture.timerStarted);
            expect(fixture.changed);
            expect(fixture.session.TextUtf8() == "# Plain document\n\nNo formulas.\n");
        };

        "document load with an already ready surface completes on UI"_test = []
        {
            LoadingFixture fixture;
            fixture.renderer.InitializeForPdf();
            fixture.Open();
            expect(fixture.changed);
            expect(fixture.session.HasFile());
        };

        "document load can reopen after cancellation during readiness wait"_test = []
        {
            LoadingFixture fixture([](auto& value) { value.controller.CancelOperation(); });
            fixture.Open();
            expect(!fixture.changed);
            fixture.renderer.InitializeForPdf();
            fixture.finished = false;
            fixture.Open();
            expect(fixture.changed);
            expect(fixture.session.HasFile());
        };
    };
}

int main(int argc, const char* argv[])
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    auto queue = winrt::Microsoft::UI::Dispatching::DispatcherQueueController::CreateOnCurrentThread();
    auto result = boost::ut::cfg<>.run({.argc = argc, .argv = argv});
    auto shutdown = queue.ShutdownQueueAsync();
    PumpUntil([&] { return shutdown.Status() != winrt::Windows::Foundation::AsyncStatus::Started; });
    shutdown.GetResults();
    queue = nullptr;
    winrt::uninit_apartment();
    return result;
}
