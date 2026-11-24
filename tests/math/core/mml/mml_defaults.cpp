#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.node_defaults;
#include "math/support/mml_default_fixtures.hpp"
using namespace boost::ut;
namespace mml = folia::math::mml;
using SourceDefaults = math_test::fixtures::MmlDefaultFixtureData<
    mml::AttributeDefaultEntry, mml::AttributeDefaultValue, mml::AttributeNull>;

namespace {
bool valueMatchesSource(
    const mml::AttributeDefaultValue& actual,
    const mml::AttributeDefaultValue& expected) {
    return std::operator==(actual, expected);
}

bool valueMatchesSource(
    const mml::AttributeValue& actual,
    const mml::AttributeDefaultValue& expected) {
    if (std::holds_alternative<mml::AttributeNull>(expected)) {
        return std::holds_alternative<mml::AttributeNull>(actual);
    }
    if (const auto* value = std::get_if<bool>(&expected)) {
        const auto* actualValue = std::get_if<bool>(&actual);
        return actualValue && *actualValue == *value;
    }
    if (const auto* value = std::get_if<double>(&expected)) {
        const auto* actualValue = std::get_if<double>(&actual);
        return actualValue && *actualValue == *value;
    }
    const auto* value = std::get_if<std::u16string_view>(&expected);
    const auto* actualValue = std::get_if<std::u16string>(&actual);
    return value && actualValue && *actualValue == *value;
}

std::size_t sourceKindIndex(mml::NodeKind kind) {
    const auto name = mml::nodeKindName(kind);
    for (std::size_t i = 0; i < SourceDefaults::nodeNames().size(); ++i) {
        if (name == SourceDefaults::nodeNames()[i]) {
            return i;
        }
    }
    return SourceDefaults::nodeNames().size();
}

bool sourceLayersMatchRecursively(const mml::MmlNode& node) {
    const auto index = sourceKindIndex(node.kind());
    if (index == SourceDefaults::nodeNames().size()) {
        return false;
    }

    const auto& attributes = node.attributes();
    const auto expectedRange = SourceDefaults::nodeRanges()[index];
    if (attributes.defaultEntries().size() != expectedRange.size) {
        return false;
    }
    for (std::size_t i = 0; i < expectedRange.size; ++i) {
        const auto& expected = SourceDefaults::nodeEntries()[expectedRange.offset + i];
        const auto& actual = attributes.defaultEntries()[i];
        if (actual.first != expected.name || !valueMatchesSource(actual.second, expected.value)) {
            return false;
        }
    }

    if (SourceDefaults::nodeHasAttributes()[index]) {
        if (attributes.globalDefaultEntries().size() != SourceDefaults::globalEntries().size()) {
            return false;
        }
        for (std::size_t i = 0; i < attributes.globalDefaultEntries().size(); ++i) {
            const auto& actual = attributes.globalDefaultEntries()[i];
            const auto& expected = SourceDefaults::globalEntries()[i];
            if (actual.first != expected.name || !valueMatchesSource(actual.second, expected.value)) {
                return false;
            }
        }
    } else if (!attributes.globalDefaultEntries().empty() ||
               !attributes.ownEntries().empty() ||
               !attributes.inheritedEntries().empty()) {
        return false;
    }

    for (const auto* child : node.children()) {
        if (!child || !sourceLayersMatchRecursively(*child)) {
            return false;
        }
    }
    return true;
}
}

suite math_native_mml_default_tables = [] {
    "all_43_node_default_tables_match_pinned_source_snapshot"_test = [] {
            expect(mml::allNodeKinds.size() == 43u);
        expect(SourceDefaults::nodeRanges().size() == 43u);
        expect(SourceDefaults::nodeNames().size() == 43u);
        expect(SourceDefaults::nodeHasAttributes().size() == 43u);

        std::size_t totalNodeEntries = 0;
        for (std::size_t i = 0; i < mml::allNodeKinds.size(); ++i) {
            const auto kind = mml::allNodeKinds[i];
            const auto sourceKind = mml::nodeKindName(kind);
            expect(sourceKind == SourceDefaults::nodeNames()[i]);
            const auto actual = mml::nodeDefaults(kind);
            expect(!actual.empty() == SourceDefaults::nodeHasAttributes()[i]);
            const auto expectedRange = SourceDefaults::nodeRanges()[i];
            expect(actual.size() == expectedRange.size);
            totalNodeEntries += actual.size();
            for (std::size_t entry = 0; entry < actual.size(); ++entry) {
                const auto& expected = SourceDefaults::nodeEntries()[expectedRange.offset + entry];
                expect(actual[entry].name.compare(expected.name) == 0);
                expect(valueMatchesSource(actual[entry].value, expected.value));
            }
        }
        expect(totalNodeEntries == SourceDefaults::nodeEntries().size());

        const auto actualGlobal = mml::globalDefaults();
        expect(actualGlobal.size() == SourceDefaults::globalEntries().size());
        for (std::size_t i = 0; i < actualGlobal.size(); ++i) {
            expect(actualGlobal[i].name.compare(SourceDefaults::globalEntries()[i].name) == 0);
            expect(valueMatchesSource(actualGlobal[i].value, SourceDefaults::globalEntries()[i].value));
        }
        expect(mml::nodeDefaults(static_cast<mml::NodeKind>(255)).empty());
    };
};

suite math_native_attribute_layers = [] {
    "lookup_precedence_and_effective_layer_fallbacks_are_mathematical"_test = [] {
        mml::AttributeSet attributes;
        attributes.setGlobalDefault(u"shared", std::u16string(u"global"));
        attributes.setDefault(u"shared", std::u16string(u"default"));
        attributes.setInherited(u"shared", std::u16string(u"inherited"));
        attributes.set(u"shared", std::u16string(u"own"));

        expect(std::get<std::u16string>(*attributes.lookup(u"shared")) == u"own");
        expect(std::get<std::u16string>(*attributes.effectiveInherited(u"shared")) == u"inherited");
        expect(std::get<std::u16string>(*attributes.effectiveDefault(u"shared")) == u"default");
        expect(attributes.isSet(u"shared"));
        expect(attributes.hasDefault(u"shared"));

        expect(attributes.erase(u"shared"));
        expect(std::get<std::u16string>(*attributes.lookup(u"shared")) == u"inherited");
        expect(attributes.erase(u"shared") == false);
        attributes.setInherited(u"shared", std::u16string(u"_inherit_"));
        expect(std::get<std::u16string>(*attributes.inherited(u"shared")) == u"_inherit_");
        expect(std::get<std::u16string>(*attributes.effectiveInherited(u"shared")) == u"_inherit_");
        expect(std::get<std::u16string>(*attributes.lookup(u"shared")) == u"global");
    };

    "inherit_sentinel_resolves_only_lookup_and_null_differs_from_absence"_test = [] {
        mml::AttributeSet attributes;
        attributes.setGlobalDefault(u"color", std::u16string(u"blue"));
        attributes.setDefault(u"color", std::u16string(u"_inherit_"));
        expect(std::get<std::u16string>(*attributes.defaultValue(u"color")) == u"_inherit_");
        expect(std::get<std::u16string>(*attributes.lookup(u"color")) == u"blue");

        attributes.set(u"explicit-null", mml::AttributeNull{});
        expect(attributes.own(u"explicit-null") != nullptr);
        expect(std::holds_alternative<mml::AttributeNull>(*attributes.lookup(u"explicit-null")));
        expect(attributes.own(u"absent") == nullptr);
        expect(attributes.lookup(u"absent") == nullptr);
        expect(attributes.isSet(u"explicit-null"));
        expect(attributes.isSet(u"absent") == false);
        expect(attributes.hasDefault(u"absent") == false);
    };

    "updates_keep_layer_order_and_erase_affects_only_own_values"_test = [] {
        mml::AttributeSet attributes;
        attributes.set(u"first", 1.0);
        attributes.set(u"second", 2.0);
        attributes.set(u"first", 3.0);
        expect(attributes.ownEntries().size() == 2u);
        expect(attributes.ownEntries()[0].first == u"first");
        expect(std::get<double>(attributes.ownEntries()[0].second) == 3.0);
        expect(attributes.erase(u"first"));
        expect(attributes.ownEntries().size() == 1u);
        expect(attributes.ownEntries()[0].first == u"second");
        attributes.set(u"first", 4.0);
        expect(attributes.ownEntries()[1].first == u"first");
        attributes.setDefault(u"first", 5.0);
        expect(attributes.erase(u"first"));
        expect(attributes.defaultValue(u"first") != nullptr);
        expect(std::get<double>(*attributes.lookup(u"first")) == 5.0);
    };
};

suite math_native_mml_default_installation = [] {
    "factory_initializes_every_kind_with_the_complete_source_layers"_test = [] {
            mml::MmlDocument document;
        auto& factory = document.factory();
        std::size_t kindsWithAttributeStore = 0;

        for (std::size_t i = 0; i < mml::allNodeKinds.size(); ++i) {
            const auto kind = mml::allNodeKinds[i];
            auto& node = factory.make(kind);
            const auto& attributes = node.attributes();
            const auto range = SourceDefaults::nodeRanges()[i];
            if (SourceDefaults::nodeHasAttributes()[i]) {
                ++kindsWithAttributeStore;
                expect(attributes.defaultEntries().size() == range.size);
                for (std::size_t j = 0; j < range.size; ++j) {
                    expect(attributes.defaultEntries()[j].first == SourceDefaults::nodeEntries()[range.offset + j].name);
                    expect(valueMatchesSource(
                        attributes.defaultEntries()[j].second,
                        SourceDefaults::nodeEntries()[range.offset + j].value));
                }
                expect(attributes.globalDefaultEntries().size() == SourceDefaults::globalEntries().size());
                for (std::size_t j = 0; j < attributes.globalDefaultEntries().size(); ++j) {
                    expect(attributes.globalDefaultEntries()[j].first == SourceDefaults::globalEntries()[j].name);
                    expect(valueMatchesSource(
                        attributes.globalDefaultEntries()[j].second,
                        SourceDefaults::globalEntries()[j].value));
                }
            } else {
                expect(attributes.defaultEntries().empty());
                expect(attributes.globalDefaultEntries().empty());
            }
        }
        expect(kindsWithAttributeStore == 40u);
    };

    "implicit_and_explicit_rows_retain_source_defaults_recursively"_test = [] {
        mml::MmlDocument document;
        auto& factory = document.factory();

        auto& identifier = factory.make(mml::NodeKind::mi);
        identifier.setText(u"x");
        const std::array<mml::MmlNode*, 1> oneChild{&identifier};
        auto& math = factory.make(mml::NodeKind::math, {}, oneChild);
        expect(math.children().size() == 1u);
        auto* implicitRow = math.children()[0];
        expect(implicitRow->kind() == mml::NodeKind::inferred_mrow);
        expect(sourceLayersMatchRecursively(math));
        expect(std::get<std::u16string>(*implicitRow->attributes().lookup(u"mathsize")) == u"normal");
        expect(std::get<std::u16string>(*implicitRow->attributes().defaultValue(u"mathsize")) == u"_inherit_");
        implicitRow->attributes().set(u"data-source-copy", std::u16string(u"kept"));

        auto& denominator = factory.make(mml::NodeKind::mn);
        const std::array<mml::MmlNode*, 2> fractionChildren{implicitRow, &denominator};
        auto& fraction = factory.make(mml::NodeKind::mfrac, {}, fractionChildren);
        expect(fraction.children().size() == 2u);
        expect(fraction.children()[0]->kind() == mml::NodeKind::mrow);
        expect(sourceLayersMatchRecursively(fraction));
        expect(std::get<std::u16string>(*fraction.children()[0]->attributes().lookup(u"mathsize")) == u"normal");
        expect(std::get<std::u16string>(*fraction.children()[0]->attributes().lookup(u"data-source-copy")) == u"kept");
    };

    "factory_keeps_source_leaf_nodes_without_attribute_defaults_empty"_test = [] {
        mml::MmlDocument document;
        auto& factory = document.factory();
        for (const auto kind : {mml::NodeKind::text, mml::NodeKind::xml, mml::NodeKind::html}) {
            auto& node = factory.make(kind);
            expect(node.attributes().ownEntries().empty());
            expect(node.attributes().inheritedEntries().empty());
            expect(node.attributes().defaultEntries().empty());
            expect(node.attributes().globalDefaultEntries().empty());
        }
    };

    "factory_installs_defaults_without_losing_any_caller_layer"_test = [] {
        mml::MmlDocument document;
        auto& factory = document.factory();
        mml::AttributeSet supplied;
        supplied.set(u"mathvariant", std::u16string(u"bold"));
        supplied.setInherited(u"dir", std::u16string(u"rtl"));
        supplied.setDefault(u"mathvariant", std::u16string(u"italic"));
        supplied.setDefault(u"custom-default", mml::AttributeNull{});
        supplied.setGlobalDefault(u"custom-global", false);
        supplied.setGlobalDefault(u"mathsize", std::u16string(u"large"));

        auto& node = factory.make(mml::NodeKind::mi, std::move(supplied));
        const auto& attributes = node.attributes();
        expect(attributes.own(u"mathvariant") != nullptr);
        expect(attributes.inherited(u"dir") != nullptr);
        expect(attributes.defaultValue(u"mathvariant") != nullptr);
        expect(attributes.defaultValue(u"custom-default") != nullptr);
        expect(attributes.globalDefault(u"custom-global") != nullptr);
        expect(std::get<std::u16string>(*attributes.lookup(u"mathvariant")) == u"bold");
        expect(std::get<std::u16string>(*attributes.lookup(u"dir")) == u"rtl");
        expect(std::get<std::u16string>(*attributes.defaultValue(u"mathvariant")) == u"italic");
        expect(std::holds_alternative<mml::AttributeNull>(*attributes.lookup(u"custom-default")));
        expect(std::get<bool>(*attributes.lookup(u"custom-global")) == false);

        expect(std::get<std::u16string>(*attributes.defaultValue(u"mathsize")) == u"_inherit_");
        expect(std::get<std::u16string>(*attributes.lookup(u"mathsize")) == u"large");
        expect(std::get<std::u16string>(*attributes.globalDefault(u"mathsize")) == u"large");
        expect(attributes.defaultValue(u"data-vertical-align") == nullptr);
    };
};

