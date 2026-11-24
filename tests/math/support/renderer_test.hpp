#pragma once

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
#include "math/support/source_svg_paint.hpp"
#include "support/folia_test.hpp"

import folia.math.util.utf16;
import folia.math;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.svg.writer;

using namespace boost::ut;

#include "math/support/scene_fixtures.hpp"
using math_test::fixtures::CanonicalSceneCase;
using math_test::fixtures::CanonicalSceneMode;
using math_test::fixtures::CanonicalSceneMetrics;
using math_test::fixtures::sourceCanonicalSceneCases;

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

} // namespace
