#include "math/support/tex_fixture.hpp"
#include "math/support/mhchem_fixtures.hpp"
import folia.math.input.tex.ams.configuration;
import folia.math.input.tex.ams.methods;
import folia.math.input.tex.base_configuration;
import folia.math.input.tex.base_methods;
import folia.math.input.tex.mhchem.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.newcommand.methods;
import folia.math.core.mml_tree.node_kind;
import folia.math.util.js_number;
import folia.math.util.utf16;

namespace {
using namespace std::literals;

constexpr std::array<MacroHandler, 4> mhchemCallbacks{
    &MhchemMethods::Machine, MhchemMethods::Macro, MhchemMethods::xArrow,
    &NewcommandMethods::Let};
constexpr std::array<TokenInputHandler, 1> mhchemTokenCallbacks{&MhchemUtils::relmo};

using math_test::fixtures::mhchemCases;
using math_test::fixtures::MhchemCase;
using math_test::fixtures::mhchemReplacementCases;
using math_test::fixtures::MhchemReplacementCase;
using math_test::fixtures::mhchemMutationInput;
using math_test::fixtures::mhchemMutationCases;
using math_test::fixtures::MhchemMutationCase;
using math_test::fixtures::mhchemUntypedMapValueMutations;
using math_test::fixtures::mhchemRetiredObjectApiMutations;
using math_test::fixtures::mhchemAliasCases;
using math_test::fixtures::MhchemAliasCase;
using math_test::fixtures::mhchemCreatorCase;
using math_test::fixtures::MhchemCreatorCase;
using math_test::fixtures::mhchemRecoveryCases;
using math_test::fixtures::MhchemRecoveryCase;

struct MhchemError {
    std::string id;
    std::u16string message;
    std::string name;
};
thread_local MhchemError *mhchemError{};

MmlNode &formatMhchemError(TeX &jax, TexError const &error) {
    if (mhchemError) {
        mhchemError->id.assign(error.id());
        mhchemError->message = error.message();
        mhchemError->name = "TexError";
    }
    return jax.formatError(error);
}

MmlNode *findToken(MmlNode &node, NodeKind kind, std::u16string_view text) {
    if (node.kind() == kind && NodeUtil::getText(node) == text)
        return &node;
    for (auto *child : node.children())
        if (auto *found = findToken(*child, kind, text))
            return found;
    return nullptr;
}

std::u16string decimal(std::size_t value) {
    const auto text = std::to_string(value);
    return {text.begin(), text.end()};
}

std::u16string decimal(int value) {
    const auto text = std::to_string(value);
    return {text.begin(), text.end()};
}

void add(std::u16string &output, std::u16string_view value) {
    output += decimal(value.size());
    output += u":";
    output += value;
}

template <class Value>
void addProperty(std::u16string &output, std::u16string_view name, std::u16string_view type,
                 Value const &value) {
    add(output, name);
    add(output, type);
    add(output, value);
}

std::u16string snapshot(MmlNode const &root) {
    std::u16string output;
    const auto visit = [&](auto &&self, MmlNode const *node) -> void {
        if (!node) {
            add(output, u"null");
            return;
        }
        const auto kind = nodeKindName(node->kind());
        add(output, std::u16string(kind.begin(), kind.end()));

        auto attributes = node->attributes().ownEntries();
        std::ranges::sort(attributes, {}, &AttributeSet::Entry::first);
        add(output, decimal(attributes.size()));
        for (const auto &[key, value] : attributes) {
            add(output, key);
            std::visit(
                [&](auto const &scalar) {
                    using T = std::remove_cvref_t<decltype(scalar)>;
                    if constexpr (std::is_same_v<T, AttributeNull>) {
                        add(output, u"null");
                        add(output, u"");
                    } else if constexpr (std::is_same_v<T, bool>) {
                        add(output, u"boolean");
                        add(output, scalar ? u"true" : u"false");
                    } else if constexpr (std::is_same_v<T, double>) {
                        const auto number = folia::math::js_number::toString(scalar);
                        add(output, u"number");
                        add(output, std::u16string(number.begin(), number.end()));
                    } else {
                        add(output, u"string");
                        add(output, scalar);
                    }
                },
                value);
        }

        const auto &tex = node->properties().tex;
        std::size_t properties{};
        properties += tex.explicit_tex_class.has_value();
        properties += tex.unicode.has_value();
        properties += tex.auto_operator.has_value();
        properties += tex.function_operator.has_value();
        properties += tex.fix_stretchy.has_value();
        properties += tex.ignore_variant.has_value();
        properties += tex.no_dictionary_definition.has_value();
        properties += tex.prime_style.has_value();
        properties += tex.in_lists.has_value();
        properties += tex.script_position.has_value();
        properties += tex.open.has_value();
        properties += tex.close.has_value();
        properties += tex.math_accent.has_value();
        properties += tex.move_sup_sub.has_value();
        properties += tex.sub_sup_allowed.has_value();
        properties += tex.variant_form.has_value();
        add(output, decimal(properties));
        if (tex.explicit_tex_class) {
            addProperty(output, u"texClass", u"number",
                        decimal(static_cast<int>(*tex.explicit_tex_class)));
        }
        const auto addBool = [&](std::u16string_view name, std::optional<bool> value) {
            if (value)
                addProperty(output, name, u"boolean", *value ? u"true" : u"false");
        };
        addBool(u"unicode", tex.unicode);
        addBool(u"autoOP", tex.auto_operator);
        addBool(u"fnOP", tex.function_operator);
        addBool(u"fixStretchy", tex.fix_stretchy);
        addBool(u"ignore-variant", tex.ignore_variant);
        addBool(u"noDictDef", tex.no_dictionary_definition);
        addBool(u"texprimestyle", tex.prime_style);
        if (tex.in_lists) {
            std::u16string joined;
            for (const auto &item : *tex.in_lists) {
                if (!joined.empty())
                    joined += u",";
                joined += item;
            }
            addProperty(output, u"in-lists", u"string", joined);
        }
        if (tex.script_position)
            addProperty(output, u"sub-sup", u"string",
                        *tex.script_position == folia::math::mml::ScriptPosition::subscript
                            ? u"_"
                            : u"^");
        if (tex.open)
            addProperty(output, u"open", u"string", *tex.open);
        if (tex.close)
            addProperty(output, u"close", u"string", *tex.close);
        addBool(u"mathaccent", tex.math_accent);
        addBool(u"movesupsub", tex.move_sup_sub);
        addBool(u"subsupOK", tex.sub_sup_allowed);
        addBool(u"variantForm", tex.variant_form);

        add(output, node->isToken() || node->kind() == NodeKind::text
                        ? NodeUtil::getText(*node)
                        : u"");
        const auto children = node->children();
        add(output, decimal(children.size()));
        for (const auto *child : children)
            self(self, child);
    };
    visit(visit, &root);
    return output;
}

void reportSnapshotDifference(std::u16string_view name, std::u16string_view actual,
                             std::u16string_view expected) {
    std::size_t position{};
    while (position < actual.size() && position < expected.size() &&
           actual[position] == expected[position])
        ++position;
    if (position == actual.size() && position == expected.size())
        return;
    static std::size_t reports{};
    if (reports++ >= 8)
        return;
    const auto begin = position > 80 ? position - 80 : 0;
    const auto actualEnd = std::min(actual.size(), position + 80);
    const auto expectedEnd = std::min(expected.size(), position + 80);
    std::cerr << "Mhchem snapshot mismatch " << folia::math::utf16::encode(name)
              << " at UTF-16 offset " << position << " (actual size " << actual.size()
              << ", expected size " << expected.size() << ")\nactual:   "
              << folia::math::utf16::encode(actual.substr(begin, actualEnd - begin))
              << "\nexpected: "
              << folia::math::utf16::encode(expected.substr(begin, expectedEnd - begin))
              << '\n';
}

std::u16string attribute(AttributeSet const &attributes, std::u16string_view name) {
    const auto *value = attributes.own(name);
    if (!value)
        return {};
    if (const auto *text = std::get_if<std::u16string>(value))
        return *text;
    if (const auto *boolean = std::get_if<bool>(value))
        return *boolean ? u"true" : u"false";
    if (const auto *number = std::get_if<double>(value)) {
        const auto formatted = folia::math::js_number::toString(*number);
        return {formatted.begin(), formatted.end()};
    }
    return {};
}

struct MhchemFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{mhchemCallbacks, mhchemTokenCallbacks};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    MhchemError error;

    explicit MhchemFixture(bool ams, bool newcommand = false) {
        BasePackage::initialize(registry, maps);
        AmsConfiguration::initializeModule(registry, maps);
        NewcommandConfiguration::initializeModule(registry, maps);
        MhchemConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (newcommand)
            options.packages->push_back({u"newcommand", {}});
        if (ams)
            options.packages->push_back({u"ams", {}});
        options.packages->push_back({u"mhchem", {}});
        options.maxMacros = 1000;
        options.formatError = &formatMhchemError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, std::move(options));
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }

    MmlNode *compile(std::u16string_view source) {
        error = {};
        struct Restore {
            MhchemError *previous{mhchemError};
            ~Restore() { mhchemError = previous; }
        } restore;
        mhchemError = &error;
        try {
            folia::math::core::AbstractMathItem math(std::u16string(source), jax.get());
            math.setDisplay(true);
            return &jax->compile(math, document);
        } catch (TexError const &exception) {
            error.id.assign(exception.id());
            error.message = exception.message();
            error.name = "TexError";
            return nullptr;
        }
    }
};

std::string narrow(std::u16string_view value) {
    std::string output;
    output.reserve(value.size());
    for (const auto code : value) {
        if (code > 0x7f)
            throw std::logic_error("Expected ASCII error field");
        output.push_back(static_cast<char>(code));
    }
    return output;
}
} // namespace

suite mhchem = [] {
    "Mhchem package configuration retains all commands characters and callback aliases"_test = [] {
        ConfigurationHandler registry;
        MapHandler maps;
        MacroCallbacks callbacks{mhchemCallbacks, mhchemTokenCallbacks};
        const auto configuration = MhchemConfiguration::initializeModule(registry, maps);
        expect(configuration != nullptr);
        const auto commandMap = std::dynamic_pointer_cast<CommandMap>(maps.getMap(u"mhchem"));
        const auto characterMap =
            std::dynamic_pointer_cast<CharacterMap>(maps.getMap(u"mhchem-chars"));
        expect(commandMap != nullptr);
        expect(characterMap != nullptr);
        if (!commandMap || !characterMap)
            return;

        expect(MhchemMethods::Macro == &BaseMethods::Macro);
        expect(MhchemMethods::xArrow == &AmsMethods::xArrow);
        expect(callbacks.macroBinding(&MhchemMethods::Machine).value != 0_u);
        expect(callbacks.macroBinding(MhchemMethods::Macro).value != 0_u);
        expect(callbacks.macroBinding(MhchemMethods::xArrow).value != 0_u);
        expect(callbacks.tokenBinding(&MhchemUtils::relmo).value != 0_u);

        constexpr std::array<std::u16string_view, 9> commands{
            u"ce", u"pu", u"mhchemxrightarrow", u"mhchemxleftarrow",
            u"mhchemxleftrightarrow", u"mhchemxleftrightarrows",
            u"mhchemxrightleftharpoons", u"mhchemxRightleftharpoons",
            u"mhchemxLeftrightharpoons"};
        for (const auto name : commands) {
            const auto entry = commandMap->lookup(name);
            const auto *macro = std::get_if<Macro const *>(&entry);
            const auto *handler = macro && *macro ? std::get_if<MacroHandler>(&(*macro)->function)
                                                  : nullptr;
            expect(handler != nullptr) << folia::math::utf16::encode(name);
            if (name == u"ce" || name == u"pu")
                expect(handler && *handler == &MhchemMethods::Machine)
                    << folia::math::utf16::encode(name);
            else
                expect(handler && *handler == MhchemMethods::xArrow)
                    << folia::math::utf16::encode(name);
        }
        constexpr std::array<std::u16string_view, 14> characters{
            u"tripledash", u"mhchemBondTD", u"mhchemBondTDD", u"mhchemBondDTD",
            u"mhchemlongleftarrow", u"mhchemlongrightarrow", u"mhchemlongleftrightarrow",
            u"mhchemlongrightleftharpoons", u"mhchemlongRightleftharpoons",
            u"mhchemlongLeftrightharpoons", u"mhchemlongleftrightarrows",
            u"mhchemrightarrow", u"mhchemleftarrow", u"mhchemleftrightarrow"};
        for (const auto name : characters)
            expect(std::holds_alternative<Token const *>(characterMap->lookup(name)))
                << folia::math::utf16::encode(name);
        const auto parser = characterMap->parser();
        const auto *relmo = parser ? std::get_if<TokenInputHandler>(&*parser) : nullptr;
        expect(relmo && *relmo == &MhchemUtils::relmo);

        const auto rules = mhchemReplacements();
        expect(rules.size() == 6_u);
        if (rules.size() == 6) {
            for (std::size_t i = 0; i < rules.size(); ++i)
                expect(rules[i].kind == (i == 5 ? MhchemReplacementKind::arrow
                                                : MhchemReplacementKind::text));
            expect(rules[0].replacement == u"\\mhchemx$3[$1]{$2}");
            expect(rules[1].replacement == u"\\mhchemx$2{$1}");
            expect(rules[2].replacement == u"\\mhchemBondTD");
            expect(rules[3].replacement == u"\\mhchemBondTDD");
            expect(rules[4].replacement == u"\\mhchemBondDTD");
            expect(rules[5].replacement.empty());
        }
    };

    "Mhchem original Source MML and TexErrors match every package case"_test = [] {
        expect(std::size(mhchemCases()) == 220_u);
        for (const auto &entry : mhchemCases()) {
            MhchemFixture fixture(entry.ams);
            auto *tree = fixture.compile(entry.input);
            expect(tree != nullptr) << folia::math::utf16::encode(entry.name);
            if (tree) {
                const auto actual = snapshot(*tree);
                if (actual != entry.snapshot)
                    reportSnapshotDifference(entry.name, actual, entry.snapshot);
                expect(actual == entry.snapshot) << folia::math::utf16::encode(entry.name);
            }
            expect(fixture.error.name == narrow(entry.formattedErrorName))
                << folia::math::utf16::encode(entry.name);
            expect(fixture.error.id == narrow(entry.formattedErrorId))
                << folia::math::utf16::encode(entry.name);
            expect(fixture.error.message == entry.formattedErrorMessage)
                << folia::math::utf16::encode(entry.name);
            expect(fixture.jax->parseOptions().request().parsers.empty())
                << folia::math::utf16::encode(entry.name);
        }
    };

    "Mhchem six ordered replacements match every Source stage result"_test = [] {
        ConfigurationHandler registry;
        MapHandler maps;
        const auto config = MhchemConfiguration::initializeModule(registry, maps);
        const auto macros = maps.getMap(u"mhchem");
        const auto chars = maps.getMap(u"mhchem-chars");
        expect(config != nullptr);
        expect(macros != nullptr);
        expect(chars != nullptr);
        if (!macros || !chars)
            return;
        expect(std::size(mhchemReplacementCases()) == 63_u);
        const auto rules = mhchemReplacements();
        for (const auto &entry : mhchemReplacementCases()) {
            std::u16string stage(entry.input);
            for (std::size_t i = 0; i < rules.size(); ++i) {
                stage = applyMhchemReplacement(std::move(stage), rules[i], *macros, *chars);
                expect(stage == entry.stages[i])
                    << folia::math::utf16::encode(entry.name);
            }
            expect(stage == entry.result) << folia::math::utf16::encode(entry.name);
        }

        constexpr auto input = u"\\rightarrow+\\xrightarrow"sv;
        expect(applyMhchemReplacements(std::u16string(input), *macros, *chars) ==
               u"\\mhchemrightarrow+\\mhchemxrightarrow");

        const auto originalCommand = std::dynamic_pointer_cast<CommandMap>(macros);
        const auto originalCharacters = std::dynamic_pointer_cast<CharacterMap>(chars);
        expect(originalCommand != nullptr);
        expect(originalCharacters != nullptr);
        if (!originalCommand || !originalCharacters)
            return;
        const auto charValue = originalCharacters->lookup(u"mhchemrightarrow");
        const auto *charToken = std::get_if<Token const *>(&charValue);
        const auto macroValue = originalCommand->lookup(u"mhchemxrightarrow");
        const auto *macroToken = std::get_if<Macro const *>(&macroValue);
        expect(charToken && *charToken);
        expect(macroToken && *macroToken);
        if (!charToken || !*charToken || !macroToken || !*macroToken)
            return;
        const Token savedCharacter = **charToken;
        const Macro savedArrow = **macroToken;

        originalCharacters->remove(u"mhchemrightarrow");
        expect(applyMhchemReplacements(std::u16string(input), *originalCommand,
                                       *originalCharacters) ==
               u"\\rightarrow+\\mhchemxrightarrow");
        originalCharacters->add(savedCharacter);
        originalCommand->remove(u"mhchemxrightarrow");
        expect(applyMhchemReplacements(std::u16string(input), *originalCommand,
                                       *originalCharacters) ==
               u"\\mhchemrightarrow+\\xrightarrow");
        originalCommand->add(savedArrow);

        maps.registerMap(std::make_shared<CharacterMap>(
            CharacterMapInit{{u"mhchem-chars", std::nullopt}, {}}));
        maps.registerMap(std::make_shared<CommandMap>(
            CommandMapInit{MacroMapInit{{u"mhchem", std::nullopt}, {}}}));
        expect(applyMhchemReplacements(std::u16string(input), *originalCommand,
                                       *originalCharacters) ==
               u"\\mhchemrightarrow+\\mhchemxrightarrow");
        expect(applyMhchemReplacements(std::u16string(input), *maps.getMap(u"mhchem"),
                                       *maps.getMap(u"mhchem-chars")) == input);
    };

    "Mhchem uses real let aliases and preserves relmo creator overrides"_test = [] {
        expect(std::size(mhchemAliasCases()) == 2_u);
        for (const auto &entry : mhchemAliasCases()) {
            MhchemFixture fixture(true, true);
            auto *tree = fixture.compile(entry.input);
            expect(tree != nullptr);
            if (tree) {
                const auto actual = snapshot(*tree);
                if (actual != entry.snapshot)
                    reportSnapshotDifference(entry.input, actual, entry.snapshot);
                expect(actual == entry.snapshot)
                    << folia::math::utf16::encode(entry.input);
            }
            expect(fixture.error.id.empty());
        }

        MhchemFixture fixture(true);
        auto characters = std::dynamic_pointer_cast<CharacterMap>(
            fixture.maps.getMap(u"mhchem-chars"));
        expect(characters != nullptr);
        if (!characters)
            return;
        AttributeSet attributes;
        attributes.set(u"stretchy", false);
        attributes.set(u"mathvariant", u"-custom");
        attributes.set(u"texClass", 0.0);
        characters->add(Token(u"mhchemCustom", u"\u2192", std::move(attributes)));

        struct CreatorRequest {
            NodeKind kind;
            NodeDefinition definition;
            std::u16string text;
        };
        std::vector<CreatorRequest> requests;
        thread_local std::vector<CreatorRequest> *activeRequests{};
        const auto creator = [](NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                                std::optional<std::u16string_view> text) -> MmlNode & {
            if (activeRequests && kind == NodeKind::mo && text == u"\u2192")
                activeRequests->push_back({kind, definition,
                                           text ? std::u16string(*text) : std::u16string{}});
            return NodeFactory::createToken(factory, kind, std::move(definition), text);
        };
        activeRequests = &requests;
        fixture.jax->parseOptions().request().nodeFactory.setCreators(
            NodeCreators{.token = creator});
        auto *tree = fixture.compile(mhchemCreatorCase().input);
        activeRequests = nullptr;
        expect(tree != nullptr);
        expect(requests.size() == 1_u);
        if (tree)
            expect(snapshot(*tree) == mhchemCreatorCase().snapshot);
        if (requests.size() == 1) {
            expect(requests[0].kind == NodeKind::mo);
            expect(requests[0].text == mhchemCreatorCase().text);
            expect(attribute(requests[0].definition.attributes, u"stretchy") == u"false");
            expect(attribute(requests[0].definition.attributes, u"mathvariant") ==
                   mhchemCreatorCase().mathvariant);
            expect(attribute(requests[0].definition.attributes, u"texClass") == u"0");
            auto *arrow = findToken(*tree, NodeKind::mo, mhchemCreatorCase().text);
            expect(arrow != nullptr);
            if (arrow) {
                const auto classValue = arrow->texClass();
                expect(classValue.has_value());
                if (classValue)
                    expect(*classValue == TexClass::ord);
            }
        }
    };

    "Mhchem error recovery succeeds on the same configured TeX instance"_test = [] {
        MhchemFixture fixture(true);
        expect(std::size(mhchemRecoveryCases()) == 2_u);
        for (const auto &entry : mhchemRecoveryCases()) {
            auto *tree = fixture.compile(entry.input);
            expect(tree != nullptr) << folia::math::utf16::encode(entry.input);
            if (tree)
                expect(snapshot(*tree) == entry.snapshot)
                    << folia::math::utf16::encode(entry.input);
            expect(fixture.error.name == narrow(entry.errorName))
                << folia::math::utf16::encode(entry.input);
            expect(fixture.error.id == narrow(entry.errorId))
                << folia::math::utf16::encode(entry.input);
            expect(fixture.error.message == entry.errorMessage)
                << folia::math::utf16::encode(entry.input);
            expect(fixture.jax->parseOptions().request().parsers.empty())
                << folia::math::utf16::encode(entry.input);
        }
    };

    "Mhchem typed map mutations match Source before during and after"_test = [] {
        constexpr std::array<std::string_view, 8> exercised{
            "ce-macro-replaced", "character-parser-replaced", "character-arrow-removed",
            "character-arrow-custom", "macro-arrow-removed",
            "character-false-macro-fallback", "registry-maps-replaced",
            "registry-shadow-added-arrow"};
        const std::array<std::string_view, 6> sourceOnly{
            mhchemUntypedMapValueMutations()[0], mhchemUntypedMapValueMutations()[1],
            mhchemRetiredObjectApiMutations()[0], mhchemRetiredObjectApiMutations()[1],
            mhchemRetiredObjectApiMutations()[2], mhchemRetiredObjectApiMutations()[3]};
        expect(std::size(mhchemMutationCases()) == 14_u);
        const auto rules = mhchemReplacements();
        const auto stages = [&](TokenMap const &macros, TokenMap const &characters) {
            std::array<std::u16string, 6> output;
            std::u16string current(mhchemMutationInput());
            for (std::size_t i = 0; i < rules.size(); ++i) {
                current = applyMhchemReplacement(std::move(current), rules[i], macros, characters);
                output[i] = current;
            }
            return output;
        };
        const auto compare = [](auto const &actual, auto const &expected,
                                std::string_view name, std::string_view phase) {
            for (std::size_t i = 0; i < actual.size(); ++i)
                expect(actual[i] == expected[i])
                    << std::string(name) << " " << phase << " replacement stage " << i;
        };
        const auto run = [&](std::string_view name) {
            const auto entry = std::ranges::find(mhchemMutationCases(), name,
                                                 &MhchemMutationCase::name);
            expect(entry != std::end(mhchemMutationCases())) << std::string(name) << " fixture row";
            if (entry == std::end(mhchemMutationCases()))
                return;
            MhchemFixture fixture(true);
            auto originalMacros = std::dynamic_pointer_cast<CommandMap>(
                fixture.maps.getMap(u"mhchem"));
            auto originalCharacters = std::dynamic_pointer_cast<CharacterMap>(
                fixture.maps.getMap(u"mhchem-chars"));
            expect(originalMacros != nullptr) << std::string(name) << " original macro map";
            expect(originalCharacters != nullptr) << std::string(name) << " original character map";
            if (!originalMacros || !originalCharacters)
                return;
            compare(stages(*originalMacros, *originalCharacters), entry->before, name, "before");

            const auto oldParser = originalCharacters->parser();
            const auto oldCe = originalMacros->lookup(u"ce");
            const auto oldArrow = originalMacros->lookup(u"mhchemxrightarrow");
            const auto oldCharacter = originalCharacters->lookup(u"mhchemrightarrow");
            const auto macroFrom = [](TokenLookup const &value) -> Macro const * {
                if (const auto *macro = std::get_if<Macro const *>(&value))
                    return *macro;
                return nullptr;
            };
            const auto tokenFrom = [](TokenLookup const &value) -> Token const * {
                if (const auto *token = std::get_if<Token const *>(&value))
                    return *token;
                return nullptr;
            };
            const auto *ce = macroFrom(oldCe);
            const auto *arrow = macroFrom(oldArrow);
            const auto *character = tokenFrom(oldCharacter);
            expect(ce && arrow && character) << std::string(name) << " original map entries";
            if (!ce || !arrow || !character)
                return;
            const Macro savedCe = *ce;
            const Macro savedArrow = *arrow;
            const Token savedCharacter = *character;

            if (name == "ce-macro-replaced") {
                originalMacros->add(Macro(u"ce", MacroMethod{&BaseMethods::Macro},
                                          {std::u16string(u"observer replacement"), 0.0}));
            } else if (name == "character-parser-replaced") {
                originalCharacters->setParser(TokenMapParser{MacroHandler{&BaseMethods::Macro}});
            } else if (name == "character-arrow-removed") {
                originalCharacters->remove(u"mhchemrightarrow");
            } else if (name == "character-arrow-custom") {
                AttributeSet attributes;
                attributes.set(u"stretchy", false);
                originalCharacters->add(Token(u"mhchemrightarrow", u"Z", std::move(attributes)));
            } else if (name == "macro-arrow-removed") {
                originalMacros->remove(u"mhchemxrightarrow");
            } else if (name == "character-false-macro-fallback") {
                originalCharacters->remove(u"mhchemrightarrow");
                originalMacros->add(Macro(u"mhchemrightarrow", MacroMethod{&BaseMethods::Macro},
                                          {std::u16string(u"fallback"), 0.0}));
            } else if (name == "registry-maps-replaced") {
                std::vector<Macro> observerMacros{
                    Macro(u"observerOnly", MacroMethod{&BaseMethods::Macro},
                          {std::u16string(u"Q"), 0.0}),
                    Macro(u"mhchemxrightarrow", MacroMethod{&BaseMethods::Macro},
                          {std::u16string(u"R"), 0.0})};
                fixture.maps.registerMap(std::make_shared<CommandMap>(
                    CommandMapInit{MacroMapInit{{u"mhchem", std::nullopt},
                                                std::move(observerMacros)}}));
                std::vector<Token> observerCharacters{
                    Token(u"mhchemleftarrow", u"L"), Token(u"observerOnly", u"Y")};
                fixture.maps.registerMap(std::make_shared<CharacterMap>(
                    CharacterMapInit{{u"mhchem-chars", std::nullopt},
                                     std::move(observerCharacters)}));
            } else if (name == "registry-shadow-added-arrow") {
                originalCharacters->remove(u"mhchemrightarrow");
                originalMacros->remove(u"mhchemxrightarrow");
                std::vector<Macro> observerMacros{
                    Macro(u"mhchemxrightarrow", MacroMethod{&BaseMethods::Macro},
                          {std::u16string(u"R"), 0.0})};
                fixture.maps.registerMap(std::make_shared<CommandMap>(
                    CommandMapInit{MacroMapInit{{u"mhchem", std::nullopt},
                                                std::move(observerMacros)}}));
                std::vector<Token> observerCharacters{Token(u"mhchemrightarrow", u"R")};
                fixture.maps.registerMap(std::make_shared<CharacterMap>(
                    CharacterMapInit{{u"mhchem-chars", std::nullopt},
                                     std::move(observerCharacters)}));
            }
            const auto currentCe = macroFrom(originalMacros->lookup(u"ce"));
            const auto currentMacroArrow = macroFrom(originalMacros->lookup(u"mhchemxrightarrow"));
            const auto currentCharacter =
                tokenFrom(originalCharacters->lookup(u"mhchemrightarrow"));
            if (name == "ce-macro-replaced")
                expect(currentCe && std::get<MacroHandler>(currentCe->function) ==
                                         &BaseMethods::Macro)
                    << std::string(name) << " replacement handler";
            if (name == "character-parser-replaced") {
                const auto parser = originalCharacters->parser();
                const auto *handler = parser ? std::get_if<MacroHandler>(&*parser) : nullptr;
                expect(handler && *handler == &BaseMethods::Macro)
                    << std::string(name) << " replacement parser alias";
            }
            if (name == "character-arrow-removed" || name == "registry-shadow-added-arrow")
                expect(currentCharacter == nullptr) << std::string(name) << " removed token";
            if (name == "character-arrow-custom")
                expect(currentCharacter && currentCharacter->character == u"Z" &&
                       currentCharacter->attributes &&
                       attribute(*currentCharacter->attributes, u"stretchy") == u"false")
                    << std::string(name) << " custom token";
            if (name == "macro-arrow-removed" || name == "registry-shadow-added-arrow")
                expect(currentMacroArrow == nullptr) << std::string(name) << " removed macro";
            if (name == "character-false-macro-fallback") {
                expect(currentCharacter == nullptr) << std::string(name) << " character absent";
                expect(currentMacroArrow && currentMacroArrow->token == u"mhchemxrightarrow")
                    << std::string(name) << " source arrow macro";
                expect(macroFrom(originalMacros->lookup(u"mhchemrightarrow")) != nullptr)
                    << std::string(name) << " fallback macro";
            }
            if (name == "registry-maps-replaced" || name == "registry-shadow-added-arrow") {
                const auto registryMacros = fixture.maps.getMap(u"mhchem");
                const auto registryCharacters = fixture.maps.getMap(u"mhchem-chars");
                expect(registryMacros.get() != originalMacros.get())
                    << std::string(name) << " registry macro owner";
                expect(registryCharacters.get() != originalCharacters.get())
                    << std::string(name) << " registry character owner";
                if (name == "registry-maps-replaced") {
                    expect(macroFrom(registryMacros->lookup(u"observerOnly")) != nullptr)
                        << std::string(name) << " observer macro";
                    expect(macroFrom(registryMacros->lookup(u"mhchemxrightarrow")) != nullptr)
                        << std::string(name) << " shadow macro";
                    expect(tokenFrom(registryCharacters->lookup(u"mhchemleftarrow")) != nullptr)
                        << std::string(name) << " shadow token";
                } else {
                    const auto *shadowMacro = macroFrom(
                        registryMacros->lookup(u"mhchemxrightarrow"));
                    const auto *shadowToken = tokenFrom(
                        registryCharacters->lookup(u"mhchemrightarrow"));
                    expect(shadowMacro && shadowMacro->token == u"mhchemxrightarrow")
                        << std::string(name) << " registry shadow macro";
                    expect(shadowToken && shadowToken->character == u"R")
                        << std::string(name) << " registry shadow token";
                }
            }
            compare(stages(*originalMacros, *originalCharacters), entry->during, name, "during");

            if (name == "ce-macro-replaced")
                originalMacros->add(savedCe);
            else if (name == "character-parser-replaced")
                originalCharacters->setParser(oldParser);
            else if (name == "character-arrow-removed" || name == "character-arrow-custom" ||
                     name == "character-false-macro-fallback" ||
                     name == "registry-shadow-added-arrow")
                originalCharacters->add(savedCharacter);
            if (name == "macro-arrow-removed" || name == "registry-shadow-added-arrow")
                originalMacros->add(savedArrow);
            if (name == "character-false-macro-fallback")
                originalMacros->remove(u"mhchemrightarrow");
            if (name == "registry-maps-replaced" || name == "registry-shadow-added-arrow") {
                fixture.maps.registerMap(originalCharacters);
                fixture.maps.registerMap(originalMacros);
            }
            const auto restoredCe = macroFrom(originalMacros->lookup(u"ce"));
            const auto restoredParser = originalCharacters->parser();
            const auto *restoredTokenParser =
                restoredParser ? std::get_if<TokenInputHandler>(&*restoredParser) : nullptr;
            expect(restoredCe && std::get<MacroHandler>(restoredCe->function) ==
                                     &MhchemMethods::Machine)
                << std::string(name) << " restored ce alias";
            expect(restoredTokenParser && *restoredTokenParser == &MhchemUtils::relmo)
                << std::string(name) << " restored character parser";
            expect(macroFrom(originalMacros->lookup(u"mhchemxrightarrow")) != nullptr)
                << std::string(name) << " restored arrow macro";
            expect(tokenFrom(originalCharacters->lookup(u"mhchemrightarrow")) != nullptr)
                << std::string(name) << " restored arrow token";
            compare(stages(*originalMacros, *originalCharacters), entry->after, name, "after");
        };
        for (const auto name : exercised)
            run(name);
        for (const auto name : sourceOnly) {
            const auto entry = std::ranges::find(mhchemMutationCases(), name,
                                                 &MhchemMutationCase::name);
            expect(entry != std::end(mhchemMutationCases()))
                << std::string(name) << " Source-only fixture row";
            if (entry != std::end(mhchemMutationCases())) {
                expect(entry->before == entry->after)
                    << std::string(name) << " Source-only restored stages";
                if (name == "character-arrow-false" || name == "character-arrow-null")
                    expect(entry->during != entry->before)
                        << std::string(name) << " Source-only changed stages";
                else
                    expect(entry->during == entry->before)
                        << std::string(name) << " retired object API stages";
            }
        }
    };
};
