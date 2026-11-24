#include "math/support/renderer_test.hpp"

namespace {
suite native_renderer_lifetime = [] {
    "public C++ Renderer retains its RenderResult Scene after destruction"_test = [] {
        std::optional<folia::math::RenderResult> retained;
        std::string retainedSvg;
        std::vector<std::uint32_t> retainedCodepoints;
        auto fontData = folia::math::fonts::makeNewcmFontData();
        {
            folia::math::Renderer renderer;
            auto const baseline = renderer.render("x");
            expect(!baseline.svg.empty()) << "public Renderer produces baseline SVG";

            (void)renderer.render(R"(\newcommand{\FoliaOwnerMacro}{x})");
            auto const macroUse = renderer.render(R"(\FoliaOwnerMacro)");
            expect(codepoints(macroUse.scene) == codepoints(baseline.scene))
                << "macro definition persists across public Renderer requests";

            auto const labelA = renderer.render(
                R"(\begin{equation}\tag{A}\label{folia:owner:a}x\end{equation})");
            auto const labelB = renderer.render(
                R"(\begin{equation}\tag{B}\label{folia:owner:b}\ref{folia:owner:a}\end{equation})");
            const auto labelACodepoints = codepoints(labelA.scene);
            const auto labelBCodepoints = codepoints(labelB.scene);
            expect(containsCodepoint(labelACodepoints, static_cast<std::uint32_t>('A')))
                << "tag A is present in the rendered Scene";
            expect(containsCodepoint(labelBCodepoints, static_cast<std::uint32_t>('A')) &&
                   containsCodepoint(labelBCodepoints, static_cast<std::uint32_t>('B')))
                << "tag B and the reference to label A are present in the rendered Scene";

            auto const matrix =
                renderer.renderScene(R"(\begin{matrix}x&y\\z&w\end{matrix})");
            const auto matrixCodepoints = codepoints(matrix);
            for (const auto character : std::string_view("xyzw")) {
                const auto cellCodepoints =
                    codepoints(renderer.renderScene(std::string(1, character)));
                expect(!cellCodepoints.empty()) << "individual matrix cell has a glyph";
                for (const auto codepoint : cellCodepoints)
                    expect(containsCodepoint(matrixCodepoints, codepoint))
                        << "renderScene retains each cell's resolved mathematical glyph";
            }

            auto const malformed = renderer.render(R"(\frac{)");
            expect(malformed.svg.find("data-mjx-error=") != std::string::npos ||
                   malformed.svg.find("data-mml-node=\"merror\"") != std::string::npos)
                << "malformed TeX produces the Source merror output";
            expect(renderer.render("x").svg == baseline.svg)
                << "Renderer recovers with identical SVG after malformed TeX";

            retained.emplace(renderer.render(R"(\mathtip{x+1}{y^2})"));
            retainedSvg = retained->svg;
            retainedCodepoints = codepoints(retained->scene);
            svg::SvgWriteOptions options;
            options.font_cache = svg::FontCacheMode::none;
            options.font_resources = &fontData.svg_resources;
            expect(svg::write(retained->scene, options) == retainedSvg)
                << "complete Scene serialization matches Renderer SVG while Renderer is alive";
        }

        expect(retained.has_value()) << "RenderResult remains owned outside Renderer scope";
        if (!retained)
            return;
        expect(retained->svg == retainedSvg) << "RenderResult SVG survives Renderer destruction";
        expect(codepoints(retained->scene) == retainedCodepoints)
            << "immutable Group Scene glyphs survive Renderer destruction";
        svg::SvgWriteOptions options;
        options.font_cache = svg::FontCacheMode::none;
        options.font_resources = &fontData.svg_resources;
        expect(svg::write(retained->scene, options) == retainedSvg)
            << "complete Scene serialization remains byte-identical after Renderer destruction";
    };

    "public C++ Renderer cancellation and RangeError leave the session reusable"_test = [] {
        folia::math::Renderer renderer;
        const auto baseline = renderer.render("x").svg;
        std::stop_source stopped;
        stopped.request_stop();
        bool observedCancellation = false;
        try {
            (void)renderer.render(std::string_view("x"), stopped.get_token(), {});
        } catch (folia::math::RenderError const &error) {
            observedCancellation = error.name == "Cancelled";
        }
        expect(observedCancellation) << "pre-cancelled public Renderer request reports Cancelled";
        expect(renderer.render("x").svg == baseline)
            << "pre-cancellation leaves the public Renderer session usable";

        bool observedRangeError = false;
        try {
            (void)renderer.render(R"(\mathtip{x}{\unicode{x110000}})");
        } catch (std::range_error const &error) {
            observedRangeError =
                std::string_view(error.what()) == "Invalid code point 1114112";
        }
        expect(observedRangeError)
            << "out-of-range Unicode propagates the Source RangeError as std::range_error";
        expect(renderer.render("x").svg == baseline)
            << "Renderer recovers with identical SVG after Unicode RangeError";
    };
};
} // namespace
