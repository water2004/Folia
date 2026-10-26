#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>
#include "folia/math/native_renderer_bridge.h"
#include "support/folia_test.hpp"

import folia.math.util.utf16;
import folia.math;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.svg.writer;

using namespace boost::ut;

#include "fixtures/canonical_scene_modes_source.hpp"

namespace {
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

void collectCodepoints(scene::Group const &group, std::vector<std::uint32_t> &result) {
    for (auto const &entry : group.entries) {
        if (auto const *primitive = std::get_if<scene::Primitive>(&entry)) {
            if (auto const *run = std::get_if<scene::GlyphRun>(primitive)) {
                for (auto const &glyph : run->glyphs)
                    result.push_back(glyph.key.codepoint);
            } else if (auto const *text = std::get_if<scene::TextRun>(primitive)) {
                for (auto const &glyph : text->measured_glyphs.glyphs)
                    result.push_back(glyph.key.codepoint);
            }
        } else if (auto const *child =
                       std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child)
                collectCodepoints(**child, result);
        }
    }
}

std::vector<std::uint32_t> codepoints(scene::Scene const &value) {
    std::vector<std::uint32_t> result;
    collectCodepoints(value.root(), result);
    return result;
}

bool containsCodepoint(std::vector<std::uint32_t> const &values, std::uint32_t value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

struct RendererDelete {
    void operator()(FoliaNativeMathRenderer *p) const noexcept {
        FoliaNativeMathRendererDestroy(p);
    }
};
struct CancellationDelete {
    void operator()(FoliaNativeMathCancellation *p) const noexcept {
        FoliaNativeMathCancellationDestroy(p);
    }
};
struct ResultDelete {
    void operator()(FoliaNativeMathResult *p) const noexcept {
        FoliaNativeMathResultDestroy(p);
    }
};
using Renderer = std::unique_ptr<FoliaNativeMathRenderer, RendererDelete>;
using Cancellation = std::unique_ptr<FoliaNativeMathCancellation, CancellationDelete>;
using Result = std::unique_ptr<FoliaNativeMathResult, ResultDelete>;

Renderer makeRenderer() {
    FoliaNativeMathRenderer *raw{};
    const auto status = FoliaNativeMathRendererCreate(&raw);
    expect(status == FOLIA_NATIVE_MATH_OK && raw != nullptr) << "renderer create";
    return Renderer(raw);
}
Cancellation makeCancellation() {
    FoliaNativeMathCancellation *raw{};
    const auto status = FoliaNativeMathCancellationCreate(&raw);
    expect(status == FOLIA_NATIVE_MATH_OK && raw != nullptr) << "cancellation create";
    return Cancellation(raw);
}
struct RenderCall {
    FoliaNativeMathStatus status{FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR};
    Result result{};
};
RenderCall render(FoliaNativeMathRenderer *renderer, std::u16string_view tex,
                  FoliaNativeMathCancellation *cancel, bool display = false, double em = 16,
                  double width = 1280, unsigned timeout = 0) {
    std::vector<std::uint16_t> units;
    units.reserve(tex.size());
    for (char16_t value : tex)
        units.push_back(static_cast<std::uint16_t>(value));
    FoliaNativeMathResult *raw{};
    const auto status = FoliaNativeMathRenderSvg(renderer, units.data(), units.size(),
                                                 display ? 1 : 0, em, width, cancel, timeout, &raw);
    return {status, Result(raw)};
}
bool checkResult(FoliaNativeMathResult const *result, std::string_view expectedSvg,
                 std::string_view label) {
    if (!result) {
        expect(false) << label << ": result is non-null";
        return false;
    }
    std::size_t svgSize = 0, errorSize = 0;
    const char *svg = FoliaNativeMathResultSvg(result, &svgSize);
    const char *error = FoliaNativeMathResultError(result, &errorSize);
    const bool valid = svg && error;
    expect(valid) << label << ": both result accessors return storage";
    if (!valid)
        return false;
    expect(svgSize == expectedSvg.size()) << label << ": SVG byte size";
    expect(std::string_view(svg, svgSize) == expectedSvg) << label << ": complete SVG bytes";
    expect(errorSize == 0) << label << ": successful result has empty error";
    return true;
}

suite native_renderer_bridge = [] {
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
            for (std::size_t begin = 0; begin < std::size(sourceCanonicalSceneCases); begin += batchSize) {
                const auto end = (std::min)(begin + batchSize, std::size(sourceCanonicalSceneCases));
                std::vector<std::u16string> texts;
                std::vector<folia::math::RenderInput> inputs;
                texts.reserve(end - begin);
                inputs.reserve(end - begin);
                for (auto i = begin; i < end; ++i) {
                    auto const& source = sourceCanonicalSceneCases[i];
                    texts.push_back(folia::math::utf16::decode(source.tex).text);
                    inputs.push_back({texts.back(), {},
                        {.display = source.requested_display, .em = source.em, .containerWidth = source.width}});
                }
                const auto output = renderer.renderBatch(inputs);
                expect(output.size() == inputs.size());
                for (std::size_t i = 0; i < output.size(); ++i) {
                    auto const& source = sourceCanonicalSceneCases[begin + i];
                    expect(!output[i].error && output[i].result.has_value()) << source.name;
                    if (output[i].result) expect(output[i].result->svg == source.modes[0].svg) << source.name;
                }
            }
        }
    };
    "opaque C ABI renders the canonical Source profile and retains result storage"_test = [] {
        static_assert(sizeof(char16_t) == sizeof(std::uint16_t));
        expect(std::size(sourceCanonicalSceneCases) == 75u) << "canonical source has 75 inputs";
        auto renderer = makeRenderer();
        if (!renderer)
            return;
        Result retained;
        char const *retainedSvg{};
        std::size_t retainedSize{};
        std::string retainedCopy;
        std::size_t unicodeInputs = 0;
        for (std::size_t i = 0; i < std::size(sourceCanonicalSceneCases); ++i) {
            auto const &source = sourceCanonicalSceneCases[i];
            auto const &mode = source.modes[0];
            const std::string label = std::string(source.name) + " [opaque C ABI]";
            const auto input = folia::math::utf16::decode(source.tex).text;
            if (std::ranges::any_of(input, [](char16_t c) { return c > 0x7f; }))
                ++unicodeInputs;
            auto cancel = makeCancellation();
            if (!cancel)
                continue;
            auto call = render(renderer.get(), input, cancel.get(), source.requested_display,
                               source.em, source.width);
            expect(call.status == FOLIA_NATIVE_MATH_OK) << label << ": status is OK";
            if (call.status != FOLIA_NATIVE_MATH_OK || !call.result) {
                expect(call.result != nullptr) << label << ": successful call returns a result";
                continue;
            }
            if (mode.error_name.empty())
                checkResult(call.result.get(), mode.svg, label);
            else
                expect(false) << label << ": Source mode is a successful canonical case";
            if (i == 0) {
                std::size_t size{};
                retainedSvg = FoliaNativeMathResultSvg(call.result.get(), &size);
                retainedSize = size;
                expect(retainedSvg != nullptr) << label << ": retained SVG accessor";
                if (retainedSvg)
                    retainedCopy.assign(retainedSvg, size);
                retained = std::move(call.result);
            }
        }
        expect(unicodeInputs > 0u) << "canonical profile exercises non-ASCII UTF-16 units";
        renderer.reset();
        expect(retainedSvg != nullptr) << "first result storage was retained";
        if (retainedSvg)
            expect(std::string_view(retainedSvg, retainedSize) == retainedCopy)
                << "result bytes survive subsequent renders and renderer destruction";
        if (retained)
            checkResult(retained.get(), retainedCopy, "retained result after renderer destruction");
    };

    "C ABI rejects invalid arguments and null accessors safely"_test = [] {
        FoliaNativeMathRendererDestroy(nullptr);
        FoliaNativeMathCancellationDestroy(nullptr);
        FoliaNativeMathCancellationRequestStop(nullptr);
        FoliaNativeMathResultDestroy(nullptr);
        std::size_t size = 91;
        expect(FoliaNativeMathResultSvg(nullptr, &size) == nullptr && size == 0u)
            << "null SVG accessor";
        size = 91;
        expect(FoliaNativeMathResultError(nullptr, &size) == nullptr && size == 0u)
            << "null error accessor";
        expect(FoliaNativeMathResultSvg(nullptr, nullptr) == nullptr)
            << "null size pointer accepted";

        expect(FoliaNativeMathRendererCreate(nullptr) == FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR)
            << "null renderer out pointer";
        expect(FoliaNativeMathCancellationCreate(nullptr) == FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR)
            << "null cancellation out pointer";
        auto renderer = makeRenderer();
        auto cancel = makeCancellation();
        if (!renderer || !cancel)
            return;
        auto expectInvalid = [&](FoliaNativeMathRenderer *rp, std::uint16_t const *text,
                                 std::size_t units, int display, double em, double width,
                                 FoliaNativeMathCancellation *cp, std::string_view label) {
            auto *out = reinterpret_cast<FoliaNativeMathResult *>(std::uintptr_t{1});
            const auto status =
                FoliaNativeMathRenderSvg(rp, text, units, display, em, width, cp, 0, &out);
            expect(status == FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR) << label << ": status";
            expect(out == nullptr) << label << ": output reset to null";
        };
        const std::uint16_t x[]{'x'};
        expect(FoliaNativeMathRenderSvg(renderer.get(), x, 1, 0, 16, 1280, cancel.get(), 0,
                                        nullptr) == FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR)
            << "null result out pointer";
        expectInvalid(nullptr, x, 1, 0, 16, 1280, cancel.get(), "null renderer");
        expectInvalid(renderer.get(), x, 1, 0, 16, 1280, nullptr, "null cancellation");
        expectInvalid(renderer.get(), nullptr, 1, 0, 16, 1280, cancel.get(), "null TeX with units");
        expectInvalid(renderer.get(), x, 1, -1, 16, 1280, cancel.get(), "negative display");
        expectInvalid(renderer.get(), x, 1, 2, 16, 1280, cancel.get(), "display above one");
        expectInvalid(renderer.get(), x, 1, 0, INFINITY, 1280, cancel.get(), "infinite em");
        expectInvalid(renderer.get(), x, 1, 0, 0, 1280, cancel.get(), "zero em");
        expectInvalid(renderer.get(), x, 1, 0, 16, NAN, cancel.get(), "NaN width");
        expectInvalid(renderer.get(), x, 1, 0, 16, 0, cancel.get(), "zero width");
    };

    "C ABI enforces owner thread and supports reusable pre-cancelled sessions"_test = [] {
        auto renderer = makeRenderer();
        auto cancel = makeCancellation();
        if (!renderer || !cancel)
            return;
        const std::u16string x = u"x";
        const std::uint16_t xUnit[]{'x'};
        FoliaNativeMathStatus wrongThreadStatus = FOLIA_NATIVE_MATH_OK;
        bool wrongThreadOutputNull = false;
        std::thread worker([&] {
            FoliaNativeMathResult *out =
                reinterpret_cast<FoliaNativeMathResult *>(std::uintptr_t{1});
            wrongThreadStatus = FoliaNativeMathRenderSvg(renderer.get(), xUnit, std::size(xUnit), 0,
                                                         16, 1280, cancel.get(), 0, &out);
            wrongThreadOutputNull = out == nullptr;
        });
        worker.join();
        expect(wrongThreadStatus == FOLIA_NATIVE_MATH_INFRASTRUCTURE_ERROR)
            << "wrong-thread call rejected";
        expect(wrongThreadOutputNull) << "wrong-thread output stays null";
        auto ownerCall = render(renderer.get(), x, cancel.get());
        expect(ownerCall.status == FOLIA_NATIVE_MATH_OK && ownerCall.result != nullptr)
            << "owner-thread call still succeeds";

        auto stopped = makeCancellation();
        if (!stopped)
            return;
        FoliaNativeMathCancellationRequestStop(stopped.get());
        auto cancelled = render(renderer.get(), x, stopped.get());
        expect(cancelled.status == FOLIA_NATIVE_MATH_CANCELLED)
            << "pre-cancelled request reports Cancelled";
        expect(cancelled.result != nullptr) << "cancelled request returns owned error result";
        if (cancelled.result) {
            std::size_t size{};
            auto error = FoliaNativeMathResultError(cancelled.result.get(), &size);
            expect(error &&
                   std::string_view(error, size).find("cancelled") != std::string_view::npos)
                << "cancel result owns its explanatory message";
        }
        auto fresh = makeCancellation();
        if (!fresh)
            return;
        auto recovered = render(renderer.get(), x, fresh.get());
        expect(recovered.status == FOLIA_NATIVE_MATH_OK && recovered.result != nullptr)
            << "pre-cancellation leaves renderer session usable";
    };

    "C ABI in-flight deadline cancels and makes the session unusable"_test = [] {
        auto renderer = makeRenderer();
        auto cancel = makeCancellation();
        auto fresh = makeCancellation();
        if (!renderer || !cancel || !fresh)
            return;
        const std::u16string input(4000, u'x');
        auto timed = render(renderer.get(), input, cancel.get(), false, 16, 1280, 3);
        expect(timed.status == FOLIA_NATIVE_MATH_CANCELLED)
            << "bounded in-flight TeX render reaches cancellation deadline";
        expect(timed.result != nullptr) << "deadline returns owned error result";
        if (timed.result) {
            std::size_t size{};
            auto error = FoliaNativeMathResultError(timed.result.get(), &size);
            expect(error &&
                   std::string_view(error, size).find("cancelled") != std::string_view::npos)
                << "deadline result owns its explanatory message";
        }
        auto after = render(renderer.get(), u"x", fresh.get());
        expect(after.status == FOLIA_NATIVE_MATH_SESSION_UNUSABLE)
            << "in-flight cancellation invalidates later requests";
        expect(after.result != nullptr) << "unusable session returns owned error result";
    };

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
