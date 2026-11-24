#include "support/folia_test.hpp"
import folia.math.input.tex.environment;

using namespace boost::ut;
using folia::math::tex::ParserEnvironmentSeed;
using folia::math::tex::ResolvedParserEnvironmentSeed;
using folia::math::tex::TexEnvironment;
using folia::math::tex::overlayEnvironment;
using folia::math::tex::resolveParserEnvironmentSeed;

namespace {
TexEnvironment fullEnvironment() {
    TexEnvironment value;
    value.display = true;
    value.in_root = true;
    value.operator_letters = true;
    value.multi_letter_identifiers = std::make_shared<folia::math::regexp_host::RegExp>(u"^[a-z]+");
    value.no_auto_op = true;
    value.is_global = true;
    value.bold_symbol = true;
    value.vector_star = true;
    value.script_level = 3.5;
    value.font = u"font";
    value.italic_font = u"italic";
    value.style = u"style";
    value.size = 2.5;
    value.color = u"color";
    value.horizontal_size = u"horizontal";
    value.closing_environment = u"closing";
    value.cd_min_width = u"width";
    value.cd_min_height = u"height";
    value.vector_font = u"vector";
    value.math_variant = u"variant";
    value.math_size = u"math-size";
    value.math_color = u"math-color";
    return value;
}
}

namespace {
ResolvedParserEnvironmentSeed makeOwnedSeedResult() {
    ParserEnvironmentSeed local;
    local.environment.font = u"owned";
    local.environment.display = false;
    local.is_inner = false;
    return resolveParserEnvironmentSeed(std::move(local));
}
}

suite math_native_raii_tex_environment = [] {
    "overlay copies every present typed field"_test = [] {
        const TexEnvironment source = fullEnvironment();
        TexEnvironment destination;
        overlayEnvironment(destination, source);
        expect(destination == source);

        // An absent source field leaves the destination's explicit value intact.
        const TexEnvironment empty;
        overlayEnvironment(destination, empty);
        expect(destination == source);
    };

    "overlay preserves false zero and empty values"_test = [] {
        TexEnvironment destination = fullEnvironment();
        TexEnvironment source;
        source.display = false;
        source.in_root = false;
        source.operator_letters = false;
        source.multi_letter_identifiers = folia::math::regexp_host::RegExpRef{};
        source.no_auto_op = false;
        source.is_global = false;
        source.bold_symbol = false;
        source.vector_star = false;
        source.script_level = 0.0;
        source.font = u"";
        source.italic_font = u"";
        source.style = u"";
        source.size = 0.0;
        source.color = u"";
        source.horizontal_size = u"";
        source.closing_environment = u"";
        source.cd_min_width = u"";
        source.cd_min_height = u"";
        source.vector_font = u"";
        source.math_variant = u"";
        source.math_size = u"";
        source.math_color = u"";

        overlayEnvironment(destination, source);
        expect(destination == source);
        expect(destination.display.has_value());
        expect(!*destination.display);
        expect(destination.script_level.has_value());
        expect(*destination.script_level == 0.0);
        expect(destination.font.has_value());
        expect(destination.font->empty());
        expect(destination.color.has_value());
        expect(destination.color->empty());
    };

    "partial overlay changes only supplied fields"_test = [] {
        TexEnvironment destination = fullEnvironment();
        TexEnvironment expected = destination;
        expected.display = false;
        expected.script_level = 0.0;
        expected.font = u"";
        TexEnvironment source;
        source.display = false;
        source.script_level = 0.0;
        source.font = u"";

        overlayEnvironment(destination, source);
        expect(destination == expected);
    };

    "ordinary environment copies own independent optional strings"_test = [] {
        TexEnvironment original = fullEnvironment();
        TexEnvironment copied = original;
        copied.font = u"changed";
        copied.display = false;
        copied.color->append(u"-child");

        expect(original.font == std::optional<std::u16string>(u"font"));
        expect(original.display == std::optional<bool>(true));
        expect(original.color == std::optional<std::u16string>(u"color"));
        expect(copied.color == std::optional<std::u16string>(u"color-child"));
    };

    "seed resolution owns the environment and defaults inner to true"_test = [] {
        ResolvedParserEnvironmentSeed defaulted =
            resolveParserEnvironmentSeed(ParserEnvironmentSeed{});
        expect(defaulted.is_inner);
        expect(defaulted.environment == TexEnvironment{});

        ResolvedParserEnvironmentSeed resolved = makeOwnedSeedResult();
        expect(!resolved.is_inner);
        expect(resolved.environment.font == std::optional<std::u16string>(u"owned"));
        expect(resolved.environment.display == std::optional<bool>(false));
        resolved.environment.font->append(u"-mutated");
        expect(resolved.environment.font == std::optional<std::u16string>(u"owned-mutated"));
    };

    "explicit true seed remains true"_test = [] {
        ParserEnvironmentSeed input;
        input.environment.script_level = 0.0;
        input.is_inner = true;
        const auto result = resolveParserEnvironmentSeed(std::move(input));
        expect(result.is_inner);
        expect(result.environment.script_level == std::optional<double>(0.0));
    };
};
