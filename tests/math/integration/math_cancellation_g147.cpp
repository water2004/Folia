#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include "support/folia_test.hpp"
#include "math/support/tex_scene.hpp"

import folia.math;
import folia.math.cancellation;
import folia.math.output.common;
import folia.math.output.scene;

using namespace boost::ut;
using folia::math::Renderer;
using folia::math::RenderError;
using folia::math::RenderOptions;
namespace cancellation = folia::math::cancellation;
namespace common = folia::math::output::common;
namespace scene = folia::math::output::scene;
namespace tex_scene = tex_scene_test;

namespace {
std::string errorKind(auto &&action) {
    try {
        action();
    } catch (RenderError const &error) {
        return error.name;
    }
    throw std::runtime_error("Expected RenderError");
}

struct StopAfterUnknownText final : common::MeasurementTarget {
    tex_scene::LiteUnknownTextMeasurement delegate_;
    std::stop_source &stop_;
    std::size_t calls{};

    StopAfterUnknownText(tex_scene::InputOwner &owner, std::stop_source &stop) noexcept
        : delegate_(owner.adaptor), stop_(stop) {}

    common::ExtendedMetrics measureMetrics(common::MetricRequest const &request) override {
        return delegate_.measureMetrics(request);
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &request) override {
        auto result = delegate_.measureUnknownText(request);
        ++calls;
        stop_.request_stop();
        return result;
    }
};
} // namespace

suite math_cancellation_g147 = [] {
    "public_renderer_overloads_and_precancelled_sessions_remain_usable"_test = [] {
        Renderer renderer;
        const auto rendered = renderer.render(std::u16string_view(u"x"), {});
        expect(!rendered.svg.empty());
        expect(!renderer.toSvg(std::u16string_view(u"x"), {}).empty());
        expect(!renderer.render(std::string_view("x"), {}).svg.empty());
        expect(!renderer.toSvg(std::string_view("x"), {}).empty());
        expect(!renderer.renderScene(std::u16string_view(u"x"), {}).root().entries.empty());
        expect(!renderer.renderScene(std::string_view("x"), {}).root().entries.empty());

        std::stop_source source;
        source.request_stop();
        expect(eq(errorKind([&] {
                      (void)renderer.render(std::string_view("x"), source.get_token(), {});
                  }),
                  std::string("Cancelled")));
        RenderOptions expired;
        expired.deadline = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
        expect(eq(errorKind([&] { (void)renderer.render(std::string_view("x"), {}, expired); }),
                  std::string("Cancelled")));
        expect(eq(errorKind([&] {
                      (void)renderer.renderScene(std::u16string_view(u"x"), source.get_token(), {});
                  }),
                  std::string("Cancelled")));
        expect(!renderer.render(std::string_view("x")).svg.empty())
            << "pre-cancellation leaves Renderer session usable";
    };

    "public_deadline_cancels_inflight_bounded_input_and_invalidates_session"_test = [] {
        Renderer renderer;
        const std::u16string input(4000, u'x');
        RenderOptions options;
        options.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(5);
        expect(
            eq(errorKind([&] { (void)renderer.render(std::u16string_view(input), {}, options); }),
               std::string("Cancelled")));
        expect(eq(errorKind([&] { (void)renderer.render(std::string_view("x")); }),
                  std::string("SessionUnusable")));
    };

    "typed_lite_measurement_cancellation_unwinds_request_then_next_scene_succeeds"_test = [] {
        tex_scene::InputOwner owner;
        std::stop_source source;
        cancellation::Request request(source.get_token());
        bool observed = false;
        std::size_t measurementCalls = 0;
        {
            cancellation::Scope scope(request);
            StopAfterUnknownText measurement(owner, source);
            try {
                (void)tex_scene::buildScene({R"(\text{漢字😀 é})", false, 16, 8, 1280, {}}, owner,
                                            measurement, true);
            } catch (cancellation::Cancelled const &) {
                observed = true;
            }
            measurementCalls = measurement.calls;
        }
        expect(observed && measurementCalls > 0)
            << "actual Lite unknown-text measurement triggers an existing pipeline checkpoint";
        tex_scene::LiteUnknownTextMeasurement normal(owner.adaptor);
        auto recovered = tex_scene::buildScene({R"(\text{漢字😀 é})", false, 16, 8, 1280, {}},
                                               owner, normal, true);
        expect(recovered.root().entries.size() > 0)
            << "request unwinding permits a subsequent scene build";
    };

    "nested_request_scopes_restore_prior_context_and_are_thread_local"_test = [] {
        std::stop_source outerSource;
        cancellation::Request activeOuter(outerSource.get_token());
        {
            cancellation::Scope outerScope(activeOuter);
            std::stop_source innerSource;
            innerSource.request_stop();
            cancellation::Request inner(innerSource.get_token());
            {
                cancellation::Scope innerScope(inner);
                bool wasCancelled = false;
                try {
                    cancellation::checkpoint();
                } catch (cancellation::Cancelled const &) {
                    wasCancelled = true;
                }
                expect(wasCancelled);
            }
            bool restored = false;
            try {
                cancellation::checkpoint();
            } catch (cancellation::Cancelled const &) {
                restored = true;
            }
            expect(!restored) << "inner scope restores the non-cancelled outer request";
            bool otherThreadCancelled = false;
            std::jthread worker([&] {
                try {
                    cancellation::checkpoint();
                } catch (cancellation::Cancelled const &) {
                    otherThreadCancelled = true;
                }
            });
            worker.join();
            expect(!otherThreadCancelled) << "cancellation request is thread-local";
            outerSource.request_stop();
            bool outerCancelled = false;
            try {
                cancellation::checkpoint();
            } catch (cancellation::Cancelled const &) {
                outerCancelled = true;
            }
            expect(outerCancelled);
        }
    };

    "renderer_wrong_thread_is_rejected_without_poisoning_owner_thread"_test = [] {
        Renderer renderer;
        std::string workerError;
        std::jthread worker([&] {
            try {
                (void)renderer.render(std::string_view("x"));
            } catch (RenderError const &error) {
                workerError = error.name;
            }
        });
        worker.join();
        expect(eq(workerError, std::string("WrongThread")));
        expect(!renderer.render(std::string_view("x")).svg.empty())
            << "wrong-thread rejection leaves owning thread usable";
    };

    "successful_results_keep_complete_scene_and_svg_contract"_test = [] {
        Renderer renderer;
        const auto result = renderer.render(std::u16string_view(u"\\frac{a}{b}"));
        expect(!result.svg.empty());
        expect(!result.scene.root().entries.empty());
        expect(eq(renderer.toSvg(std::u16string_view(u"\\frac{a}{b}")), result.svg));
        expect(!renderer.renderScene(std::string_view("x+1")).root().entries.empty());
    };
};
