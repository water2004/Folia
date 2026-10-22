#include "support/folia_test.hpp"
import folia.math.input.tex.parse_util;
import folia.math.util.js_numeric_input;

using namespace boost::ut;
using namespace folia::math::tex;
namespace {
using folia::math::mml::AttributeSet;
using folia::math::mml::AttributeValue;
namespace numeric = folia::math::js_input;
#include "fixtures/tex-source/input_host_numeric_values.inc"
void optionText(AttributeSet const &options, std::u16string_view key,
                std::u16string_view expected) {
    auto value = options.own(key);
    expect(value != nullptr);
    if (!value)
        return;
    auto text = std::get_if<std::u16string>(value);
    expect(text != nullptr);
    if (text)
        expect(std::ranges::equal(*text, expected));
}
void optionBool(AttributeSet const &options, std::u16string_view key, bool expected) {
    auto value = options.own(key);
    expect(value != nullptr);
    if (!value)
        return;
    auto boolean = std::get_if<bool>(value);
    expect(boolean && *boolean == expected);
}
void optionNumber(AttributeSet const &options, std::u16string_view key, double expected) {
    auto value = options.own(key);
    expect(value != nullptr);
    if (!value)
        return;
    auto number = std::get_if<double>(value);
    expect(number && *number == expected);
}
template <class Action> void optionError(Action action, std::string_view id) {
    try {
        action();
        expect(false);
    } catch (TexError const &error) {
        expect(error.id() == id);
    }
}
bool positive(std::u16string_view value) {
    return !value.empty() &&
           std::ranges::all_of(value, [](auto c) { return c >= u'0' && c <= u'9'; }) &&
           numeric::number(value) > 0;
}
AttributeValue twice(std::u16string_view value) {
    return 2 * numeric::number(value);
}
} // namespace

suite keyval_options = [] {
    "keyval options parse flags booleans repeated keys and empty keys without normalization"_test =
        [] {
            auto values =
                parse_util::keyvalOptions(u"flag,yes=true,no=false,flag=false,a=1,a=2,=empty");
            expect(values.ownEntries().size() == 5_u);
            optionBool(values, u"flag", false);
            optionBool(values, u"yes", true);
            optionBool(values, u"no", false);
            optionText(values, u"a", u"2");
            optionText(values, u"", u"empty");
            expect(parse_util::keyvalOptions(u"").ownEntries().empty());
            expect(parse_util::keyvalOptions(u"\t \n").ownEntries().empty());
            values = parse_util::keyvalOptions(u",,a=,b= ,,,, =,last");
            optionText(values, u"a", u"");
            optionText(values, u"b", u"");
            optionText(values, u"", u"");
            optionBool(values, u"last", true);
        };
    "keyval options nested outer braces first pass and l3keys preserve original stripping rules"_test =
        [] {
            for (auto source : {u"{a=1,b=2}", u" { a=1, b=2 } "}) {
                auto values = parse_util::keyvalOptions(source);
                optionText(values, u"a", u"1");
                optionText(values, u"b", u"2");
            }
            auto nested = parse_util::keyvalOptions(u"{{a=1,b=2}}");
            optionBool(nested, u"a=1,b=2", true);
            auto ordinary = parse_util::keyvalOptions(u"a={{{x}}},b={{ y }},c= true ");
            optionText(ordinary, u"a", u"x");
            optionText(ordinary, u"b", u" y ");
            optionBool(ordinary, u"c", true);
            auto l3 = parse_util::keyvalOptions(u"a={{{x}}},b={{ y }},c= true ", std::nullopt,
                                                false, true);
            optionText(l3, u"a", u"{{x}}");
            optionText(l3, u"b", u"{ y }");
            optionBool(l3, u"c", true);
            l3 = parse_util::keyvalOptions(u"{a={{x}},b={{y}}}", std::nullopt, false, true);
            optionText(l3, u"a", u"{x}");
            optionText(l3, u"b", u"{y}");
        };
    "keyval options keep nested punctuation quotes and escaped units character exact"_test = [] {
        auto values = parse_util::keyvalOptions(u" key = { a,b=c } , other={ {x} }");
        optionText(values, u"key", u" a,b=c ");
        optionText(values, u"other", u"x");
        values = parse_util::keyvalOptions(u"a={{x}},b={y} z,c=x{y},d={x}{y}");
        optionText(values, u"a", u"x");
        optionText(values, u"b", u"{y} z");
        optionText(values, u"c", u"x{y}");
        optionText(values, u"d", u"x}{y");
        values = parse_util::keyvalOptions(uR"(a=x\,y,b=x\=y,c=\{x\},d=\\)");
        optionText(values, u"a", uR"(x\,y)");
        optionText(values, u"b", uR"(x\=y)");
        optionText(values, u"c", uR"(\{x\})");
        optionText(values, u"d", uR"(\\)");
        values = parse_util::keyvalOptions(uR"(a={x\}y},b={{x\{y}})");
        optionText(values, u"a", uR"(x\}y)");
        optionText(values, u"b", uR"(x\{y)");
        values = parse_util::keyvalOptions(u"a=\"x,y\",b='x=y'");
        optionText(values, u"a", u"\"x");
        optionBool(values, u"y\"", true);
        optionText(values, u"b", u"'x=y'");
    };
    "keyval options preserve trailing backslashes and escaped whitespace while trimming other whitespace"_test =
        [] {
            auto values = parse_util::keyvalOptions(u"a=x\\");
            optionText(values, u"a", u"x\\");
            values = parse_util::keyvalOptions(u"tail\\");
            optionBool(values, u"tail\\", true);
            values = parse_util::keyvalOptions(u"a= x\\  ,b= x\\\\  ,c= \t y \t");
            optionText(values, u"a", u"x\\ ");
            optionText(values, u"b", u"x\\\\");
            optionText(values, u"c", u"y");
        };
    "keyval options allowed empty and absent filters retain conversions and precise errors"_test =
        [] {
            const std::array<std::u16string_view, 2> directions{u"up", u"down"};
            const std::array<KeyValueOption, 7> allowed{{{u"free", std::nullopt},
                                                         {u"flag", KeyValueTypes::boolean},
                                                         {u"n", KeyValueTypes::number},
                                                         {u"i", KeyValueTypes::integer},
                                                         {u"s", KeyValueTypes::string},
                                                         {u"d", KeyValueTypes::dimen},
                                                         {u"dir", KeyValueDef::oneof(directions)}}};
            auto filter = std::span<KeyValueOption const>(allowed);
            auto values = parse_util::keyvalOptions(
                u"flag,n=-.5e+2,i=+012,s=false,d=1em,dir=up,free=true,dropped=x", filter);
            optionBool(values, u"flag", true);
            optionNumber(values, u"n", -50);
            optionNumber(values, u"i", 12);
            optionText(values, u"s", u"false");
            optionText(values, u"d", u"1em");
            optionText(values, u"dir", u"up");
            optionBool(values, u"free", true);
            expect(values.own(u"dropped") == nullptr);
            expect(parse_util::keyvalOptions(u"x=1,y", std::span<KeyValueOption const>{})
                       .ownEntries()
                       .empty());
            optionText(parse_util::keyvalOptions(u"x=1"), u"x", u"1");
            optionError(
                [] {
                    parse_util::keyvalOptions(u"unknown=1", std::span<KeyValueOption const>{},
                                              true);
                },
                "InvalidOption");
            for (auto source : {u"flag=yes", u"n=1E3", u"i=1.2", u"d=no", u"dir=left"})
                optionError([&] { parse_util::keyvalOptions(source, filter); }, "InvalidValue");
            for (auto const &[source, id] :
                 {std::pair{u"a=}", "ExtraCloseMissingOpen"},
                  std::pair{u"a={x", "ExtraOpenMissingClose"},
                  std::pair{u"bad=x,good={missing", "ExtraOpenMissingClose"}})
                optionError(
                    [&] {
                        parse_util::keyvalOptions(source, std::span<KeyValueOption const>{}, true);
                    },
                    id);
        };
    "keyval options all five standard validators keep exact case numeric grammar and dimension rules"_test =
        [] {
            for (auto value : {u"true", u"false"})
                expect(KeyValueTypes::boolean.verify(value));
            for (auto value : {u"", u"True", u"1"})
                expect(!KeyValueTypes::boolean.verify(value));
            for (auto value : {u"0", u"-0", u"+1", u".5", u"1.", u"-1.25e+2"})
                expect(KeyValueTypes::number.verify(value));
            for (auto value : {u"", u"Infinity", u"NaN", u"1E3", u"0x10", u" 1", u"1\n"})
                expect(!KeyValueTypes::number.verify(value));
            for (auto value : {u"0", u"-0", u"+12", u"0012"})
                expect(KeyValueTypes::integer.verify(value));
            for (auto value : {u"", u"1.0", u"1e2", u" 1"})
                expect(!KeyValueTypes::integer.verify(value));
            for (auto value : {u"", u"true", u"{x}", u"x\\"})
                expect(KeyValueTypes::string.verify(value));
            for (auto value : {u"1em", u"-.5pt"})
                expect(KeyValueTypes::dimen.verify(value));
            for (auto value : {u"", u"1", u"no", u"1emx", u"50%"})
                expect(!KeyValueTypes::dimen.verify(value));
            expect(!std::get<bool>(KeyValueTypes::boolean.convert(u"yes")));
            expect(std::get<double>(KeyValueTypes::integer.convert(u"0xF")) == 15.0);
            expect(std::get<double>(KeyValueTypes::integer.convert(u"2e3")) == 2.0);
            expect(
                std::ranges::equal(std::get<std::u16string>(KeyValueTypes::string.convert(u" x ")),
                                   std::u16string_view(u" x ")));
            expect(
                std::ranges::equal(std::get<std::u16string>(KeyValueTypes::dimen.convert(u" 1em ")),
                                   std::u16string_view(u" 1em ")));
        };
    "keyval options number conversion exposes original parseFloat prefixes Infinity NaN and negative zero"_test =
        [] {
            struct Example {
                std::u16string_view input;
                double result;
            };
            for (auto const &example :
                 {Example{u"1e+", 1}, Example{u"2E3x", 2000},
                  Example{u" Infinitystuff", std::numeric_limits<double>::infinity()},
                  Example{u"\uFEFF -Infinityfoo", -std::numeric_limits<double>::infinity()},
                  Example{u"0xF", 0}, Example{u".5tail", .5}, Example{u"1e-999", 0},
                  Example{u"1e999", std::numeric_limits<double>::infinity()}})
                expect(std::get<double>(KeyValueTypes::number.convert(example.input)) ==
                       example.result);
            for (auto value : {u"", u"hello", u".", u"+"})
                expect(std::isnan(std::get<double>(KeyValueTypes::number.convert(value))));
            auto zero = std::get<double>(KeyValueTypes::number.convert(u"-0"));
            expect(zero == 0.0 && std::signbit(zero));
        };
    "input_host_source_binary64_number_and_parseInt_values"_test = [] {
        const auto matches = [](double actual, std::uint64_t bits) {
            const auto expected = std::bit_cast<double>(bits);
            return std::isnan(expected) ? std::isnan(actual)
                 : std::bit_cast<std::uint64_t>(actual) == bits;
        };
        for (const auto& row : source_number_cases)
            expect(matches(numeric::number(row.input), row.bits));
        for (const auto& row : source_parse_int_cases)
            expect(matches(numeric::parseInt(row.input, row.radix), row.bits));
    };
    "keyval options oneof owns choices including empty set and custom types use compiled functions"_test =
        [] {
            std::u16string first = u"up";
            const std::array<std::u16string_view, 2> choices{first, u"down"};
            auto type = KeyValueDef::oneof(choices);
            first = u"left";
            expect(type.verify(u"up"));
            expect(!type.verify(u"left"));
            expect(std::ranges::equal(type.name, std::u16string_view(u"string")));
            expect(std::ranges::equal(std::get<std::u16string>(type.convert(u"anything")),
                                      std::u16string_view(u"anything")));
            auto none = KeyValueDef::oneof({});
            expect(!none.verify(u""));
            const std::array<KeyValueOption, 1> emptyAllowed{{{u"dir", none}}};
            optionError(
                [&] {
                    parse_util::keyvalOptions(u"dir=up",
                                              std::span<KeyValueOption const>(emptyAllowed));
                },
                "InvalidValue");
            const std::array<KeyValueOption, 1> custom{
                {{u"x", KeyValueDef(u"positive", &positive, &twice)}}};
            optionNumber(parse_util::keyvalOptions(u"x=3", std::span<KeyValueOption const>(custom)),
                         u"x", 6);
            optionError(
                [&] { parse_util::keyvalOptions(u"x=0", std::span<KeyValueOption const>(custom)); },
                "InvalidValue");
        };
};
