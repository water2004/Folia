#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "math/support/heap_snapshot.hpp"
#include "support/folia_test.hpp"
#include "math/support/tex_scene.hpp"

import folia.math.core.dom_node;
import folia.math.core.mml_tree.mml_document;
import folia.math.output.common;
import folia.math.output.scene;
import folia.math.output.svg.writer;
import folia.math.util.instance_count;

using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;
namespace tex_scene = tex_scene_test;
namespace counts = folia::math::util;

namespace {
using Clock = std::chrono::steady_clock;
constexpr std::array<tex_scene::TexSceneInput, 12> fixedInputs{{
    {"x", false, 16, 8, 1280, {}},
    {R"(\frac{a}{b})", false, 16, 8, 1280, {}},
    {R"(\sqrt{x})", false, 16, 8, 1280, {}},
    {R"(\mathbb{R})", false, 16, 8, 1280, {}},
    {R"(\begin{matrix}a&b\\c&d\end{matrix})", true, 16, 8, 1280, {}},
    {"x_i^2", false, 16, 8, 1280, {}},
    {R"(\text{中文})", false, 16, 8, 1280, {}},
    {"x+y", false, 16, 8, 1280, {}},
    {R"(\color{red}{x})", false, 16, 8, 1280, {}},
    {R"(\bbox[2px]{x})", false, 16, 8, 1280, {}},
    {R"(\sum_{i=1}^n i)", false, 16, 8, 1280, {}},
    {R"(\left(x\right))", false, 16, 8, 1280, {}},
}};

struct LiveCounts {
    std::size_t nodes{}, documents{}, wrappers{}, dom_nodes{}, dom_documents{}, dom_windows{};
};

LiveCounts liveCounts() noexcept {
    return {counts::InstanceCount<mml::MmlNode>::live(),
            counts::InstanceCount<mml::MmlDocument>::live(),
            counts::InstanceCount<common::CommonWrapper>::live(),
            counts::InstanceCount<folia::math::DOMNode>::live(),
            counts::InstanceCount<folia::math::DOMDocument>::live(),
            counts::InstanceCount<folia::math::DOMWindow>::live()};
}

struct Totals {
    std::size_t node_constructed{}, node_destroyed{}, document_constructed{}, document_destroyed{};
    std::size_t wrapper_constructed{}, wrapper_destroyed{}, dom_constructed{}, dom_destroyed{};
};

Totals totals() noexcept {
    return {counts::InstanceCount<mml::MmlNode>::constructed(),
            counts::InstanceCount<mml::MmlNode>::destroyed(),
            counts::InstanceCount<mml::MmlDocument>::constructed(),
            counts::InstanceCount<mml::MmlDocument>::destroyed(),
            counts::InstanceCount<common::CommonWrapper>::constructed(),
            counts::InstanceCount<common::CommonWrapper>::destroyed(),
            counts::InstanceCount<folia::math::DOMNode>::constructed(),
            counts::InstanceCount<folia::math::DOMNode>::destroyed()};
}

bool same(LiveCounts const &a, LiveCounts const &b) noexcept {
    return a.nodes == b.nodes && a.documents == b.documents && a.wrappers == b.wrappers &&
           a.dom_nodes == b.dom_nodes && a.dom_documents == b.dom_documents &&
           a.dom_windows == b.dom_windows;
}

bool requestBalanced(Totals const &before, Totals const &after) noexcept {
    return after.node_constructed > before.node_constructed &&
           after.node_constructed - before.node_constructed ==
               after.node_destroyed - before.node_destroyed &&
           after.document_constructed > before.document_constructed &&
           after.document_constructed - before.document_constructed ==
               after.document_destroyed - before.document_destroyed &&
           after.wrapper_constructed > before.wrapper_constructed &&
           after.wrapper_constructed - before.wrapper_constructed ==
               after.wrapper_destroyed - before.wrapper_destroyed &&
           after.dom_constructed > before.dom_constructed &&
           after.dom_constructed - before.dom_constructed ==
               after.dom_destroyed - before.dom_destroyed;
}

struct ThrowOnUnknownText final : common::MeasurementTarget {
    std::size_t calls{};
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("unexpected extended metric request in lifetime probe");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        ++calls;
        throw std::runtime_error("request-lifetime injected unknown-text measurement failure");
    }
};

void serialize(scene::Scene const &value, svg::SvgFontResources const &resources) {
    svg::SvgWriteOptions options;
    options.font_cache = svg::FontCacheMode::none;
    options.font_resources = &resources;
    (void)svg::write(value, options);
}
} // namespace

suite request_lifetime = [] {
    "native_request_live_objects_and_debug_heap_return_to_warmed_baseline"_test = [] {
        std::optional<scene::Scene> retained;
        svg::SvgFontResources retainedResources;
        std::string retainedSvg;
        const auto globalLiveBeforeOwner = liveCounts();
        bool measurementExceptionBalanced = false;
        bool malformedMerrorBalanced = false;
        bool writerExceptionBalanced = false;
        bool malformedOutputMarkedAsError = false;
        bool requestFramesInactive = true;
        std::size_t measurementFailureCalls = 0;
        std::size_t failedRequestIndex = 0;
        std::array<math_heap_test::Snapshot, 24> firstHeapSamples{};
        std::uint64_t requestNanoseconds = 0;
        std::uint64_t cleanupNanoseconds = 0;
        {
            tex_scene::InputOwner owner;
            tex_scene::LiteUnknownTextMeasurement normalMeasurement(owner.adaptor);
            const auto hasNoActiveRequest = [&]() noexcept {
                return owner.canonicalInput && !owner.canonicalInput->parseOptions().hasRequest();
            };

            // Warm all three exceptional paths before taking the fixed-state baseline.
            (void)tex_scene::buildScene(fixedInputs[6], owner, normalMeasurement, true);
            {
                ThrowOnUnknownText warmFailure;
                try {
                    (void)tex_scene::buildScene(fixedInputs[6], owner, warmFailure, true);
                } catch (std::runtime_error const &) {
                }
                requestFramesInactive &= hasNoActiveRequest();
            }
            (void)tex_scene::buildScene(fixedInputs[6], owner, normalMeasurement, true);
            (void)tex_scene::buildScene({R"(\frac{a}{)", false, 16, 8, 1280, {}}, owner,
                                        normalMeasurement, true);
            requestFramesInactive &= hasNoActiveRequest();
            {
                auto writerWarm =
                    tex_scene::buildScene(fixedInputs[0], owner, normalMeasurement, true);
                try {
                    svg::SvgWriteOptions missingResources;
                    missingResources.font_cache = svg::FontCacheMode::none;
                    (void)svg::write(writerWarm, missingResources);
                } catch (svg::UnsupportedError const &) {
                }
                serialize(writerWarm, owner.fonts.svg_resources);
            }
            requestFramesInactive &= hasNoActiveRequest();

            // Warm every fixed, stateless formula in one persistent TeX session.
            for (const auto &input : fixedInputs) {
                auto warmScene = tex_scene::buildScene(input, owner, normalMeasurement, true);
                (void)warmScene;
                requestFramesInactive &= hasNoActiveRequest();
            }
            retainedResources = owner.fonts.svg_resources;
            retained.emplace(tex_scene::buildScene(fixedInputs[0], owner, normalMeasurement, true));
            {
                svg::SvgWriteOptions options;
                options.font_cache = svg::FontCacheMode::none;
                options.font_resources = &owner.fonts.svg_resources;
                retainedSvg = svg::write(*retained, options);
            }

            const auto baselineLive = liveCounts();
            const auto baselineHeap = math_heap_test::snapshot();

            const auto beforeUnknownLive = liveCounts();
            const auto beforeUnknownTotals = totals();
            const auto beforeUnknownHeap = math_heap_test::snapshot();
            {
                ThrowOnUnknownText failingMeasurement;
                bool threw = false;
                try {
                    auto failed =
                        tex_scene::buildScene(fixedInputs[6], owner, failingMeasurement, true);
                    (void)failed;
                } catch (std::runtime_error const &) {
                    threw = true;
                }
                requestFramesInactive &= hasNoActiveRequest();
                measurementFailureCalls = failingMeasurement.calls;
                measurementExceptionBalanced = threw && measurementFailureCalls > 0;
            }
            const auto afterUnknownLive = liveCounts();
            const auto afterUnknownTotals = totals();
            const auto afterUnknownHeap = math_heap_test::snapshot();
            measurementExceptionBalanced &=
                same(beforeUnknownLive, afterUnknownLive) &&
                requestBalanced(beforeUnknownTotals, afterUnknownTotals) &&
                (!math_heap_test::has_crt_heap_snapshot ||
                 math_heap_test::same_heap(beforeUnknownHeap, afterUnknownHeap));
            const auto beforeUnknownRecovery = liveCounts();
            const auto beforeUnknownRecoveryHeap = math_heap_test::snapshot();
            {
                auto recovered =
                    tex_scene::buildScene(fixedInputs[6], owner, normalMeasurement, true);
                serialize(recovered, owner.fonts.svg_resources);
            }
            requestFramesInactive &= hasNoActiveRequest();
            const auto afterUnknownRecovery = liveCounts();
            const auto afterUnknownRecoveryHeap = math_heap_test::snapshot();
            measurementExceptionBalanced &=
                same(beforeUnknownRecovery, afterUnknownRecovery) &&
                same(baselineLive, afterUnknownRecovery) &&
                (!math_heap_test::has_crt_heap_snapshot ||
                 math_heap_test::same_heap(beforeUnknownRecoveryHeap, afterUnknownRecoveryHeap));

            const auto beforeMerrorLive = liveCounts();
            const auto beforeMerrorTotals = totals();
            const auto beforeMerrorHeap = math_heap_test::snapshot();
            {
                auto merror = tex_scene::buildScene({R"(\frac{a}{)", false, 16, 8, 1280, {}}, owner,
                                                    normalMeasurement, true);
                svg::SvgWriteOptions options;
                options.font_cache = svg::FontCacheMode::none;
                options.font_resources = &owner.fonts.svg_resources;
                const auto output = svg::write(merror, options);
                malformedOutputMarkedAsError = output.find("data-mjx-error=") != std::string::npos;
            }
            requestFramesInactive &= hasNoActiveRequest();
            const auto afterMerrorLive = liveCounts();
            const auto afterMerrorTotals = totals();
            const auto afterMerrorHeap = math_heap_test::snapshot();
            malformedMerrorBalanced =
                same(beforeMerrorLive, afterMerrorLive) &&
                requestBalanced(beforeMerrorTotals, afterMerrorTotals) &&
                (!math_heap_test::has_crt_heap_snapshot ||
                 math_heap_test::same_heap(beforeMerrorHeap, afterMerrorHeap));

            const auto beforeWriterLive = liveCounts();
            const auto beforeWriterTotals = totals();
            const auto beforeWriterHeap = math_heap_test::snapshot();
            {
                auto value = tex_scene::buildScene(fixedInputs[0], owner, normalMeasurement, true);
                bool threw = false;
                try {
                    svg::SvgWriteOptions missingResources;
                    missingResources.font_cache = svg::FontCacheMode::none;
                    (void)svg::write(value, missingResources);
                } catch (svg::UnsupportedError const &) {
                    threw = true;
                }
                if (threw)
                    serialize(value, owner.fonts.svg_resources);
                writerExceptionBalanced = threw;
            }
            requestFramesInactive &= hasNoActiveRequest();
            const auto afterWriterLive = liveCounts();
            const auto afterWriterTotals = totals();
            const auto afterWriterHeap = math_heap_test::snapshot();
            writerExceptionBalanced &=
                same(beforeWriterLive, afterWriterLive) &&
                requestBalanced(beforeWriterTotals, afterWriterTotals) &&
                (!math_heap_test::has_crt_heap_snapshot ||
                 math_heap_test::same_heap(beforeWriterHeap, afterWriterHeap));
            const auto beforeWriterRecovery = liveCounts();
            const auto beforeWriterRecoveryHeap = math_heap_test::snapshot();
            {
                auto recovered =
                    tex_scene::buildScene(fixedInputs[0], owner, normalMeasurement, true);
                serialize(recovered, owner.fonts.svg_resources);
            }
            requestFramesInactive &= hasNoActiveRequest();
            const auto afterWriterRecovery = liveCounts();
            const auto afterWriterRecoveryHeap = math_heap_test::snapshot();
            writerExceptionBalanced &=
                same(beforeWriterRecovery, afterWriterRecovery) &&
                same(baselineLive, afterWriterRecovery) &&
                (!math_heap_test::has_crt_heap_snapshot ||
                 math_heap_test::same_heap(beforeWriterRecoveryHeap, afterWriterRecoveryHeap));

            const auto runRequests = [&](std::size_t count, std::size_t offset,
                                         std::size_t &firstFailure) {
                for (std::size_t iteration = 0; iteration < count; ++iteration) {
                    const auto beforeLive = liveCounts();
                    const auto beforeTotals = totals();
                    const auto beforeHeap = math_heap_test::snapshot();
                    const auto start = Clock::now();
                    Clock::time_point built;
                    {
                        auto requestScene = tex_scene::buildScene(
                            fixedInputs[(offset + iteration) % fixedInputs.size()], owner,
                            normalMeasurement, true);
                        if ((offset + iteration) % 2 == 0)
                            serialize(requestScene, owner.fonts.svg_resources);
                        built = Clock::now();
                        requestNanoseconds += static_cast<std::uint64_t>(
                            std::chrono::duration_cast<std::chrono::nanoseconds>(built - start)
                                .count());
                    }
                    const auto end = Clock::now();
                    cleanupNanoseconds += static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(end - built).count());
                    const auto afterLive = liveCounts();
                    const auto afterTotals = totals();
                    const auto afterHeap = math_heap_test::snapshot();
                    if (offset + iteration < firstHeapSamples.size())
                        firstHeapSamples[offset + iteration] = afterHeap;
                    const bool objectStable = same(beforeLive, afterLive) &&
                                              same(afterLive, baselineLive) &&
                                              requestBalanced(beforeTotals, afterTotals);
                    const bool heapStable = !math_heap_test::has_crt_heap_snapshot ||
                                            (math_heap_test::same_heap(beforeHeap, afterHeap) &&
                                             math_heap_test::same_heap(baselineHeap, afterHeap));
                    requestFramesInactive &= hasNoActiveRequest();
                    if ((!objectStable || !heapStable) && firstFailure == 0)
                        firstFailure = offset + iteration + 1;
                }
            };

            runRequests(192, 0, failedRequestIndex);
            const auto after192Live = liveCounts();
            const auto after192Heap = math_heap_test::snapshot();
            runRequests(768, 192, failedRequestIndex);
            const auto after960Live = liveCounts();
            const auto after960Heap = math_heap_test::snapshot();

            if (failedRequestIndex || !measurementExceptionBalanced || !malformedMerrorBalanced ||
                !writerExceptionBalanced) {
                const auto report = [](char const *stage, math_heap_test::Snapshot const &value) {
                    std::cerr << "lifetime heap " << stage
                              << " normal_blocks=" << value.normal_blocks
                              << " normal_bytes=" << value.normal_bytes
                              << " client_blocks=" << value.client_blocks
                              << " client_bytes=" << value.client_bytes << '\n';
                };
                report("baseline", baselineHeap);
                report("before_measurement_error", beforeUnknownHeap);
                report("after_measurement_error", afterUnknownHeap);
                report("after_measurement_recovery", afterUnknownRecoveryHeap);
                report("before_merror", beforeMerrorHeap);
                report("after_merror", afterMerrorHeap);
                report("before_writer_error", beforeWriterHeap);
                report("after_writer_error", afterWriterHeap);
                for (std::size_t i = 0; i < firstHeapSamples.size(); ++i) {
                    std::cerr << "lifetime heap sample=" << i + 1
                              << " input=" << i % fixedInputs.size() << '\n';
                    report("after_request", firstHeapSamples[i]);
                }
                report("after192", after192Heap);
                report("after960", after960Heap);
            }

            expect(measurementExceptionBalanced) << "unknown-text measurement exception returns "
                                                    "objects/heap and next request succeeds";
            expect(malformedMerrorBalanced)
                << "malformed TeX merror request serializes and returns objects/heap";
            expect(malformedOutputMarkedAsError)
                << "malformed TeX request retains the Source merror marker in output";
            expect(writerExceptionBalanced)
                << "real missing-resource writer error recovers and returns objects/heap";
            expect(same(baselineLive, after192Live) && same(baselineLive, after960Live))
                << "live MML/Common/DOM counts return to warmed baseline at 192 and 960";
            expect(requestFramesInactive)
                << "caller-owned TeX request frame is unbound after every request and error";
            if (math_heap_test::has_crt_heap_snapshot) {
                expect(math_heap_test::same_heap(baselineHeap, after192Heap) &&
                       math_heap_test::same_heap(baselineHeap, after960Heap))
                    << "Windows Debug CRT normal/client blocks and bytes return to baseline";
            }
            expect(failedRequestIndex == 0)
                << "every mixed request boundary returns live counts and CRT heap; first="
                << failedRequestIndex;
        }

        expect(same(globalLiveBeforeOwner, liveCounts()))
            << "InputOwner destruction releases request Lite DOM and persistent-session state";
        std::string afterOwnerSvg;
        {
            svg::SvgWriteOptions options;
            options.font_cache = svg::FontCacheMode::none;
            options.font_resources = &retainedResources;
            afterOwnerSvg = svg::write(*retained, options);
        }
        expect(afterOwnerSvg == retainedSvg)
            << "retained frozen Scene serializes identically after InputOwner destruction";
        expect(requestNanoseconds > 0 && cleanupNanoseconds > 0)
            << "request execution and Scene cleanup durations were recorded";
        std::cout << "request lifetime requests=192+768 live_domains=mml,node,wrapper,dom "
                  << "crt_debug=" << math_heap_test::has_crt_heap_snapshot
                  << " request_ns=" << requestNanoseconds << " cleanup_ns=" << cleanupNanoseconds
                  << '\n';
    };
};
