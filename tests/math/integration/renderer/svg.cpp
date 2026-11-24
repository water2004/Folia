#include "math/support/renderer_test.hpp"

namespace {
suite native_renderer_svg = [] {
    "C ABI array rules and frames paint in inline display and standalone SVG"_test = [] {
        auto renderer = makeRenderer();
        auto cancellation = makeCancellation();
        if (!renderer || !cancellation)
            return;
        const auto tags = [](std::string_view svg, std::string_view element) {
            const std::string opening = "<" + std::string(element) + " ";
            std::vector<std::string_view> result;
            std::size_t offset = 0;
            while ((offset = svg.find(opening, offset)) != svg.npos) {
                const auto end = svg.find('>', offset);
                if (end == svg.npos)
                    throw std::logic_error("native SVG contains an unterminated element");
                result.push_back(svg.substr(offset, end - offset + 1));
                offset = end + 1;
            }
            return result;
        };
        enum class Mode { inline_math, display_math, fragment };
        const auto capture = [&](std::u16string_view tex, Mode mode) {
            RenderCall call;
            if (mode != Mode::fragment) {
                call = render(renderer.get(), tex, cancellation.get(), mode == Mode::display_math);
            } else {
                const std::vector<std::uint16_t> units(tex.begin(), tex.end());
                FoliaNativeMathResult *raw{};
                call.status = FoliaNativeMathRenderSvgFragment(
                    renderer.get(), units.data(), units.size(), 16, 1280,
                    cancellation.get(), 0, &raw);
                call.result.reset(raw);
            }
            expect(call.status == FOLIA_NATIVE_MATH_OK && call.result != nullptr);
            if (call.status != FOLIA_NATIVE_MATH_OK || !call.result)
                return std::string{};
            std::size_t size{};
            const auto data = FoliaNativeMathResultSvg(call.result.get(), &size);
            expect(data != nullptr && size > 0);
            return data ? std::string(data, size) : std::string{};
        };
        const auto checkLinePaint = [&](std::string_view svg, std::size_t horizontal) {
            std::size_t verticalCount = 0, horizontalCount = 0;
            for (const auto tag : tags(svg, "line")) {
                if (!tag.contains(" data-line=\""))
                    continue;
                verticalCount += tag.contains(" data-line=\"v\"");
                horizontalCount += tag.contains(" data-line=\"h\"");
                expect(tag.contains(" fill=\"none\"")) << tag;
                expect(tag.contains(" stroke-width=\"70\"")) << tag;
                expect(!tag.contains(" stroke-thickness=")) << tag;
            }
            expect(verticalCount == 1_u && horizontalCount == horizontal)
                << "line nodes themselves retain visible resolved paint";
        };
        constexpr std::u16string_view array =
            uR"(\begin{array}{cc|c}a & b & c \\ \hline d & e & f\end{array})";
        constexpr std::u16string_view frame =
            uR"(\begin{array}{|c|c|}\hline a&b\\\hline c&d\\\hline\end{array})";
        constexpr std::u16string_view undefined =
            uR"(\begin{array}{cc|c}a & b & c \\ \hlined & e & f\end{array})";
        for (const auto mode : {Mode::inline_math, Mode::display_math, Mode::fragment}) {
            checkLinePaint(capture(array, mode), 1);
            const auto framed = capture(frame, mode);
            checkLinePaint(framed, 1);
            std::size_t frames = 0;
            for (const auto tag : tags(framed, "rect")) {
                if (!tag.contains(" data-frame=\"true\""))
                    continue;
                ++frames;
                expect(tag.contains(" fill=\"none\"")) << tag;
                expect(tag.contains(" stroke-width=\"70\"")) << tag;
            }
            expect(frames == 1_u);

            // Pinned MathJax's noundefined package renders the complete unknown
            // control sequence in red. It does not split \hlined into \hline d.
            const auto typo = capture(undefined, mode);
            checkLinePaint(typo, 0);
            bool redUnknownCommand = false;
            for (const auto tag : tags(typo, "g"))
                redUnknownCommand |= tag.contains(" data-mml-node=\"mtext\"") &&
                    tag.contains(" data-latex=\"\\hlined\"") &&
                    tag.contains(" fill=\"red\"") && tag.contains(" stroke=\"red\"");
            expect(redUnknownCommand);
        }
    };

    "C ABI SVG fragments retain typed pixel geometry and preserve document rendering"_test = [] {
        auto renderer = makeRenderer();
        auto cancel = makeCancellation();
        const auto before = render(renderer.get(), u"x^2", cancel.get());
        std::size_t beforeSize{};
        const auto beforeData = FoliaNativeMathResultSvg(before.result.get(), &beforeSize);
        const std::string documentSvg(beforeData, beforeSize);
        std::array<std::uint16_t, 3> units{'x', '^', '2'};
        FoliaNativeMathResult *raw{};
        expect(FoliaNativeMathRenderSvgFragment(renderer.get(), units.data(), units.size(), 16,
                                                1280, cancel.get(), 0, &raw) ==
               FOLIA_NATIVE_MATH_OK);
        Result fragment(raw);
        FoliaNativeMathSvgMetrics metrics{};
        expect(FoliaNativeMathResultFragmentMetrics(fragment.get(), &metrics) == 1);
        expect(metrics.width > 0 && metrics.height > 0 && metrics.baseline > 0);
        std::size_t size{};
        const auto bytes = FoliaNativeMathResultSvg(fragment.get(), &size);
        const std::string text(bytes, size);
        expect(text.starts_with("<svg ") && text.ends_with("</svg>"));
        expect(text.find("mjx-container") == text.npos);

        folia::math::Renderer direct;
        folia::math::RenderOptions options;
        options.display = true;
        options.svgMode = folia::math::SvgMode::standalone_display;
        const auto expected = direct.render("x^2", options);
        const auto expectedMetrics = svg::pixel_metrics(expected.scene);
        expect(text == expected.svg);
        expect(metrics.width == expectedMetrics.width && metrics.height == expectedMetrics.height &&
               metrics.baseline == expectedMetrics.baseline);
        const auto after = render(renderer.get(), u"x^2", cancel.get());
        checkResult(after.result.get(), documentSvg, "document bytes after fragment rendering");
        expect(FoliaNativeMathResultFragmentMetrics(after.result.get(), &metrics) == 0);
        expect(metrics.width == 0 && metrics.height == 0 && metrics.baseline == 0);
        renderer.reset();
        expect(FoliaNativeMathResultFragmentMetrics(fragment.get(), &metrics) == 1);
        expect(metrics.width == expectedMetrics.width);
        expect(std::string_view(bytes, size) == text);
    };

    "C ABI SVG fragments scale with font size and do not wrap to the diagram width"_test = [] {
        auto renderer = makeRenderer();
        auto cancel = makeCancellation();
        constexpr std::u16string_view tex = u"a+b+c+d+e+f+g+h+i+j";
        std::vector<std::uint16_t> units(tex.begin(), tex.end());
        std::array<FoliaNativeMathSvgMetrics, 3> metrics{};
        std::array<Result, 3> results;
        for (std::size_t i = 0; i < metrics.size(); ++i) {
            FoliaNativeMathResult *raw{};
            expect(FoliaNativeMathRenderSvgFragment(renderer.get(), units.data(), units.size(),
                                                    i == 2 ? 32 : 16, i == 1 ? 24 : 1280,
                                                    cancel.get(), 0, &raw) == FOLIA_NATIVE_MATH_OK);
            results[i].reset(raw);
            expect(FoliaNativeMathResultFragmentMetrics(raw, &metrics[i]) == 1);
        }
        expect(metrics[0].width > 24);
        expect(metrics[1].width == metrics[0].width && metrics[1].height == metrics[0].height);
        expect(metrics[2].width == metrics[0].width * 2 &&
               metrics[2].height == metrics[0].height * 2 &&
               metrics[2].baseline == metrics[0].baseline * 2);
    };

    "C ABI SVG fragment errors expose no successful geometry"_test = [] {
        auto renderer = makeRenderer();
        auto cancel = makeCancellation();
        const std::uint16_t tex = 'x';
        FoliaNativeMathResult *raw{};
        expect(FoliaNativeMathRenderSvgFragment(renderer.get(), &tex, 1, 0, 1280, cancel.get(),
                                                0, &raw) == FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR);
        expect(raw == nullptr);
        FoliaNativeMathSvgMetrics metrics{1, 2, 3};
        expect(FoliaNativeMathResultFragmentMetrics(nullptr, &metrics) == 0);
        expect(metrics.width == 0 && metrics.height == 0 && metrics.baseline == 0);
        expect(FoliaNativeMathResultFragmentMetrics(nullptr, nullptr) == 0);
        FoliaNativeMathCancellationRequestStop(cancel.get());
        expect(FoliaNativeMathRenderSvgFragment(renderer.get(), &tex, 1, 16, 1280, cancel.get(),
                                                0, &raw) == FOLIA_NATIVE_MATH_CANCELLED);
        Result cancelled(raw);
        expect(FoliaNativeMathResultFragmentMetrics(cancelled.get(), &metrics) == 0);
    };

    "standalone math retains visible formulas and textual SVG tooltips"_test = [] {
        folia::math::Renderer renderer;
        folia::math::RenderOptions options;
        options.display = true;
        options.svgMode = folia::math::SvgMode::standalone_display;
        const auto plain = renderer.render("x", options);
        const auto expected = svg::pixel_metrics(plain.scene);
        for (const auto source : {R"(\mathtip{x}{\frac{y}{z}})", R"(\texttip{x}{note})"}) {
            const auto result = renderer.render(source, options);
            const auto metrics = svg::pixel_metrics(result.scene);
            expect(metrics.width == expected.width && metrics.height == expected.height);
            expect(result.svg.find("foreignObject") == result.svg.npos);
            expect(result.svg.find("data-c=\"1D465\"") != result.svg.npos);
            if (std::string_view(source).starts_with(R"(\texttip)"))
                expect(result.svg.find("<title>note</title>") != result.svg.npos);
        }
        expect(throws<svg::UnsupportedError>([&] {
            (void)renderer.render(
                R"(\setOptions{allowTexHTML=true}<tex-html><b>visible</b></tex-html>\setOptions{allowTexHTML=false})",
                options);
        })) << "visible HTML requires an HTML host and is not silently discarded";
    };


};
} // namespace
