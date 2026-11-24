#pragma once

#include <memory>
#include <stop_token>
#include <string_view>

#include "windows/media/math/MathJaxRenderer.h"

namespace folia::platform
{
    struct NativeMathInput
    {
        std::string_view tex;
        bool display = false;
        float em = 0.0f;
        float containerWidth = 0.0f;
    };
    class NativeMathSession
    {
    public:
        NativeMathSession();
        ~NativeMathSession();

        NativeMathSession(NativeMathSession const&) = delete;
        NativeMathSession& operator=(NativeMathSession const&) = delete;
        NativeMathSession(NativeMathSession&&) = delete;
        NativeMathSession& operator=(NativeMathSession&&) = delete;

        MathJaxSvg Render(
            std::string_view tex,
            bool display,
            float em,
            float containerWidth,
            std::stop_token stop = {});
        std::vector<MathJaxSvg> RenderBatch(std::span<NativeMathInput const> inputs,
            std::stop_token stop = {});

    private:
        struct State;
        std::unique_ptr<State> state;
    };
}
