#include "math/support/renderer_test.hpp"

namespace {
suite native_renderer_abi = [] {
    "opaque C ABI renders the canonical Source profile and retains result storage"_test = [] {
        static_assert(sizeof(char16_t) == sizeof(std::uint16_t));
        expect(std::size(sourceCanonicalSceneCases()) == 75u) << "canonical source has 75 inputs";
        auto renderer = makeRenderer();
        if (!renderer)
            return;
        Result retained;
        char const *retainedSvg{};
        std::size_t retainedSize{};
        std::string retainedCopy;
        std::size_t unicodeInputs = 0;
        for (std::size_t i = 0; i < std::size(sourceCanonicalSceneCases()); ++i) {
            auto const &source = sourceCanonicalSceneCases()[i];
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
                checkResult(call.result.get(), source_svg_paint::materialize(mode.svg), label);
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


};
} // namespace
