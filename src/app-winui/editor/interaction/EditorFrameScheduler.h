#pragma once

namespace winrt::Folia
{
    // Runs animation work during the WinUI render pass.
    struct EditorFrameScheduler
    {
        using FrameCallback = std::function<bool(float elapsedSeconds)>;

        EditorFrameScheduler() = default;
        EditorFrameScheduler(EditorFrameScheduler const&) = delete;
        EditorFrameScheduler& operator=(EditorFrameScheduler const&) = delete;
        ~EditorFrameScheduler();
        void Attach(FrameCallback frameCallback);
        void Detach();
        bool Running() const noexcept { return renderingToken.value != 0; }
        void Start();
        void Stop();

    private:
        struct InitialFrame { EditorFrameScheduler* owner; };
        void OnFrame();

        winrt::event_token renderingToken{};
        std::shared_ptr<InitialFrame> initialFrame;
        FrameCallback frameCallback;
        std::chrono::steady_clock::time_point lastFrame{};
        bool compositorBoostActive = false;
    };
}
