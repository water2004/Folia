#include "support/folia_test.hpp"
import folia.math.core.mml_tree.operator_dictionary;
import folia.math.core.mml_tree.mo_patterns;

using namespace boost::ut;
using namespace folia::math;
using namespace folia::math::mml;
using namespace folia::math::mml::operator_dict;

#include "fixtures/tex-source/operator_source_entries.inc"
#include "fixtures/tex-source/operator_source_named.inc"
#include "fixtures/tex-source/operator_source_ranges.inc"
#include "fixtures/tex-source/operator_source_spacing.inc"
#include "fixtures/tex-source/operator_source_queries.inc"

namespace {
bool sourcePropertyEqual(const AttributeValue& actual,
                         const SourceOperatorProperty& expected) {
    if (expected.isText) {
        const auto* value = std::get_if<std::u16string>(&actual);
        return value && *value == expected.text;
    }
    const auto* value = std::get_if<bool>(&actual);
    return value && *value == expected.boolean;
}

bool sourceDefinitionEqual(const OperatorDef& actual, double left, double right,
                           TexClass texClass, std::size_t propertyOffset,
                           std::size_t propertyCount,
                           const SourceOperatorProperty* properties) {
    if (actual.left != left || actual.right != right ||
        actual.texClass != texClass || actual.properties.size() != propertyCount)
        return false;
    for (std::size_t i = 0; i < propertyCount; ++i) {
        if (actual.properties[i].first != properties[propertyOffset + i].key ||
            !sourcePropertyEqual(actual.properties[i].second,
                                 properties[propertyOffset + i]))
            return false;
    }
    return true;
}

bool sourceRangeEqual(const RangeDef& actual, const SourceRangeQuery& expected) {
    return actual.first == expected.first && actual.last == expected.last &&
           actual.texClass == expected.texClass && actual.kind == expected.kind &&
           actual.variant.has_value() == expected.hasVariant &&
           (!expected.hasVariant || *actual.variant == expected.variant);
}
} // namespace
#include "fixtures/tex-source/mo_pattern_utf16_masks.inc"
#include "fixtures/tex-source/mo_pattern_retained_values.inc"
namespace {
std::u16string sourceMaskInput(char16_t c, unsigned shape, char16_t first) {
    if (shape == 0) return std::u16string(1, c);
    if (shape == 1) return std::u16string(2, c);
    if (shape == 2) return std::u16string(3, c);
    if (shape == 3) return std::u16string(1, first) + c;
    if (shape == 4) return std::u16string(1, c) + first;
    return std::u16string(1, first) + c + first;
}
std::string sourceMask(const SourceMoMask& row) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (unsigned start = 0; start < 65536; start += 4) {
        unsigned bits = 0;
        for (unsigned j = 0; j < 4; ++j)
            if (matchMoPattern(row.pattern, sourceMaskInput(static_cast<char16_t>(start + j), row.shape, row.first)))
                bits |= 1U << j;
        result.push_back(digits[bits]);
    }
    return result;
}
}

suite typed_operator_dictionary = [] {
    "all pinned Source operator records preserve order and every field"_test = [] {
        const auto actual = OPTABLE();
        expect(actual.size() == std::size(source_operator_entries));
        const auto count = std::min(actual.size(), std::size(source_operator_entries));
        for (std::size_t i = 0; i < count; ++i) {
            const auto& expected = source_operator_entries[i];
            const auto* lookup = findOperatorDefinition(expected.form, expected.text);
            expect(actual[i].form == expected.form);
            expect(static_cast<bool>(actual[i].text == expected.text));
            expect(sourceDefinitionEqual(actual[i].definition, expected.left,
                expected.right, expected.texClass, expected.propertyOffset,
                expected.propertyCount, source_operator_properties));
            expect(lookup != nullptr);
            if (lookup) {
                expect(sourceDefinitionEqual(*lookup, expected.left, expected.right,
                    expected.texClass, expected.propertyOffset, expected.propertyCount,
                    source_operator_properties));
            }
        }
    };

    "all 27 pinned Source named MO records preserve every field"_test = [] {
        const auto actual = MO();
        expect(actual.size() == std::size(source_named_operators));
        const auto count = std::min(actual.size(), std::size(source_named_operators));
        for (std::size_t i = 0; i < count; ++i) {
            const auto& expected = source_named_operators[i];
            expect(actual[i].first == expected.name);
            expect(sourceDefinitionEqual(actual[i].second, expected.left,
                expected.right, expected.texClass, expected.propertyOffset,
                expected.propertyCount, source_named_properties));
        }
    };

    "all 50 pinned Source ranges and eight spacing rows match"_test = [] {
        const auto actualRanges = RANGES();
        expect(actualRanges.size() == std::size(source_ranges));
        const auto rangeCount = std::min(actualRanges.size(), std::size(source_ranges));
        for (std::size_t i = 0; i < rangeCount; ++i) {
            const auto& actual = actualRanges[i];
            const auto& expected = source_ranges[i];
            expect(actual.first == expected.first);
            expect(actual.last == expected.last);
            expect(actual.texClass == expected.texClass);
            expect(actual.kind == expected.kind);
            expect(actual.variant == expected.variant);
        }
        const auto actualSpacing = MMLSPACING();
        expect(actualSpacing.size() == std::size(source_spacing));
        const auto spacingCount = std::min(actualSpacing.size(), std::size(source_spacing));
        for (std::size_t i = 0; i < spacingCount; ++i)
            expect(actualSpacing[i] == source_spacing[i]);
    };

    "all pinned Source operator lookups and range boundaries match"_test = [] {
        for (const auto& expected : source_operator_queries)
            expect(sourceRangeEqual(getRange(expected.input), expected));
    };
    "all pinned Source MoPattern UTF16 masks match every unit for each source shape"_test = [] {
        for (const auto& row : source_mo_masks)
            expect(sourceMask(row) == row.expected);
    };
    "operator_tables_preserve_source_order_and_records"_test = [] {
        const auto named = MO();
        const auto ranges = RANGES();
        const auto spacing = MMLSPACING();
        const auto operators = OPTABLE();

        expect(named.size() == 27_u);
        expect(named.front().first == u"REL");
        expect(named.front().second.left == 5.0);
        expect(named.front().second.right == 5.0);
        expect(named.front().second.texClass == TexClass::rel);
        expect(named[1].first == u"WIDEREL");
        expect(named[1].second.properties.size() == 2_u);
        expect(named[1].second.properties[0].first == u"accent");
        const auto* accent = std::get_if<bool>(&named[1].second.properties[0].second);
        expect(accent != nullptr);
        if (accent) expect(*accent);
        expect(named[1].second.properties[1].first == u"stretchy");
        const auto* stretchy = std::get_if<bool>(&named[1].second.properties[1].second);
        expect(stretchy != nullptr);
        if (stretchy) expect(*stretchy);

        expect(ranges.size() == 50_u);
        expect(ranges.front().first == U' ');
        expect(ranges.front().last == U'\x7f');
        expect(ranges.front().texClass == TexClass::rel);
        expect(ranges.front().kind == u"mo");
        expect(!ranges.front().variant.has_value());
        expect(getRange(u"中").variant == u"normal");
        expect(getRange(u"あ").variant == u"normal");
        expect(getRange(u"𠀀").variant == u"normal");
        expect(!getRange(u"a").variant.has_value());
        expect(spacing.size() == 8_u);
        expect(spacing[0] == std::array<double, 2>{0.0, 0.0});
        expect(spacing[1] == std::array<double, 2>{1.0, 2.0});
        expect(spacing[7] == std::array<double, 2>{1.0, 1.0});

        expect(operators.size() == 1252_u);
        expect(operators.front().form == u"prefix");
        expect(operators.front().text == u"!");
        expect(operators.front().definition.left == 0.0);
        expect(operators.front().definition.texClass == TexClass::ord);
        const auto* prefixPlus = findOperatorDefinition(u"prefix", u"+");
        const auto* infixPlus = findOperatorDefinition(u"infix", u"+");
        expect(prefixPlus != nullptr);
        expect(infixPlus != nullptr);
        if (prefixPlus) expect(prefixPlus->left == 0.0);
        if (infixPlus) expect(infixPlus->left == 4.0);
        expect(findOperatorDefinition(u"unknown", u"+") == nullptr);
    };

    "range_lookup_preserves_operator_precedence_and_unicode"_test = [] {
        const auto operatorRange = getRange(u"!");
        expect(operatorRange.texClass == TexClass::ord);
        expect(operatorRange.kind == u"mo");
        const auto* postfixBang = findOperatorDefinition(u"postfix", u"!");
        expect(postfixBang != nullptr);
        if (postfixBang) expect(postfixBang->texClass == TexClass::close);

        const auto latin = getRange(u"a");
        expect(latin.first == 0x20);
        expect(latin.last == 0x7f);
        expect(latin.texClass == TexClass::rel);
        const auto greek = getRange(u"\u03b1");
        expect(greek.first == 0x370);
        expect(greek.texClass == TexClass::ord);
        expect(greek.kind == u"mi");

        const std::u16string emoji{char16_t{0xd83d}, char16_t{0xde00}};
        const auto unmatched = getRange(emoji);
        expect(unmatched.first == 0x1df00);
        expect(unmatched.last == 0x1f7ff);
        expect(unmatched.texClass == TexClass::ord);
        expect(unmatched.kind == u"mo");
        // Source has an explicit empty infix operator, preceding range fallback.
        expect(getRange(u"").texClass == TexClass::ord);
    };

    "opdef_defaults_are_typed_values"_test = [] {
        const auto defaulted = OPDEF(1.25, 2.5);
        expect(defaulted.left == 1.25);
        expect(defaulted.right == 2.5);
        expect(defaulted.texClass == TexClass::bin);
        expect(defaulted.properties.empty());

        const auto explicitDef = OPDEF(0.0, 3.0, TexClass::punct,
            {{u"linebreakstyle", AttributeValue{std::u16string{u"after"}}}});
        expect(explicitDef.texClass == TexClass::punct);
        expect(explicitDef.properties.size() == 1_u);
        expect(explicitDef.properties[0].first == u"linebreakstyle");
        const auto* value = std::get_if<std::u16string>(&explicitDef.properties[0].second);
        expect(value != nullptr);
        if (value) expect(*value == u"after");
    };
};

suite typed_mo_pattern_data = [] {
    "pattern_specs_preserve_source_and_length_bounds"_test = [] {
        const auto& pseudo = moPatternSpec(MoPattern::pseudoScripts);
        expect(pseudo.source.starts_with(u"^[\"'*`"));
        expect(pseudo.flags.empty());
        expect(pseudo.minimum == 1_u);
        expect(!pseudo.maximum.has_value());

        const auto& names = moPatternSpec(MoPattern::opPattern);
        expect(names.source == u"^[a-zA-Z]{2,}$");
        expect(names.minimum == 2_u);
        expect(!names.maximum.has_value());

        const auto& accent = moPatternSpec(MoPattern::mathaccents);
        expect(accent.minimum == 1_u);
        expect(accent.maximum == std::size_t{1});
        const auto& width = moPatternSpec(MoPattern::mathaccentsWithWidth);
        expect(width.maximum == std::size_t{1});
    };

    "finite_mo_patterns_match_source_utf16_sets"_test = [] {
        expect(matchMoPattern(MoPattern::pseudoScripts, u"'\u00b9"));
        expect(!matchMoPattern(MoPattern::pseudoScripts, u"x"));
        expect(matchMoPattern(MoPattern::primes, u"\u2019\u201f"));
        expect(!matchMoPattern(MoPattern::primes, u"a"));
        expect(matchMoPattern(MoPattern::opPattern, u"sin"));
        expect(!matchMoPattern(MoPattern::opPattern, u"x"));
        expect(!matchMoPattern(MoPattern::opPattern, u"\u03b1\u03b2"));
        expect(matchMoPattern(MoPattern::mathaccents, u"\u0301"));
        expect(!matchMoPattern(MoPattern::mathaccents, u"\u0301\u0300"));
        expect(matchMoPattern(MoPattern::mathaccentsWithWidth, u"\u2190"));
        expect(!matchMoPattern(MoPattern::mathaccentsWithWidth, u"\u2195"));
    };

    "finite_mo_patterns_match_source_terminal_line_terminators"_test = [] {
        constexpr std::array<std::u16string_view, 5> suffixes{
            u"\n", u"\r", u"\u2028", u"\u2029", u"\r\n"};
        constexpr std::array<std::u16string_view, 5> examples{
            u"'\u00b9", u"\u2019", u"sin", u"\u0301", u"\u2190"};
        constexpr std::array<MoPattern, 5> kinds{
            MoPattern::pseudoScripts, MoPattern::primes, MoPattern::opPattern,
            MoPattern::mathaccents, MoPattern::mathaccentsWithWidth};

        for (std::size_t index = 0; index < kinds.size(); ++index) {
            expect(matchMoPattern(kinds[index], examples[index]));
            for (const auto suffix : suffixes) {
                std::u16string text{examples[index]};
                text.append(suffix);
                expect(!matchMoPattern(kinds[index], text));
            }
        }
    };

    "all five retained MoPattern specs preserve every Source metadata field"_test = [] {
        for (const auto& row : source_mo_retained_values) {
            const auto& actual = moPatternSpec(row.pattern);
            expect(actual.source.compare(row.source) == 0);
            expect(actual.flags == row.flags);
            expect(actual.minimum == row.minimum);
            expect(actual.maximum == row.maximum);
            expect(std::equal(actual.units.begin(), actual.units.end(),
                row.ranges.begin(), row.ranges.end(), [](const auto& a, const auto& b) {
                    return a.first == b.first && a.last == b.second;
                }));
        }
    };

    "all retained MoPattern members and mixed products match Source values"_test = [] {
        for (const auto& row : source_mo_retained_values) {
            std::vector<unsigned> actualMembers;
            for (unsigned unit = 0; unit < 65536; ++unit)
                if (matchMoPattern(row.pattern,
                    std::u16string(row.minimum, static_cast<char16_t>(unit))))
                    actualMembers.push_back(unit);
            expect(std::equal(actualMembers.begin(), actualMembers.end(),
                row.members.begin(), row.members.end()));
            const std::array<std::u16string, 7> tails{
                u"", std::u16string(1, static_cast<char16_t>(row.ranges.front().first)),
                u"\n", u"\r\n", u"\u2028", u"\u2029", std::u16string(1, char16_t{0})};
            std::string product;
            for (const auto x : row.members)
                for (const auto y : row.members)
                    for (const auto& tail : tails) {
                        std::u16string text{static_cast<char16_t>(x), static_cast<char16_t>(y)};
                        text.append(tail);
                        product.push_back(matchMoPattern(row.pattern, text) ? '1' : '0');
                    }
            expect(product == row.product);
        }
    };

    "all 255 additional and 40 independent MoPattern Source truth values match"_test = [] {
        for (const auto& row : source_mo_retained_samples)
            expect(matchMoPattern(row.pattern, row.text) == row.expected);
        for (const auto& row : source_mo_independent_truths)
            expect(matchMoPattern(row.pattern, row.text) == row.expected);
    };
};
