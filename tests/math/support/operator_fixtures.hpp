#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

template <class TexClass, class MoPattern>
struct OperatorFixtureData {
    struct Property { std::u16string key; bool isText, boolean; std::u16string text; };
    struct Entry {
        std::u16string form, text;
        double left, right;
        TexClass texClass;
        std::size_t propertyOffset, propertyCount;
    };
    struct Named {
        std::u16string name;
        double left, right;
        TexClass texClass;
        std::size_t propertyOffset, propertyCount;
    };
    struct Range {
        char32_t first, last;
        TexClass texClass;
        std::u16string kind;
        std::optional<std::u16string> variant;
    };
    struct Query {
        std::u16string input;
        char32_t first, last;
        TexClass texClass;
        std::u16string kind, variant;
        bool hasVariant;
    };
    struct Mask { MoPattern pattern; unsigned shape; char16_t first; std::string expected; };
    struct Sample { MoPattern pattern; std::u16string text; bool expected; };
    struct Retained {
        MoPattern pattern;
        std::u16string source;
        std::string flags;
        std::size_t minimum;
        std::optional<std::size_t> maximum;
        std::vector<std::pair<unsigned, unsigned>> ranges;
        std::vector<unsigned> members;
        std::string product;
    };

    static TexClass texClass(std::string_view name) {
        if (name == "none") return TexClass::none;
        if (name == "ord") return TexClass::ord;
        if (name == "op") return TexClass::op;
        if (name == "bin") return TexClass::bin;
        if (name == "rel") return TexClass::rel;
        if (name == "open") return TexClass::open;
        if (name == "close") return TexClass::close;
        if (name == "punct") return TexClass::punct;
        if (name == "inner") return TexClass::inner;
        throw std::runtime_error("Unknown operator fixture TeX class: " + std::string{name});
    }

    static MoPattern pattern(std::string_view name) {
        if (name == "pseudoScripts") return MoPattern::pseudoScripts;
        if (name == "primes") return MoPattern::primes;
        if (name == "opPattern") return MoPattern::opPattern;
        if (name == "mathaccents") return MoPattern::mathaccents;
        if (name == "mathaccentsWithWidth") return MoPattern::mathaccentsWithWidth;
        throw std::runtime_error("Unknown operator fixture pattern: " + std::string{name});
    }

    static double number(std::string_view value) { return std::bit_cast<double>(binary64(value)); }
    static std::u16string text(std::string_view value) { return folia::test::data::decode_utf16(value); }
    static char32_t point(std::string_view value) { return static_cast<char32_t>(integer<unsigned>(value)); }
    static Property property(const std::vector<std::string>& r) {
        return {text(r[0]), boolean(r[1]), boolean(r[2]), text(r[3])};
    }
    static Sample sample(const std::vector<std::string>& r) {
        return {pattern(r[0]), text(r[1]), boolean(r[2])};
    }

    static const auto& properties() {
        static const auto result = records<Property>(
            "math/fixtures/operators/properties.tsv", 4, 944, property);
        return result;
    }
    static const auto& namedProperties() {
        static const auto result = records<Property>(
            "math/fixtures/operators/named-properties.tsv", 4, 28, property);
        return result;
    }
    static const auto& entries() {
        static const auto result = records<Entry>(
            "math/fixtures/operators/entries.tsv", 7, 1252, [](const auto& r) {
                return Entry{text(r[0]), text(r[1]), number(r[2]), number(r[3]), texClass(r[4]),
                             integer<std::size_t>(r[5]), integer<std::size_t>(r[6])};
            });
        return result;
    }
    static const auto& named() {
        static const auto result = records<Named>(
            "math/fixtures/operators/named.tsv", 6, 27, [](const auto& r) {
                return Named{text(r[0]), number(r[1]), number(r[2]), texClass(r[3]),
                             integer<std::size_t>(r[4]), integer<std::size_t>(r[5])};
            });
        return result;
    }
    static const auto& ranges() {
        static const auto result = records<Range>(
            "math/fixtures/operators/ranges.tsv", 6, 50, [](const auto& r) {
                return Range{point(r[0]), point(r[1]), texClass(r[2]), text(r[3]),
                             boolean(r[4]) ? std::optional{text(r[5])} : std::nullopt};
            });
        return result;
    }
    static const auto& spacing() {
        static const auto result = records<std::array<double, 2>>(
            "math/fixtures/operators/spacing.tsv", 2, 8, [](const auto& r) {
                return std::array{number(r[0]), number(r[1])};
            });
        return result;
    }
    static const auto& queries() {
        static const auto result = records<Query>(
            "math/fixtures/operators/queries.tsv", 7, 1480, [](const auto& r) {
                return Query{text(r[0]), point(r[1]), point(r[2]), texClass(r[3]),
                             text(r[4]), text(r[5]), boolean(r[6])};
            });
        return result;
    }
    static const auto& masks() {
        static const auto result = records<Mask>(
            "math/fixtures/operators/masks.tsv", 4, 30, [](const auto& r) {
                const auto first = integer<unsigned>(r[2]);
                if (first > 0xffff) throw std::runtime_error("Operator mask unit is outside UTF-16");
                return Mask{pattern(r[0]), integer<unsigned>(r[1]), static_cast<char16_t>(first), bytes(r[3])};
            });
        return result;
    }
    static const auto& retainedSamples() {
        static const auto result = records<Sample>(
            "math/fixtures/operators/retained-samples.tsv", 3, 255, sample);
        return result;
    }
    static const auto& independentTruths() {
        static const auto result = records<Sample>(
            "math/fixtures/operators/independent-truths.tsv", 3, 40, sample);
        return result;
    }

    template <class Decode>
    static void commaSeparated(std::string_view value, Decode decode) {
        if (value.empty()) return;
        std::size_t start{};
        for (;;) {
            const auto end = value.find(',', start);
            decode(value.substr(start, end == std::string_view::npos ? end : end - start));
            if (end == std::string_view::npos) break;
            start = end + 1;
        }
    }

    static const auto& retainedValues() {
        static const auto result = records<Retained>(
            "math/fixtures/operators/retained-values.tsv", 10, 5, [](const auto& r) {
                Retained value{pattern(r[0]), text(r[1]), bytes(r[2]), integer<std::size_t>(r[3]),
                    r[4] == "null" ? std::nullopt : std::optional{integer<std::size_t>(r[4])}, {}, {}, bytes(r[9])};
                commaSeparated(r[6], [&](std::string_view pair) {
                    const auto split = pair.find(':');
                    if (split == std::string_view::npos)
                        throw std::runtime_error("Operator fixture range needs two bounds");
                    value.ranges.emplace_back(integer<unsigned>(pair.substr(0, split)),
                                              integer<unsigned>(pair.substr(split + 1)));
                });
                commaSeparated(r[8], [&](std::string_view member) {
                    value.members.push_back(integer<unsigned>(member));
                });
                if (value.ranges.size() != integer<std::size_t>(r[5]) ||
                    value.members.size() != integer<std::size_t>(r[7]))
                    throw std::runtime_error("Operator fixture range/member count changed");
                return value;
            });
        return result;
    }
};

} // namespace math_test::fixtures
