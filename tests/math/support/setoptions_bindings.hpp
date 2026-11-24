#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::setoptions {
using namespace folia::math::tex;
using namespace std::literals;

enum class OptionKind { boolean, string, budget, regexp, complex };
template <auto Head, auto... Tail, class Owner> auto &optionSlot(Owner &owner) {
    auto &value = owner.*Head;
    if constexpr (sizeof...(Tail) == 0)
        return value;
    else {
        if (!value)
            value.emplace();
        return optionSlot<Tail...>(*value);
    }
}
struct OptionCase {
    std::u16string_view package, key;
    OptionKind kind;
    std::u16string_view input;
    void (*seed)(TexOptionValues &);
    bool (*matches)(TexOptionValues &);
};
template <auto... Members>
OptionCase optionCase(std::u16string_view package, std::u16string_view key, OptionKind kind,
                      std::u16string_view input) {
    return {
        package,
        key,
        kind,
        input,
        +[](TexOptionValues &options) {
            auto &value = optionSlot<Members...>(options);
            if (!value)
                value.emplace();
        },
        +[](TexOptionValues &options) {
            auto &value = optionSlot<Members...>(options);
            if (!value)
                return false;
            using T = typename std::remove_reference_t<decltype(value)>::value_type;
            if constexpr (std::is_same_v<T, bool>)
                return !*value;
            else if constexpr (std::is_same_v<T, std::variant<bool, std::u16string>>)
                return std::holds_alternative<bool>(*value) && !std::get<bool>(*value);
            else if constexpr (std::is_same_v<T, std::u16string>)
                return std::ranges::equal(*value, u"set-value"sv);
            else if constexpr (std::is_same_v<T, double>)
                return *value == 2048;
            else if constexpr (std::is_same_v<T, folia::math::regexp_host::RegExpRef>)
                return *value && std::ranges::equal((*value)->source(), u"^Q"sv);
            else if constexpr (std::is_same_v<T, std::variant<std::u16string,
                                                              folia::math::regexp_host::RegExpRef>>)
                return std::holds_alternative<std::u16string>(*value) &&
                       std::ranges::equal(std::get<std::u16string>(*value), u"set-value"sv);
            else
                return false;
        }};
}

struct OptionInput {
    std::string binding;
    std::u16string package, key;
    OptionKind kind;
    std::u16string input;
};

inline const auto& optionInputs() {
    using namespace math_test::fixtures;
    static const auto result = records<OptionInput>(
        "math/fixtures/packages/setoptions/fields.tsv", 5, 96, [](const auto& r) {
            const std::array names{"boolean", "string", "budget", "regexp", "complex"};
            const auto found = std::ranges::find(names, r[3]);
            if (found == names.end()) throw std::runtime_error("Unknown setoptions reference kind");
            return OptionInput{bytes(r[0]), folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2]), static_cast<OptionKind>(found - names.begin()),
                folia::test::data::decode_utf16(r[4])};
        });
    return result;
}

// Native member pointers are executable bindings, not serialized fixture values.
// Keep every original template member sequence; reject an unknown binding.
inline OptionCase bindOption(const OptionInput& r) {
    if (r.binding == "TexOptionValues::preFilters")
        return optionCase<&TexOptionValues::preFilters>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::postFilters")
        return optionCase<&TexOptionValues::postFilters>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::findTeX")
        return optionCase<&TexOptionValues::findTeX>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::packages")
        return optionCase<&TexOptionValues::packages>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::maxBuffer")
        return optionCase<&TexOptionValues::maxBuffer>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::maxTemplateSubtitutions")
        return optionCase<&TexOptionValues::maxTemplateSubtitutions>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathStyle")
        return optionCase<&TexOptionValues::mathStyle>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::formatError")
        return optionCase<&TexOptionValues::formatError>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::tags")
        return optionCase<&TexOptionValues::tags>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::tagSide")
        return optionCase<&TexOptionValues::tagSide>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::tagIndent")
        return optionCase<&TexOptionValues::tagIndent>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::useLabelIds")
        return optionCase<&TexOptionValues::useLabelIds>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::ignoreDuplicateLabels")
        return optionCase<&TexOptionValues::ignoreDuplicateLabels>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::tagAlign")
        return optionCase<&TexOptionValues::tagAlign>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::textmacros")
        return optionCase<&TexOptionValues::textmacros>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::textmacros/TextMacrosOptions::packages")
        return optionCase<&TexOptionValues::textmacros, &TextMacrosOptions::packages>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::setoptions")
        return optionCase<&TexOptionValues::setoptions>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::setoptions/SetOptionsOptions::filterPackage")
        return optionCase<&TexOptionValues::setoptions, &SetOptionsOptions::filterPackage>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::setoptions/SetOptionsOptions::filterOption")
        return optionCase<&TexOptionValues::setoptions, &SetOptionsOptions::filterOption>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::setoptions/SetOptionsOptions::filterValue")
        return optionCase<&TexOptionValues::setoptions, &SetOptionsOptions::filterValue>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::setoptions/SetOptionsOptions::allowPackageDefault")
        return optionCase<&TexOptionValues::setoptions, &SetOptionsOptions::allowPackageDefault>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::setoptions/SetOptionsOptions::allowOptionsDefault")
        return optionCase<&TexOptionValues::setoptions, &SetOptionsOptions::allowOptionsDefault>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::setoptions/SetOptionsOptions::allowOptions")
        return optionCase<&TexOptionValues::setoptions, &SetOptionsOptions::allowOptions>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::noundefined")
        return optionCase<&TexOptionValues::noundefined>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::noundefined/NoUndefinedOptions::color")
        return optionCase<&TexOptionValues::noundefined, &NoUndefinedOptions::color>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::noundefined/NoUndefinedOptions::background")
        return optionCase<&TexOptionValues::noundefined, &NoUndefinedOptions::background>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::noundefined/NoUndefinedOptions::size")
        return optionCase<&TexOptionValues::noundefined, &NoUndefinedOptions::size>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::bboldx")
        return optionCase<&TexOptionValues::bboldx>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::bboldx/BboldxOptions::bfbb")
        return optionCase<&TexOptionValues::bboldx, &BboldxOptions::bfbb>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::bboldx/BboldxOptions::light")
        return optionCase<&TexOptionValues::bboldx, &BboldxOptions::light>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::units")
        return optionCase<&TexOptionValues::units>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::units/UnitsOptions::loose")
        return optionCase<&TexOptionValues::units, &UnitsOptions::loose>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::units/UnitsOptions::ugly")
        return optionCase<&TexOptionValues::units, &UnitsOptions::ugly>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::allowTexHTML")
        return optionCase<&TexOptionValues::allowTexHTML>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::tagformat")
        return optionCase<&TexOptionValues::tagformat>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::tagformat/TagFormatOptions::number")
        return optionCase<&TexOptionValues::tagformat, &TagFormatOptions::number>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::tagformat/TagFormatOptions::tag")
        return optionCase<&TexOptionValues::tagformat, &TagFormatOptions::tag>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::tagformat/TagFormatOptions::ref")
        return optionCase<&TexOptionValues::tagformat, &TagFormatOptions::ref>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::tagformat/TagFormatOptions::id")
        return optionCase<&TexOptionValues::tagformat, &TagFormatOptions::id>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::tagformat/TagFormatOptions::url")
        return optionCase<&TexOptionValues::tagformat, &TagFormatOptions::url>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::physics")
        return optionCase<&TexOptionValues::physics>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::physics/PhysicsOptions::italicdiff")
        return optionCase<&TexOptionValues::physics, &PhysicsOptions::italicdiff>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::physics/PhysicsOptions::arrowdel")
        return optionCase<&TexOptionValues::physics, &PhysicsOptions::arrowdel>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::maxMacros")
        return optionCase<&TexOptionValues::maxMacros>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::protectedMacros")
        return optionCase<&TexOptionValues::protectedMacros>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools")
        return optionCase<&TexOptionValues::mathtools>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::multlinedGap")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::multlinedGap>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::multlinedPos")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::multlinedPos>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::multlinedWidth")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::multlinedWidth>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::firstlineAfterskip")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::firstlineAfterskip>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::lastlinePreskip")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::lastlinePreskip>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::smallmatrixAlign")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::smallmatrixAlign>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::shortvdotsAdjustAbove")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::shortvdotsAdjustAbove>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::shortvdotsAdjustBelow")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::shortvdotsAdjustBelow>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::centercolon")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::centercolon>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::centercolonOffset")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::centercolonOffset>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::thincolonDx")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::thincolonDx>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::thincolonDw")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::thincolonDw>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::useUnicode")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::useUnicode>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::legacycolonsymbols")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::legacycolonsymbols>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::prescriptSubFormat")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::prescriptSubFormat>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::prescriptSupFormat")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::prescriptSupFormat>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::prescriptArgFormat")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::prescriptArgFormat>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::allowMathtoolsset")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::allowMathtoolsset>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::pairedDelimiters")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::pairedDelimiters>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::mathtools/MathtoolsOptions::tagforms")
        return optionCase<&TexOptionValues::mathtools, &MathtoolsOptions::tagforms>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::dsfont")
        return optionCase<&TexOptionValues::dsfont>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::dsfont/DsfontOptions::sans")
        return optionCase<&TexOptionValues::dsfont, &DsfontOptions::sans>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::active")
        return optionCase<&TexOptionValues::active>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::macros")
        return optionCase<&TexOptionValues::macros>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::environments")
        return optionCase<&TexOptionValues::environments>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::color")
        return optionCase<&TexOptionValues::color>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::color/ColorOptions::padding")
        return optionCase<&TexOptionValues::color, &ColorOptions::padding>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::color/ColorOptions::borderWidth")
        return optionCase<&TexOptionValues::color, &ColorOptions::borderWidth>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::begingroup")
        return optionCase<&TexOptionValues::begingroup>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::begingroup/BegingroupOptions::allowGlobal")
        return optionCase<&TexOptionValues::begingroup, &BegingroupOptions::allowGlobal>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::bbm")
        return optionCase<&TexOptionValues::bbm>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::bbm/BbmOptions::bold")
        return optionCase<&TexOptionValues::bbm, &BbmOptions::bold>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::amscd")
        return optionCase<&TexOptionValues::amscd>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::amscd/AmscdOptions::colspace")
        return optionCase<&TexOptionValues::amscd, &AmscdOptions::colspace>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::amscd/AmscdOptions::rowspace")
        return optionCase<&TexOptionValues::amscd, &AmscdOptions::rowspace>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::amscd/AmscdOptions::harrowsize")
        return optionCase<&TexOptionValues::amscd, &AmscdOptions::harrowsize>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::amscd/AmscdOptions::varrowsize")
        return optionCase<&TexOptionValues::amscd, &AmscdOptions::varrowsize>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::amscd/AmscdOptions::hideHorizontalLabels")
        return optionCase<&TexOptionValues::amscd, &AmscdOptions::hideHorizontalLabels>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::multlineWidth")
        return optionCase<&TexOptionValues::multlineWidth>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::ams")
        return optionCase<&TexOptionValues::ams>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::ams/AmsOptions::operatornamePattern")
        return optionCase<&TexOptionValues::ams, &AmsOptions::operatornamePattern>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::ams/AmsOptions::multlineWidth")
        return optionCase<&TexOptionValues::ams, &AmsOptions::multlineWidth>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::ams/AmsOptions::multlineIndent")
        return optionCase<&TexOptionValues::ams, &AmsOptions::multlineIndent>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::digits")
        return optionCase<&TexOptionValues::digits>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::numberPattern")
        return optionCase<&TexOptionValues::numberPattern>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::initialDigit")
        return optionCase<&TexOptionValues::initialDigit>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::identifierPattern")
        return optionCase<&TexOptionValues::identifierPattern>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::initialLetter")
        return optionCase<&TexOptionValues::initialLetter>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::baseURL")
        return optionCase<&TexOptionValues::baseURL>(r.package, r.key, r.kind, r.input);
    if (r.binding == "TexOptionValues::internalMath")
        return optionCase<&TexOptionValues::internalMath>(r.package, r.key, r.kind, r.input);
    throw std::runtime_error("Unknown native setoptions field binding: " + r.binding);
}

inline const auto& optionCases() {
    static const auto result = [] {
        std::vector<OptionCase> values;
        for (const auto& input : optionInputs()) values.push_back(bindOption(input));
        return values;
    }();
    return result;
}

} // namespace math_test::setoptions
