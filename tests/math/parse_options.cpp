#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.parser;

using namespace boost::ut;
using namespace folia::math::tex;

namespace {
struct StyleCase {
    std::u16string_view input, tex, texBold, french;
};
#include "fixtures/tex-source/parse-options-source.inc"
}

suite parse_options = [] {
    "parse options defaults preserve sparse fields and later false zero empty values"_test = [] {
        TexOptionValues values;
        values.maxBuffer = 5120;
        values.physics = PhysicsOptions{.italicdiff = true, .arrowdel = true};
        values.noundefined = NoUndefinedOptions{.color = u"red", .size = u"small"};
        TexOptionValues layer;
        layer.maxBuffer = 0;
        layer.physics = PhysicsOptions{.italicdiff = false};
        layer.noundefined = NoUndefinedOptions{.color = u""};
        values.mergeDefaults(layer);
        expect(values.maxBuffer.value() == 0_u);
        expect(!values.physics->italicdiff.value());
        expect(values.physics->arrowdel.value());
        expect(values.noundefined->color->empty());
        expect(values.noundefined->size.value() == u"small");
        expect(!values.mathStyle.has_value());
        expect(layer.physics->italicdiff.has_value());
        expect(!layer.physics->arrowdel.has_value());
    };

    "parse options replace arrays and own copied definition values"_test = [] {
        TexOptionValues values;
        values.protectedMacros = std::vector<std::u16string>{u"a", u"b"};
        values.macros.emplace();
        (*values.macros)[u"a"] = MacroDefinition{.replacement_source = u"old", .arity = 1,
                                               .default_first_argument = u"x"};
        (*values.macros)[u"b"] = MacroDefinition{.replacement_source = u"keep"};
        TexOptionValues layer;
        layer.protectedMacros = std::vector<std::u16string>{u"c"};
        layer.macros.emplace();
        (*layer.macros)[u"a"] = MacroDefinition{.replacement_source = u"new"};
        (*layer.macros)[u"c"] = MacroDefinition{.replacement_source = u"add"};
        values.mergeDefaults(layer);
        layer.protectedMacros->push_back(u"changed");
        (*layer.macros)[u"a"].replacement_source = u"changed";
        expect(values.protectedMacros.value() == std::vector<std::u16string>{u"c"});
        expect(values.macros->size() == 3_u);
        expect(values.macros->at(u"a").replacement_source == u"new");
        expect(values.macros->at(u"a").arity == 0_u);
        expect(!values.macros->at(u"a").default_first_argument.has_value());
        expect(values.macros->at(u"b").replacement_source == u"keep");
    };

    "parse options merge permissions by domain and preserve explicit package denial"_test = [] {
        using PermissionMap = std::unordered_map<std::u16string, bool>;
        using Permission = SetOptionsOptions::Permission;
        using PackageMap = std::unordered_map<std::u16string, Permission>;
        TexOptionValues values;
        values.setoptions = SetOptionsOptions{.allowOptions = PackageMap{
            {u"tex", PermissionMap{{u"maxBuffer", false}, {u"mathStyle", true}}}, {u"physics", true}}};
        TexOptionValues layer;
        layer.setoptions = SetOptionsOptions{.allowOptions = PackageMap{
            {u"tex", PermissionMap{{u"maxMacros", false}, {u"mathStyle", false}}}, {u"color", false}}};
        values.mergeDefaults(layer);
        auto const& tex = std::get<PermissionMap>(values.setoptions->allowOptions->at(u"tex"));
        expect(tex.size() == 3_u);
        expect(!tex.at(u"maxBuffer") && !tex.at(u"mathStyle") && !tex.at(u"maxMacros"));
        expect(std::get<bool>(values.setoptions->allowOptions->at(u"physics")));
        expect(!std::get<bool>(values.setoptions->allowOptions->at(u"color")));
        layer.setoptions->allowOptions = PackageMap{
            {u"tex", false}, {u"physics", PermissionMap{{u"italicdiff", false}}}};
        values.mergeDefaults(layer);
        expect(!std::get<bool>(values.setoptions->allowOptions->at(u"tex")));
        expect(!std::get<PermissionMap>(values.setoptions->allowOptions->at(u"physics")).at(u"italicdiff"));
    };

    "parse options retain regular expression state across default composition"_test = [] {
        TexOptionValues values, layer;
        auto expression = std::make_shared<folia::math::regexp_host::RegExp>(u"a", u"g");
        layer.numberPattern = expression;
        values.mergeDefaults(layer);
        expect(values.numberPattern.value() == expression);
        values.numberPattern.value()->last_index = 2;
        expect(expression->last_index == 2_u);
        TexOptionValues absent;
        values.mergeDefaults(absent);
        expect(values.numberPattern.value() == expression);
    };

    "parse options style resolvers match original mathematical variant functions"_test = [] {
        auto tex = ParseOptions::getVariant(u"TeX");
        auto iso = ParseOptions::getVariant(u"ISO");
        auto french = ParseOptions::getVariant(u"French");
        auto upright = ParseOptions::getVariant(u"upright");
        expect(tex != nullptr && iso != nullptr && french != nullptr && upright != nullptr);
        expect(ParseOptions::getVariant(u"tex") == nullptr);
        expect(ParseOptions::getVariant(u"unknown") == nullptr);
        for (auto const& row : styleCases) {
            expect(tex(row.input, false) == row.tex);
            expect(tex(row.input, true) == row.texBold);
            for (bool bold : {false, true}) {
                expect(iso(row.input, bold) == u"italic");
                expect(upright(row.input, bold) == u"normal");
                expect(french(row.input, bold) == row.french);
            }
        }
    };
};

namespace {
struct ResetWitnessTags final : NoTags {
    using NoTags::NoTags;
    void resetTag() override { throw std::runtime_error("reset-witness"); }
};
suite retained_parse_options_owner_values = [] {
    "parse_options_parser_stack_and_tree_filter_preserve_duplicates"_test = [] {
        Fixture f;
        auto& options = *f.options;
        TexParser parser(u"", {}, options);
        expect(options.parser() == &parser);
        parser.mml();
        expect(options.parser() == nullptr);
        options.pushParser(parser);
        expect(options.parser() == &parser);
        options.popParser();
        expect(options.parser() == nullptr);
        auto& root = f.node(NodeKind::mrow);
        auto& live = f.node(NodeKind::mi);
        auto& detached = f.node(NodeKind::mn);
        root.appendChild(live);
        options.request().root = &root;
        options.addNode(u"custom", live);
        options.addNode(u"custom", detached);
        options.addNode(u"custom", live);
        expect(live.properties().tex.in_lists ==
               std::optional<std::vector<std::u16string>>(std::vector<std::u16string>{u"custom", u"custom"}));
        expect(detached.properties().tex.in_lists ==
               std::optional<std::vector<std::u16string>>(std::vector<std::u16string>{u"custom"}));
        auto filtered = options.getList(u"custom");
        expect(filtered.size() == 2_u);
        if (filtered.size() == 2) expect(filtered[0] == &live && filtered[1] == &live);
        const std::array removed{&live};
        options.removeFromList(u"custom", removed);
        expect(options.getList(u"custom").size() == 1_u);
        expect(options.getList(u"custom").front() == &live);
        options.addNode(u"root", root);
        expect(options.getList(u"root").size() == 1_u);
    };
    "parse_options_clear_keeps_persistent_owners_and_clears_before_reset_failure"_test = [] {
        Fixture f;
        auto& options = *f.options;
        auto* effective = &options.options;
        auto* packageData = &options.packageData;
        auto* handlers = &options.handlers;
        auto* itemFactory = options.itemFactory.get();
        auto* nodeFactory = &options.request().nodeFactory;
        options.options.maxBuffer = 17;
        TexParser parser(u"", {}, options);
        parser.mml();
        auto populate = [&] {
            options.pushParser(parser);
            options.request().root = &f.node(NodeKind::mrow);
            options.request().error = true;
            options.addNode(u"custom", *options.request().root);
        };
        auto check = [&] {
            expect(options.parser() == nullptr);
            expect(options.request().root == nullptr);
            expect(options.request().nodeLists.empty());
            expect(!options.request().error);
            expect(&options.options == effective && &options.packageData == packageData);
            expect(&options.handlers == handlers && options.itemFactory.get() == itemFactory);
            expect(&options.request().nodeFactory == nodeFactory);
            expect(options.options.maxBuffer.value() == 17.0);
        };
        populate(); options.clear(); check();
        options.tags = std::make_unique<ResetWitnessTags>(options);
        auto* tags = options.tags.get();
        populate();
        expect(throws<std::runtime_error>([&] { options.clear(); }));
        check();
        expect(options.tags.get() == tags);
    };
};
}
