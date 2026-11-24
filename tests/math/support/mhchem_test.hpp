#pragma once

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
