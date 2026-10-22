#include "support/folia_test.hpp"
import folia.math.util.js_case;

namespace {
using namespace boost::ut;
namespace c = folia::math::js_case;

std::u16string units(std::initializer_list<char16_t> values) {
    return {values.begin(), values.end()};
}

std::u16string point(char32_t value) {
    if (value <= 0xFFFF) return {static_cast<char16_t>(value)};
    value -= 0x10000;
    return {static_cast<char16_t>(0xD800 + (value >> 10)),
            static_cast<char16_t>(0xDC00 + (value & 1023))};
}

struct Mapping {
    char32_t point;
    std::u16string output;
};
struct Context {
    char32_t first, last;
    unsigned flags;
};
struct Example {
    std::u16string input, output;
};

#include "fixtures/tex-source/js_case_source_values.inc"

suite native_js_case = [] {
    "math_port_case_host_full_unicode_domain"_test = [] {
        std::size_t next = 0;
        bool passed = true;
        for (char32_t value = 0; value <= 0x10FFFF; ++value) {
            const auto input = point(value);
            auto expected = input;
            if (next < std::size(sourceCaseMappings) &&
                sourceCaseMappings[next].point == value) {
                expected = sourceCaseMappings[next++].output;
            }
            if (c::lowerCase(input) != expected) {
                std::cerr << "lowercase mismatch at " << unsigned(value) << '\n';
                passed = false;
                break;
            }
        }
        expect(passed);
        expect(next == std::size(sourceCaseMappings));
    };

    "math_port_case_host_full_unicode_sigma_contexts"_test = [] {
        std::size_t index = 0;
        bool passed = true;
        for (char32_t value = 0; value <= 0x10FFFF; ++value) {
            while (value > sourceCaseContexts[index].last) ++index;
            const auto flags = sourceCaseContexts[index].flags;
            const auto input = point(value);
            const auto before = c::lowerCase(input + u"Σ");
            const auto ignored = c::lowerCase(u"A" + input + u"Σ");
            const auto after = c::lowerCase(u"AΣ" + input);
            if ((before.back() == u'ς') != bool(flags & 1) ||
                (ignored.back() == u'ς') != bool(flags & 2) ||
                (after[1] == u'ς') != bool(flags & 4)) {
                std::cerr << "sigma context mismatch at " << unsigned(value) << '\n';
                passed = false;
                break;
            }
        }
        expect(passed);
    };

    "math_port_case_host_strings_context_and_lone_surrogates"_test = [] {
        for (const auto &example : sourceCaseExamples) {
            expect(static_cast<bool>(c::lowerCase(example.input) == example.output));
        }
    };
};
}
