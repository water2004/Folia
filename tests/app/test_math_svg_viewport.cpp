#include "pch.h"
#include "support/folia_test.hpp"
#include "media/MathJaxSvgParser.h"
#include "media/NativeMathSession.h"
#include "media/SvgNormalizer.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    constexpr std::string_view taggedEquation = R"(|\widetilde0\rangle\approx |0\rangle
-\frac{\gamma_e(B_x-iB_y)}{\sqrt2(D_g+\gamma_eB_z)}|+1\rangle
-\frac{\gamma_e(B_x+iB_y)}{\sqrt2(D_g-\gamma_eB_z)}|-1\rangle. \tag{4.9})";

    constexpr std::string_view alignedBody = R"(\begin{aligned}
\hat H_g/h={}&D'\left(\hat S_z^2-\frac23I_3\right)
+\gamma_e(B_x\hat S_x+B_y\hat S_y+B_z\hat S_z)\\
&+q_x(\hat S_y^2-\hat S_x^2)+q_y\{\hat S_x,\hat S_y\}
+u_x\{\hat S_x,\hat S_z\}+u_y\{\hat S_y,\hat S_z\}. )";

    suite math_svg_viewport_tests = []
    {
        "math SVG percentage viewport honors container and intrinsic minimum"_test = []
        {
            constexpr std::string_view source = R"(<mjx-container width="full"><svg xmlns="http://www.w3.org/2000/svg" style="vertical-align: -1ex; min-width: 40ex;" width="100%" height="4ex" data-mjx-viewBox="0 -1000 4000 2000"><svg width="100%" height="100%" viewBox="0 0 100 10"><path d="M0 0L100 10"/></svg></svg></mjx-container>)";
            SvgNormalizer normalizer;
            for (float em : {16.0f, 24.0f})
            {
                for (float available : {120.0f, 1280.0f})
                {
                    auto parsed = ParseMathJaxSvgOutput(source, true, em, available);
                    expect(static_cast<bool>(parsed));
                    expect(parsed.fragments.size() == 1_u);
                    if (parsed.fragments.size() != 1) continue;
                    auto const& fragment = parsed.fragments.front();
                    auto expectedWidth = (std::max)(available, 20.0f * em);
                    expect(fragment.width == expectedWidth);
                    expect(fragment.height == 2.0f * em);
                    expect(fragment.verticalAlign == -0.5f * em);
                    // Only the outer viewport is resolved; the nested layout
                    // and metadata remain exactly as MathJax emitted them.
                    expect(fragment.svg->find(R"(<svg width="100%" height="100%" viewBox="0 0 100 10">)") != std::string::npos);
                    expect(fragment.svg->find(R"(data-mjx-viewBox="0 -1000 4000 2000")") != std::string::npos);
                    auto normalized = normalizer.NormalizeNow(*fragment.svg, em);
                    expect(static_cast<bool>(normalized)) << normalized.error;
                    expect(std::abs(normalized.width - expectedWidth) < 0.01f);
                    expect(std::abs(normalized.height - fragment.height) < 0.01f);
                }
            }
        };

        "math SVG fixed viewport and inline fragments retain original geometry"_test = []
        {
            constexpr std::string_view first = R"(<svg width="3ex" height="2ex" style="vertical-align: -0.5ex;"><path d="M0 0"/></svg>)";
            constexpr std::string_view second = R"(<svg width="2em" height="1em"><svg width="100%" height="100%" style="min-width: 900ex;"></svg></svg>)";
            auto output = std::string(first) + R"(<mjx-break size="3"></mjx-break>)" + std::string(second);
            auto parsed = ParseMathJaxSvgOutput(output, false, 16, 1280);
            expect(parsed.fragments.size() == 2_u);
            if (parsed.fragments.size() != 2) return;
            expect(*parsed.fragments[0].svg == first);
            expect(*parsed.fragments[1].svg == second);
            expect(parsed.fragments[0].width == 24.0f);
            expect(parsed.fragments[1].width == 32.0f);
            expect(parsed.fragments[1].breakBefore);
            expect(std::abs(parsed.fragments[1].breakSpace - 16.0f * 4 / 18) < 0.001f);
        };

        "math SVG tagged equations normalize consistently in single and batch requests"_test = []
        {
            NativeMathSession session;
            SvgNormalizer normalizer;
            auto aligned = std::string(alignedBody) + R"(\end{aligned}\tag{4.1})";
            std::vector<NativeMathInput> inputs;
            for (auto tex : {taggedEquation, std::string_view(aligned)})
                for (float em : {16.0f, 24.0f})
                    for (float available : {240.0f, 1280.0f})
                        inputs.push_back({tex, true, em, available});
            auto results = session.RenderBatch(inputs);
            expect(results.size() == inputs.size());
            for (std::size_t i = 0; i < results.size(); ++i)
            {
                auto const& input = inputs[i];
                auto const& rendered = results[i];
                expect(rendered.error.empty()) << rendered.error;
                expect(rendered.errorKind == MathJaxErrorKind::None);
                expect(rendered.fragments.size() == 1_u);
                if (rendered.fragments.size() != 1) continue;
                expect(rendered.width >= input.containerWidth);
                // Both full formulas exceed a 240px container. Their minimum
                // width must survive, instead of squeezing the label inward.
                if (input.containerWidth == 240) expect(rendered.width > 240.0f);
                else expect(rendered.width == 1280.0f);
                auto single = session.Render(input.tex, true, input.em, input.containerWidth);
                expect(single.width == rendered.width && single.height == rendered.height);
                expect(single.fragments.size() == 1_u);
                if (single.fragments.size() == 1)
                    expect(*single.fragments.front().svg == *rendered.fragments.front().svg);
                auto normalized = normalizer.NormalizeNow(*rendered.fragments.front().svg, input.em);
                expect(static_cast<bool>(normalized)) << normalized.error;
                expect(std::abs(normalized.width - rendered.width) < 0.01f);
                expect(std::abs(normalized.height - rendered.height) < 0.01f);
            }
        };

        "math SVG aligned rejects inner tag as MathJax does"_test = []
        {
            NativeMathSession session;
            auto tex = std::string(alignedBody) + R"(\tag{4.1}\end{aligned})";
            auto rendered = session.Render(tex, true, 16, 1280);
            expect(rendered.errorKind == MathJaxErrorKind::Formula);
            expect(rendered.error == R"(\tag not allowed in aligned environment)");
        };
    };
}
