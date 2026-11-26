#include "pch.h"
#include "editor/interaction/EditorFrameScheduler.h"

namespace winrt::Folia
{
    namespace
    {
        using Boost = HRESULT(WINAPI*)(BOOL);

        Boost CompositorBoost()
        {
            struct Library {
                HMODULE module = LoadLibraryExW(L"dcomp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                Boost boost = module ? reinterpret_cast<Boost>(GetProcAddress(module, "DCompositionBoostCompositorClock")) : nullptr;
                ~Library() { if (module) FreeLibrary(module); }
            };
            static const Library library;
            return library.boost;
        }
    }

    EditorFrameScheduler::~EditorFrameScheduler()
    {
        Stop();
    }

    void EditorFrameScheduler::Attach(FrameCallback callback)
    {
        Stop();
        frameCallback = std::move(callback);
    }

    void EditorFrameScheduler::Detach()
    {
        Stop();
        frameCallback = {};
    }

    void EditorFrameScheduler::Start()
    {
        if (!frameCallback || renderingToken.value) return;

        // Publish animated editor surfaces in the same rendering pass as the
        // XAML scene. An independent display wait can run at the right rate
        // and still submit between commits, causing the compositor to merge frames.
        lastFrame = std::chrono::steady_clock::now();
        renderingToken = Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(
            [this](auto const&, auto const&) { OnFrame(); });
        if (auto boost = CompositorBoost()) compositorBoostActive = SUCCEEDED(boost(TRUE));

        // Give the input that starts motion a response even if normal-priority
        // work is queued ahead of the first XAML rendering pass. The first
        // callback consumes this request, and later frames use only XAML.
        initialFrame = std::make_shared<InitialFrame>(this);
        auto queue = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        if (!queue.TryEnqueue([pending = std::weak_ptr{initialFrame}] {
            if (auto frame = pending.lock()) frame->owner->OnFrame();
        })) initialFrame.reset();
    }

    void EditorFrameScheduler::Stop()
    {
        if (!renderingToken.value) return;
        initialFrame.reset();
        Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(std::exchange(renderingToken, {}));
        if (std::exchange(compositorBoostActive, false)) CompositorBoost()(FALSE);
    }

    void EditorFrameScheduler::OnFrame()
    {
        if (!renderingToken.value) return;
        initialFrame.reset();
        auto token = renderingToken;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<float>(now - std::exchange(lastFrame, now)).count();
        if (!frameCallback(elapsed) && token.value == renderingToken.value) Stop();
    }
}
