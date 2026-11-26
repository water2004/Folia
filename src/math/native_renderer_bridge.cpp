#include "folia/math/native_renderer_bridge.h"

import folia.math;
import folia.math.output.svg.writer;
import std;

struct FoliaNativeMathRenderer {
    std::thread::id owner = std::this_thread::get_id();
    folia::math::Renderer renderer;
};

struct FoliaNativeMathCancellation {
    std::stop_source source;
};

struct FoliaNativeMathResult {
    std::string svg;
    std::string error;
    std::optional<FoliaNativeMathSvgMetrics> fragmentMetrics;
    std::optional<double> reusableWidthFrom;
};

extern "C" FoliaNativeMathStatus
FoliaNativeMathRendererCreate(FoliaNativeMathRenderer **out_renderer) {
    if (out_renderer)
        *out_renderer = nullptr;
    if (!out_renderer)
        return FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR;
    try {
        auto renderer = std::make_unique<FoliaNativeMathRenderer>();
        *out_renderer = renderer.release();
        return FOLIA_NATIVE_MATH_OK;
    } catch (std::bad_alloc const &) {
        return FOLIA_NATIVE_MATH_OUT_OF_MEMORY;
    } catch (...) {
        return FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR;
    }
}

extern "C" void FoliaNativeMathRendererDestroy(FoliaNativeMathRenderer *renderer) {
    delete renderer;
}

extern "C" FoliaNativeMathStatus
FoliaNativeMathCancellationCreate(FoliaNativeMathCancellation **out_cancellation) {
    if (out_cancellation)
        *out_cancellation = nullptr;
    if (!out_cancellation)
        return FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR;
    try {
        auto cancellation = std::make_unique<FoliaNativeMathCancellation>();
        *out_cancellation = cancellation.release();
        return FOLIA_NATIVE_MATH_OK;
    } catch (std::bad_alloc const &) {
        return FOLIA_NATIVE_MATH_OUT_OF_MEMORY;
    } catch (...) {
        return FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR;
    }
}

extern "C" void FoliaNativeMathCancellationRequestStop(FoliaNativeMathCancellation *cancellation) {
    if (cancellation)
        (void)cancellation->source.request_stop();
}

extern "C" void FoliaNativeMathCancellationDestroy(FoliaNativeMathCancellation *cancellation) {
    delete cancellation;
}

namespace {
std::atomic<std::uint64_t> nextFragmentId{1};

FoliaNativeMathStatus renderSvgBatch(FoliaNativeMathRenderer *renderer,
                                    FoliaNativeMathInput const *inputs, size_t count,
                                    FoliaNativeMathOutput *outputs, bool fragment) {
    if (count && !outputs)
        return FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR;
    for (size_t i = 0; i < count; ++i)
        outputs[i] = {FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR, nullptr};
    if (!renderer || (count && !inputs) || renderer->owner != std::this_thread::get_id())
        return FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR;
    try {
        std::vector<std::u16string> texts;
        std::vector<folia::math::RenderInput> requests;
        std::vector<size_t> indexes;
        std::vector<std::unique_ptr<FoliaNativeMathResult>> results(count);
        std::vector<FoliaNativeMathStatus> statuses(count, FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR);
        texts.reserve(count);
        requests.reserve(count);
        indexes.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            const auto &input = inputs[i];
            if (!input.cancellation || (input.tex_units && !input.tex) || input.display < 0 ||
                input.display > 1 || !std::isfinite(input.em) || input.em <= 0 ||
                !std::isfinite(input.container_width) || input.container_width <= 0)
                continue;
            std::u16string text;
            text.reserve(input.tex_units);
            for (size_t at = 0; at < input.tex_units; ++at)
                text.push_back(static_cast<char16_t>(input.tex[at]));
            texts.push_back(std::move(text));
            folia::math::RenderOptions options;
            options.display = input.display != 0;
            options.em = input.em;
            options.containerWidth = input.container_width;
            if (fragment) {
                options.svgMode = folia::math::SvgMode::standalone_display;
                options.svgIdPrefix =
                    "folia-math-" +
                    std::to_string(nextFragmentId.fetch_add(1, std::memory_order_relaxed)) + "-";
            }
            requests.push_back({texts.back(), input.cancellation->source.get_token(), options,
                                std::chrono::milliseconds(input.timeout_ms)});
            indexes.push_back(i);
        }
        auto rendered = renderer->renderer.renderBatch(requests);
        for (size_t i = 0; i < rendered.size(); ++i) {
            const auto target = indexes[i];
            auto result = std::make_unique<FoliaNativeMathResult>();
            try {
                if (rendered[i].error)
                    std::rethrow_exception(rendered[i].error);
                if (!rendered[i].result || rendered[i].result->svg.empty() ||
                    rendered[i].result->svg.find("<svg") == std::string::npos)
                    result->error = "Native renderer completed without SVG output";
                else {
                    if (fragment) {
                        const auto metrics = folia::math::output::svg::pixel_metrics(
                            rendered[i].result->scene);
                        result->fragmentMetrics =
                            FoliaNativeMathSvgMetrics{metrics.width, metrics.height,
                                                     metrics.baseline};
                    }
                    result->reusableWidthFrom = rendered[i].result->reusableWidthFrom;
                    result->svg = std::move(rendered[i].result->svg);
                    statuses[target] = FOLIA_NATIVE_MATH_OK;
                }
            } catch (folia::math::RenderError const &error) {
                result->error = error.what();
                statuses[target] = error.name == "Cancelled" ? FOLIA_NATIVE_MATH_CANCELLED
                                   : error.name == "SessionUnusable"
                                       ? FOLIA_NATIVE_MATH_SESSION_UNUSABLE
                                       : FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR;
            } catch (std::bad_alloc const &) {
                statuses[target] = FOLIA_NATIVE_MATH_OUT_OF_MEMORY;
                result.reset();
            } catch (std::exception const &error) {
                result->error = error.what();
            } catch (...) {
                result->error = "Unknown native math renderer failure";
            }
            results[target] = std::move(result);
        }
        for (size_t i = 0; i < count; ++i)
            outputs[i] = {statuses[i], results[i].release()};
        return FOLIA_NATIVE_MATH_OK;
    } catch (std::bad_alloc const &) {
        return FOLIA_NATIVE_MATH_OUT_OF_MEMORY;
    } catch (...) {
        return FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR;
    }
}
} // namespace

extern "C" FoliaNativeMathStatus FoliaNativeMathRenderSvgBatch(
    FoliaNativeMathRenderer *renderer, FoliaNativeMathInput const *inputs, size_t count,
    FoliaNativeMathOutput *outputs) {
    return renderSvgBatch(renderer, inputs, count, outputs, false);
}

extern "C" FoliaNativeMathStatus
FoliaNativeMathRenderSvg(FoliaNativeMathRenderer *renderer, uint16_t const *tex, size_t tex_units,
                         int display, double em, double container_width,
                         FoliaNativeMathCancellation *cancellation, unsigned int timeout_ms,
                         FoliaNativeMathResult **out_result) {
    if (!out_result)
        return FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR;
    *out_result = nullptr;
    const FoliaNativeMathInput input{tex,          tex_units, display, em, container_width,
                                     cancellation, timeout_ms};
    FoliaNativeMathOutput output{};
    const auto status = FoliaNativeMathRenderSvgBatch(renderer, &input, 1, &output);
    if (status != FOLIA_NATIVE_MATH_OK)
        return status;
    *out_result = output.result;
    return output.status;
}

extern "C" FoliaNativeMathStatus FoliaNativeMathRenderSvgFragment(
    FoliaNativeMathRenderer *renderer, uint16_t const *tex, size_t tex_units, double em,
    double container_width, FoliaNativeMathCancellation *cancellation, unsigned int timeout_ms,
    FoliaNativeMathResult **out_result) {
    if (!out_result)
        return FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR;
    *out_result = nullptr;
    const FoliaNativeMathInput input{tex, tex_units, 1, em, container_width, cancellation,
                                     timeout_ms};
    FoliaNativeMathOutput output{};
    const auto status = renderSvgBatch(renderer, &input, 1, &output, true);
    if (status != FOLIA_NATIVE_MATH_OK)
        return status;
    *out_result = output.result;
    return output.status;
}

extern "C" int FoliaNativeMathResultFragmentMetrics(FoliaNativeMathResult const *result,
                                                     FoliaNativeMathSvgMetrics *metrics) {
    if (!metrics)
        return 0;
    *metrics = {};
    if (!result || !result->fragmentMetrics)
        return 0;
    *metrics = *result->fragmentMetrics;
    return 1;
}

extern "C" int FoliaNativeMathResultReusableWidthFrom(FoliaNativeMathResult const *result, double *width) {
    if (!result || !width || !result->reusableWidthFrom) return 0;
    *width = *result->reusableWidthFrom;
    return 1;
}

extern "C" char const *FoliaNativeMathResultSvg(FoliaNativeMathResult const *result, size_t *size) {
    if (size)
        *size = result ? result->svg.size() : 0;
    return result ? result->svg.data() : nullptr;
}

extern "C" char const *FoliaNativeMathResultError(FoliaNativeMathResult const *result,
                                                  size_t *size) {
    if (size)
        *size = result ? result->error.size() : 0;
    return result ? result->error.data() : nullptr;
}

extern "C" void FoliaNativeMathResultDestroy(FoliaNativeMathResult *result) { delete result; }
