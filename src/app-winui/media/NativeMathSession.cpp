#include "pch.h"

import std;
#include "media/NativeMathSession.h"

#include "folia/math/native_renderer_bridge.h"
#include "media/MathJaxSvgParser.h"

namespace winrt::Folia
{
namespace
{
using RendererHandle =
    std::unique_ptr<FoliaNativeMathRenderer, decltype(&FoliaNativeMathRendererDestroy)>;
using CancellationHandle =
    std::unique_ptr<FoliaNativeMathCancellation, decltype(&FoliaNativeMathCancellationDestroy)>;
using ResultHandle =
    std::unique_ptr<FoliaNativeMathResult, decltype(&FoliaNativeMathResultDestroy)>;

MathJaxSvg InfrastructureFailure(std::string message, bool display)
{
    MathJaxSvg result;
    result.display = display;
    result.error = std::move(message);
    result.errorKind = MathJaxErrorKind::Infrastructure;
    return result;
}
} // namespace

struct NativeMathSession::State
{
    State() : owner(std::this_thread::get_id()), renderer(nullptr, &FoliaNativeMathRendererDestroy)
    {
    }

    ~State()
    {
        // The bridge requires renderer destruction on the creating thread.
        if (renderer && owner != std::this_thread::get_id())
            std::terminate();
    }

    void RequireOwnerThread() const
    {
        if (owner != std::this_thread::get_id())
            throw std::logic_error("NativeMathSession must be used on its creating thread");
    }

    std::thread::id owner;
    RendererHandle renderer;
};

NativeMathSession::NativeMathSession() : state(std::make_unique<State>()) {}

NativeMathSession::~NativeMathSession() = default;

MathJaxSvg NativeMathSession::Render(std::string_view tex, bool display, float em,
                                     float containerWidth, std::stop_token stop)
{
    const NativeMathInput input{tex, display, em, containerWidth};
    auto result = RenderBatch(std::span{&input, std::size_t{1}}, stop);
    return std::move(result.front());
}

std::vector<MathJaxSvg> NativeMathSession::RenderBatch(std::span<NativeMathInput const> inputs,
                                                       std::stop_token stop)
{
    state->RequireOwnerThread();
    if (stop.stop_requested())
        throw winrt::hresult_canceled();
    if (inputs.empty())
        return {};
    auto failed = [&](std::string message)
    {
        std::vector<MathJaxSvg> results;
        results.reserve(inputs.size());
        for (auto const &input : inputs)
            results.push_back(InfrastructureFailure(message, input.display));
        return results;
    };
    try
    {
        if (!state->renderer)
        {
            FoliaNativeMathRenderer *created = nullptr;
            const auto status = FoliaNativeMathRendererCreate(&created);
            state->renderer.reset(created);
            if (status == FOLIA_NATIVE_MATH_OUT_OF_MEMORY)
                throw std::bad_alloc();
            if (status != FOLIA_NATIVE_MATH_OK || !state->renderer)
            {
                state->renderer.reset();
                return failed("Native math renderer could not be created");
            }
        }
        FoliaNativeMathCancellation *rawCancellation = nullptr;
        const auto cancelStatus = FoliaNativeMathCancellationCreate(&rawCancellation);
        CancellationHandle cancellation{rawCancellation, &FoliaNativeMathCancellationDestroy};
        if (cancelStatus == FOLIA_NATIVE_MATH_OUT_OF_MEMORY)
            throw std::bad_alloc();
        if (cancelStatus != FOLIA_NATIVE_MATH_OK || !cancellation)
        {
            state->renderer.reset();
            return failed("Native math cancellation handle could not be created");
        }
        std::vector<std::vector<std::uint16_t>> texts;
        std::vector<FoliaNativeMathInput> requests;
        std::vector<FoliaNativeMathOutput> outputs(inputs.size());
        std::vector<ResultHandle> ownedResults;
        texts.reserve(inputs.size());
        requests.reserve(inputs.size());
        ownedResults.reserve(inputs.size());
        for (auto const &input : inputs)
        {
            auto text = winrt::to_hstring(input.tex);
            std::vector<std::uint16_t> units;
            units.reserve(text.size());
            for (auto unit : text)
                units.push_back(static_cast<std::uint16_t>(unit));
            texts.push_back(std::move(units));
            requests.push_back({texts.back().data(), texts.back().size(), input.display ? 1 : 0,
                                input.em, input.containerWidth, cancellation.get(), 2000});
            ownedResults.emplace_back(nullptr, &FoliaNativeMathResultDestroy);
        }
        FoliaNativeMathStatus status;
        {
            std::stop_callback cancel(stop, [handle = cancellation.get()]
                                      { FoliaNativeMathCancellationRequestStop(handle); });
            status = FoliaNativeMathRenderSvgBatch(state->renderer.get(), requests.data(),
                                                   requests.size(), outputs.data());
        }
        for (std::size_t i = 0; i < outputs.size(); ++i)
            ownedResults[i].reset(outputs[i].result);
        if (status == FOLIA_NATIVE_MATH_OUT_OF_MEMORY)
            throw std::bad_alloc();
        if (stop.stop_requested())
            throw winrt::hresult_canceled();
        if (status != FOLIA_NATIVE_MATH_OK)
        {
            state->renderer.reset();
            return failed("Native math batch failed");
        }
        for (auto const &output : outputs)
        {
            if (output.status == FOLIA_NATIVE_MATH_OUT_OF_MEMORY)
                throw std::bad_alloc();
            if (output.status == FOLIA_NATIVE_MATH_CANCELLED ||
                output.status == FOLIA_NATIVE_MATH_SESSION_UNUSABLE)
                throw winrt::hresult_canceled();
        }
        std::vector<MathJaxSvg> results;
        results.reserve(inputs.size());
        bool resetRenderer = false;
        for (std::size_t i = 0; i < inputs.size(); ++i)
        {
            const auto &input = inputs[i];
            auto *nativeResult = ownedResults[i].get();
            if (outputs[i].status != FOLIA_NATIVE_MATH_OK)
            {
                std::size_t size = 0;
                const auto *bytes = FoliaNativeMathResultError(nativeResult, &size);
                results.push_back(InfrastructureFailure(bytes ? std::string(bytes, size)
                                                              : "Native math renderer failed",
                                                        input.display));
                resetRenderer = true;
                continue;
            }
            std::size_t size = 0;
            const auto *bytes = FoliaNativeMathResultSvg(nativeResult, &size);
            if (!bytes && size)
                throw std::runtime_error("Native renderer returned an invalid SVG result");
            try
            {
                results.push_back(ParseMathJaxSvgOutput(std::string_view(bytes ? bytes : "", size),
                                                        input.display, input.em, input.containerWidth));
            }
            catch (std::bad_alloc const &)
            {
                throw;
            }
            catch (std::exception const &error)
            {
                results.push_back(InfrastructureFailure(error.what(), input.display));
                resetRenderer = true;
            }
        }
        if (resetRenderer)
            state->renderer.reset();
        return results;
    }
    catch (std::bad_alloc const &)
    {
        state->renderer.reset();
        throw;
    }
    catch (winrt::hresult_canceled const &)
    {
        state->renderer.reset();
        throw;
    }
    catch (std::exception const &error)
    {
        state->renderer.reset();
        return failed(error.what());
    }
    catch (...)
    {
        state->renderer.reset();
        return failed("Unknown native math renderer failure");
    }
}
} // namespace winrt::Folia
