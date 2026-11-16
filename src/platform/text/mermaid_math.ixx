module;
#include <folia/math/native_renderer_bridge.h>

export module folia.platform.mermaid_math;
import std;
import folia.mermaid.math_label;

export namespace folia::platform {

// A diagram owns one lazy MathJax session on its rendering thread. The existing
// shared Math DLL owns all TeX and font data; this bridge owns only C handles.
class MermaidMathRenderer {
    using Renderer = std::unique_ptr<FoliaNativeMathRenderer,
        decltype(&FoliaNativeMathRendererDestroy)>;
    using Cancellation = std::unique_ptr<FoliaNativeMathCancellation,
        decltype(&FoliaNativeMathCancellationDestroy)>;
    using Result = std::unique_ptr<FoliaNativeMathResult,
        decltype(&FoliaNativeMathResultDestroy)>;
    Renderer renderer_{nullptr, FoliaNativeMathRendererDestroy};
    Cancellation cancellation_{nullptr, FoliaNativeMathCancellationDestroy};

    std::expected<void, std::string> initialize() {
        if (renderer_) return {};
        FoliaNativeMathRenderer* renderer = nullptr;
        if (FoliaNativeMathRendererCreate(&renderer) != FOLIA_NATIVE_MATH_OK)
            return std::unexpected("Could not create the Mermaid MathJax session");
        Renderer owned{renderer, FoliaNativeMathRendererDestroy};
        FoliaNativeMathCancellation* cancellation = nullptr;
        if (FoliaNativeMathCancellationCreate(&cancellation) != FOLIA_NATIVE_MATH_OK)
            return std::unexpected("Could not create Mermaid math cancellation");
        cancellation_.reset(cancellation);
        renderer_ = std::move(owned);
        return {};
    }

public:
    std::expected<folia::mermaid::MathFragment, std::string> render(
        std::u16string_view tex, double font_size) {
        if (!std::isfinite(font_size) || font_size <= 0)
            return std::unexpected("Invalid Mermaid math font size");
        auto ready = initialize();
        if (!ready) return std::unexpected(ready.error());
        // The C ABI uses uint16_t units. Copy rather than alias char16_t through
        // a different integer type; TeX is consumed synchronously by the DLL.
        const std::vector<std::uint16_t> units(tex.begin(), tex.end());
        FoliaNativeMathResult* raw = nullptr;
        // Mermaid's MathJax provider uses an unconstrained display formula.
        // Explicit TeX percentages still receive the same 1e6px context width.
        const auto status = FoliaNativeMathRenderSvgFragment(renderer_.get(),
            units.data(), units.size(), font_size, 1e6,
            cancellation_.get(), 2000, &raw);
        Result result{raw, FoliaNativeMathResultDestroy};
        if (status != FOLIA_NATIVE_MATH_OK || !result) {
            std::size_t size = 0;
            const auto* error = result ? FoliaNativeMathResultError(result.get(), &size) : nullptr;
            const auto detail = error && size ? std::string{error, size} :
                "Native MathJax could not render the formula";
            return std::unexpected("Mermaid math: " + detail);
        }
        FoliaNativeMathSvgMetrics metrics{};
        if (!FoliaNativeMathResultFragmentMetrics(result.get(), &metrics) ||
            !std::isfinite(metrics.width) || metrics.width < 0 ||
            !std::isfinite(metrics.height) || metrics.height < 0 ||
            !std::isfinite(metrics.baseline))
            return std::unexpected("Native MathJax returned invalid formula dimensions");
        std::size_t size = 0;
        const auto* svg = FoliaNativeMathResultSvg(result.get(), &size);
        if (!svg || !size)
            return std::unexpected("Native MathJax returned an empty SVG fragment");
        return folia::mermaid::MathFragment{
            std::string{svg, size}, metrics.width, metrics.height, metrics.baseline};
    }
};

}
