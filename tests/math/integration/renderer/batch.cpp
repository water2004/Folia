#include "math/support/renderer_test.hpp"

namespace {
suite native_renderer_batch = [] {
    "C ABI batch isolates item errors and shares the single rendering path"_test = [] {
        auto renderer = makeRenderer();
        auto cancel = makeCancellation();
        const std::array<std::u16string_view, 3> texts{u"x^2", u"\\unicode{x110000}", u"\\mathbb{R}"};
        std::array<std::vector<std::uint16_t>, 3> units;
        std::array<FoliaNativeMathInput, 3> inputs{};
        std::array<FoliaNativeMathOutput, 3> outputs{};
        for (std::size_t i = 0; i < texts.size(); ++i) {
            for (auto value : texts[i]) units[i].push_back(static_cast<std::uint16_t>(value));
            inputs[i] = {units[i].data(), units[i].size(), 0, 16, 640, cancel.get(), 2000};
        }
        expect(FoliaNativeMathRenderSvgBatch(renderer.get(), inputs.data(), inputs.size(), outputs.data())
            == FOLIA_NATIVE_MATH_OK);
        std::array<Result, 3> results;
        for (std::size_t i = 0; i < outputs.size(); ++i) results[i].reset(outputs[i].result);
        expect(outputs[0].status == FOLIA_NATIVE_MATH_OK);
        expect(outputs[1].status == FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR);
        expect(outputs[2].status == FOLIA_NATIVE_MATH_OK);
        const auto one = render(renderer.get(), texts[2], cancel.get(), false, 16, 640, 2000);
        std::size_t size = 0;
        const auto* bytes = FoliaNativeMathResultSvg(one.result.get(), &size);
        if (bytes) checkResult(results[2].get(), std::string_view(bytes, size), "batch-of-one equivalence");
        expect(FoliaNativeMathRenderSvgBatch(renderer.get(), nullptr, 0, nullptr) == FOLIA_NATIVE_MATH_OK);
    };
    "Renderer batch shares request fonts and matches batches of one"_test = [] {
        folia::math::Renderer renderer;
        const std::array<folia::math::RenderInput, 4> input{{
            {u"x^2", {}, {}}, {u"\\frac{x}{2}", {}, {}},
            {u"\\mathbb{R}", {}, {}}, {u"\\sqrt{x+1}", {}, {}}}};
        auto batch = renderer.renderBatch(input);
        expect(batch.size() == input.size());
        for (std::size_t i=0; i<batch.size(); ++i)
            expect(batch[i].result.has_value() && !batch[i].error);
        for (std::size_t i=0; i<batch.size(); ++i)
            if (batch[i].result) expect(batch[i].result->svg == renderer.render(input[i].tex).svg);
        expect(renderer.renderBatch({}).empty());
    };
    "Renderer bounded batches preserve every canonical Source SVG"_test = [] {
        for (std::size_t batchSize : {32, 64}) {
            folia::math::Renderer renderer;
            for (std::size_t begin = 0; begin < std::size(sourceCanonicalSceneCases()); begin += batchSize) {
                const auto end = (std::min)(begin + batchSize, std::size(sourceCanonicalSceneCases()));
                std::vector<std::u16string> texts;
                std::vector<folia::math::RenderInput> inputs;
                texts.reserve(end - begin);
                inputs.reserve(end - begin);
                for (auto i = begin; i < end; ++i) {
                    auto const& source = sourceCanonicalSceneCases()[i];
                    texts.push_back(folia::math::utf16::decode(source.tex).text);
                    inputs.push_back({texts.back(), {},
                        {.display = source.requested_display, .em = source.em, .containerWidth = source.width}});
                }
                const auto output = renderer.renderBatch(inputs);
                expect(output.size() == inputs.size());
                for (std::size_t i = 0; i < output.size(); ++i) {
                    auto const& source = sourceCanonicalSceneCases()[begin + i];
                    expect(!output[i].error && output[i].result.has_value()) << source.name;
                    if (output[i].result)
                        expect(output[i].result->svg == source_svg_paint::materialize(source.modes[0].svg))
                            << source.name << ": complete SVG with explicit CSS paint";
                }
            }
        }
    };

};
} // namespace
