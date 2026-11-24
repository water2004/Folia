// Focused native tests for the Source-mapped common style services.
#include "support/folia_test.hpp"
import folia.math.util.styles;

namespace {
using namespace boost::ut;
namespace styles = folia::math::styles;
bool same16(std::u16string_view actual, std::u16string_view expected) {
    if (actual.size() != expected.size()) return false;
    for (std::size_t i = 0; i < actual.size(); ++i)
        if (actual[i] != expected[i]) return false;
    return true;
}

struct StyleProbe : styles::Styles {
    using Styles::Styles;
    using Styles::normalizeName;
    using Styles::parentName;
    using Styles::childName;
    using Styles::sanitizeValue;
};

struct DerivedStyles : styles::Styles {
    static inline styles::StylesPatterns derivedPatterns{
        std::make_shared<folia::math::regexp_host::RegExp>(u"['\";]"),
        std::make_shared<folia::math::regexp_host::RegExp>(u"^([^;]*)(?:;|$)"),
        std::make_shared<folia::math::regexp_host::RegExp>(u"([a-z]+)\\s*=\\s*([^;]*);?", u"g"),
        std::make_shared<folia::math::regexp_host::RegExp>(u"/\\*[^]*?\\*/", u"g")};
    explicit DerivedStyles(std::u16string_view css) : Styles(css, derivedPatterns) {}
};
}

suite math_port_common_styles = [] {
    "styles_parse_preserves_source_shorthands_and_insertion_order"_test = [] {
        styles::Styles style(u"color: red; padding: 1em 2em; border-top: thin dashed red;");
        expect(same16(style.get(u"paddingTop"), u"1em"));
        expect(same16(style.get(u"padding-right"), u"2em"));
        expect(same16(style.get(u"borderTopWidth"), u"thin"));
        expect(same16(style.get(u"border-top-style"), u"dashed"));
        expect(style.cssText().find(u"color: red;") != std::u16string::npos);
        expect(style.cssText().find(u"padding: 1em 2em;") != std::u16string::npos);
        const auto list = style.styleList();
        expect(eq(list.size(), std::size_t{10}));
        expect(eq(list[0].first, std::string("color")));
        expect(eq(list[1].first, std::string("padding")));
        expect(eq(list[2].first, std::string("padding-top")));
        expect(eq(list[3].first, std::string("padding-right")));
        expect(eq(list[4].first, std::string("padding-bottom")));
        expect(eq(list[5].first, std::string("padding-left")));
        expect(eq(list[6].first, std::string("border-top")));
        expect(eq(list[7].first, std::string("border-top-width")));
        expect(eq(list[8].first, std::string("border-top-style")));
        expect(eq(list[9].first, std::string("border-top-color")));
    };
    "styles_delete_and_reinsert_preserves_source_object_order"_test = [] {
        styles::Styles style(u"a: 1; b: 2; c: 3");
        style.set(u"b", styles::StyleValue(u""));
        style.set(u"b", styles::StyleValue(u"4"));
        const auto list = style.styleList();
        expect(eq(list.size(), std::size_t{3}));
        expect(eq(list[0].first, std::string("a")));
        expect(eq(list[1].first, std::string("c")));
        expect(eq(list[2].first, std::string("b")));
    };
    "styles_set_propagates_and_recombines_border_children"_test = [] {
        styles::Styles style;
        style.set(u"borderTop", styles::StyleValue(std::u16string(u"1px solid red")));
        style.set(u"borderRight", styles::StyleValue(std::u16string(u"1px solid red")));
        style.set(u"borderBottom", styles::StyleValue(std::u16string(u"1px solid red")));
        style.set(u"borderLeft", styles::StyleValue(std::u16string(u"1px solid red")));
        expect(same16(style.get(u"border"), u"1px solid red"));
        style.set(u"borderLeftColor", styles::StyleValue(std::u16string{}));
        expect(style.get(u"border").empty());
    };
    "styles_sanitize_and_normalize_utf16_names"_test = [] {
        StyleProbe style;
        expect(same16(style.sanitizeValue(u"red; blue"), u"red"));
        expect(same16(style.sanitizeValue(u"\"unclosed"), u"\"unclosed\""));
        expect(eq(style.normalizeName(u"x😀Y"), std::string("x😀-y")));
        std::u16string loneName{u'x', static_cast<char16_t>(0xD800), u'Y'};
        std::string wtf8Name{"x"};
        wtf8Name.append("\xED\xA0\x80", 3);
        wtf8Name += "-y";
        expect(eq(style.normalizeName(loneName), wtf8Name));
        styles::Styles roundTrip;
        roundTrip.set(loneName, styles::StyleValue(u"kept"));
        expect(same16(roundTrip.get(loneName), u"kept"));
        std::u16string normalizedName{u'x', static_cast<char16_t>(0xD800), u'-', u'y'};
        expect(same16(roundTrip.cssText(), normalizedName + u": kept;"));
        expect(eq(style.parentName("border-top-width"), std::string("border-top")));
        expect(eq(style.childName("border-width", "top"), std::string("border-top-width")));
    };
    "styles_style_list_is_a_detached_copy_and_primitives_convert"_test = [] {
        styles::Styles style(u"color: red");
        auto detached = style.styleList();
        detached[0].second = u"blue";
        detached.emplace_back("extra", u"ignored");
        expect(same16(style.get(u"color"), u"red"));
        expect(style.get(u"extra").empty());
        expect(eq(style.styleList().size(), std::size_t{1}));

        style.set(u"opacity", styles::StyleValue(2.0));
        style.set(u"display", styles::StyleValue(false));
        expect(same16(style.get(u"opacity"), u"2"));
        expect(same16(style.get(u"display"), u"false"));
        expect(same16(style.cssText(), u"color: red; opacity: 2; display: false;"));
    };
    "styles_derived_constructor_uses_class_pattern_before_parse"_test = [] {
        DerivedStyles style(u"padding=1em 2em;color=red;");
        expect(same16(style.get(u"padding"), u"1em 2em"));
        expect(same16(style.get(u"color"), u"red"));
    };

    "source_css_parsing_and_css_text_cases"_test = [] {
        struct Case { std::u16string_view css; std::u16string_view key; std::u16string_view value; std::u16string_view output; };
        const std::array cases{
            Case{u"", u"color", u"", u""},
            Case{u"font-size: 150%", u"font-size", u"150%", u"font-size: 150%;"},
            Case{u"abc-def: 0", u"abc-def", u"0", u"abc-def: 0;"},
            Case{u"a: 0; b: 1px", u"b", u"1px", u"a: 0; b: 1px;"},
            Case{u"a: 0; /*b: 1px*/ c: bold", u"c", u"bold", u"a: 0; c: bold;"},
            Case{u"abc: ; xyz: 0", u"xyz", u"0", u"xyz: 0;"},
            Case{u"abc xyz: 1px", u"abc", u"", u""},
            Case{u"abc: xy 'pqr", u"abc", u"xy 'pqr'", u"abc: xy 'pqr';"},
            Case{u"abc: ';", u"abc", u"';'", u"abc: ';';"},
            Case{u"background: red; background-clip: none", u"background-clip", u"none", u"background: red; background-clip: none;"},
        };
        for (const auto& test : cases) {
            styles::Styles style(test.css);
            expect(same16(style.get(test.key), test.value));
            expect(same16(style.cssText(), test.output));
        }
    };
    "source_css_margin_border_and_font_shorthands"_test = [] {
        styles::Styles margin(u"margin: 3px; margin-right: 1px");
        expect(same16(margin.get(u"margin"), u"3px 1px 3px 3px"));
        expect(same16(margin.get(u"margin-bottom"), u"3px"));
        expect(same16(margin.cssText(), u"margin: 3px 1px 3px 3px;"));

        styles::Styles border(u"border: 3px solid red; border-top-color: blue");
        expect(same16(border.get(u"border-top"), u"3px solid blue"));
        expect(same16(border.get(u"border-right"), u"3px solid red"));
        expect(same16(border.get(u"border-top-width"), u"3px"));
        expect(same16(border.get(u"border-top-style"), u"solid"));
        expect(same16(border.get(u"border-top-color"), u"blue"));
        expect(same16(border.cssText(), u"border-top: 3px solid blue; border-right: 3px solid red; border-bottom: 3px solid red; border-left: 3px solid red;"));
        const std::array<std::string_view, 16> borderKeys{
            "border-top", "border-top-width", "border-top-style", "border-top-color",
            "border-right", "border-right-width", "border-right-style", "border-right-color",
            "border-bottom", "border-bottom-width", "border-bottom-style", "border-bottom-color",
            "border-left", "border-left-width", "border-left-style", "border-left-color"};
        const auto borderList = border.styleList();
        expect(eq(borderList.size(), borderKeys.size()));
        for (std::size_t i = 0; i < std::min(borderList.size(), borderKeys.size()); ++i)
            expect(eq(std::string_view(borderList[i].first), borderKeys[i]));

        styles::Styles font(u"font: arial bold italic 120%");
        expect(same16(font.get(u"font-family"), u"arial"));
        expect(same16(font.get(u"font-weight"), u"bold"));
        expect(same16(font.get(u"font-style"), u"italic"));
        expect(same16(font.get(u"font-size"), u"120%"));
        expect(same16(font.cssText(), u"font-style: italic; font-weight: bold; font-size: 120%; font-family: arial;"));
        styles::Styles lineHeight(u"font: arial small/12px");
        expect(same16(lineHeight.get(u"font-size"), u"small"));
        expect(same16(lineHeight.get(u"line-height"), u"12px"));
        expect(same16(lineHeight.cssText(), u"line-height: 12px; font-size: small; font-family: arial;"));
    };
    "source_css_get_and_set_semantics"_test = [] {
        styles::Styles style(u"padding: 0; padding-left: 1px");
        expect(same16(style.get(u"padding"), u"0 0 0 1px"));
        expect(same16(style.get(u"padding-top"), u"0"));
        expect(same16(style.get(u"padding-left"), u"1px"));
        style.set(u"padding-left", styles::StyleValue(u"3px"));
        expect(same16(style.get(u"padding"), u"0 0 0 3px"));
        style.set(u"padding", styles::StyleValue(u""));
        expect(style.get(u"padding-left").empty());
        expect(style.cssText().empty());
    };

};
