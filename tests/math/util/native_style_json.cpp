// Focused native coverage for the Source-mapped StyleJsonSheet CSS contract.
#include "support/folia_test.hpp"
import folia.math.util.style_json;

namespace {
using namespace boost::ut;
namespace sj = folia::math::style_json;

sj::StyleEntry text(std::u16string name, std::u16string value) {
    return {std::move(name), std::move(value)};
}

sj::StyleEntry number(std::u16string name, double value) {
    return {std::move(name), value};
}

sj::StyleEntry group(std::u16string name, sj::StyleJson value) {
    return {std::move(name), std::move(value)};
}

bool same16(std::u16string_view actual, std::u16string_view expected) {
    return actual.compare(expected) == 0;
}
} // namespace

suite native_style_json = [] {
    "nested_media_and_font_face_keep_source_formatting"_test = [] {
        sj::StyleJsonSheet sheet({group(
            u".x",
            {text(u"color", u"red"),
             group(u"@media test",
                   {group(u".child", {number(u"font-size", 12),
                                      group(u"@font-face", {text(u"font", u"MathJax")})})})})});
        expect(
            same16(sheet.cssText(),
                   u".x {\n  color: red;\n  @media test {\n      .child {\n      font-size: 12;\n  "
                   u"            @font-face {\n        font: MathJax;\n      }\n    }\n  }\n}"));
        expect(std::ranges::equal(sheet.getStyleRules(),
                                  std::vector<std::u16string>{sheet.cssText()}));
    };
    "addStyles_replaces_in_place_appends_and_remove_reinsert_moves_selector"_test = [] {
        sj::StyleJsonSheet sheet({group(u".a", {text(u"color", u"red"), number(u"opacity", 0.5)}),
                                  group(u".b", {text(u"display", u"block")})});
        const sj::StyleJson addition{
            group(u".a", {text(u"color", u"blue"), text(u"padding", u"1em")}),
            group(u".c", {text(u"display", u"inline")})};
        sheet.addStyles(addition);
        expect(same16(sheet.cssText(),
                      u".a {\n  color: blue;\n  opacity: 0.5;\n  padding: 1em;\n}\n\n.b {\n  "
                      u"display: block;\n}\n\n.c {\n  display: inline;\n}"));
        const std::array<std::u16string, 2> removed{u".a", u".missing"};
        sheet.removeStyles(removed);
        sheet.addStyles({group(u".a", {text(u"color", u"green")})});
        expect(std::ranges::equal(sheet.getStyleRules(), std::vector<std::u16string>{
            u".b {\n  display: block;\n}", u".c {\n  display: inline;\n}",
            u".a {\n  color: green;\n}"}));
    };
    "shallow_group_replacement_and_input_are_owned_snapshots"_test = [] {
        sj::StyleJsonSheet sheet(
            {group(u".a", {text(u"color", u"red"),
                           group(u"@media", {group(u".child", {text(u"color", u"old"),
                                                               text(u"padding", u"2em")})})})});
        sj::StyleJson replacement{
            group(u".a", {group(u"@media", {group(u".child", {text(u"color", u"new")})})})};
        sheet.addStyles(replacement);
        std::get<sj::StyleJson>(replacement[0].value)[0].name = u"mutated input";
        expect(same16(
            sheet.cssText(),
            u".a {\n  color: red;\n  @media {\n      .child {\n      color: new;\n    }\n  }\n}"));
    };
    "empty_groups_numbers_and_utf16_are_serialized"_test = [] {
        sj::StyleJsonSheet sheet(
            {group(u"empty", {}), group(u"@media empty", {}),
             group(u"n", {number(u"fraction", 0.5), number(u"negativeZero", -0.0),
                          number(u"exponent", 1e21), number(u"smallExponent", 1e-7),
                          number(u"notANumber", std::numeric_limits<double>::quiet_NaN()),
                          number(u"infinity", std::numeric_limits<double>::infinity())}),
             group(u"😀 selector", {text(u"字体", u"数学😀")}),
             group(std::u16string{0xD800},
                   {text(std::u16string{0xDC00}, std::u16string{0xD801})})});
        const std::u16string loneSurrogateRule{0xD800, u' ', u'{',   u'\n', u' ',  u' ', 0xDC00,
                                               u':',   u' ', 0xD801, u';',  u'\n', u'}'};
        expect(
            same16(sheet.cssText(),
                   u"empty {\n\n}\n\n@media empty {\n\n}\n\nn {\n  fraction: 0.5;\n  negativeZero: "
                   u"0;\n  exponent: 1e+21;\n  smallExponent: 1e-7;\n  notANumber: NaN;\n  "
                   u"infinity: Infinity;\n}\n\n😀 selector {\n  字体: 数学😀;\n}\n\n" +
                       loneSurrogateRule));
        expect(same16(sheet.getStyleRules().back(), loneSurrogateRule));
    };
    "explicit_rules_and_sheet_lifecycle"_test = [] {
        sj::StyleJsonSheet sheet({group(u".a", {text(u"color", u"red")})});
        const sj::StyleJson custom{group(u".custom", {text(u"padding", u"1em")})};
        expect(std::ranges::equal(sheet.getStyleRules(custom, u"  "),
                                  std::vector<std::u16string>{u"  .custom {\n    padding: 1em;\n  }"}));
        expect(same16(sheet.getStyleString(), sheet.cssText()));
        sheet.clear();
        expect(sheet.cssText().empty());
        expect(sheet.getStyleRules().empty());
    };
    "selector_values_must_be_css_groups"_test = [] {
        bool rejected = false;
        try {
            sj::StyleJsonSheet invalid({text(u".bad", u"not a group")});
        } catch (std::invalid_argument const &) {
            rejected = true;
        }
        expect(rejected);
    };
};
