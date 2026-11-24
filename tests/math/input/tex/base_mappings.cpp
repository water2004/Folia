#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.base_mappings;
import folia.math.input.tex.parse_methods;

namespace {
void setupMappings(Fixture &fixture) {
    BaseMappings::initialize(fixture.maps);
    fixture.effective.numberPattern =
        std::make_shared<folia::math::regexp_host::RegExp>(u"^[0-9]+");
    fixture.effective.identifierPattern =
        std::make_shared<folia::math::regexp_host::RegExp>(u"^[a-zA-Z]+");
    const std::array handlers{
        HandlerRegistration{HandlerKind::character, {u"command", u"special"}},
        HandlerRegistration{HandlerKind::character, {u"letter", u"digit"}, nullptr, 4},
        HandlerRegistration{HandlerKind::macro,
                            {u"delimiter", u"macros", u"lcGreek", u"ucGreek", u"mathchar0mi",
                             u"mathchar0mo", u"mathchar7"}},
        HandlerRegistration{HandlerKind::delimiter, {u"delimiter"}},
        HandlerRegistration{HandlerKind::environment, {u"environment"}}};
    fixture.options->handlers.add(handlers);
}
} // namespace
suite base_mappings = [] {
    "base mappings retain original table names callback types and literal arguments"_test = [] {
        Fixture fixture;
        setupMappings(fixture);
        for (auto name :
             {u"letter", u"digit", u"command", u"special", u"lcGreek", u"ucGreek", u"mathchar0mi",
              u"mathchar0mo", u"mathchar7", u"delimiter", u"macros", u"environment", u"not_remap"})
            expect(fixture.maps.getMap(name) != nullptr);
        auto macros = fixture.maps.getMap(u"macros");
        expect(std::get<MacroHandler>(*macros->parserFor(u"mathbb")) == &BaseMethods::MathFont);
        auto definition = std::get<Macro const *>(macros->lookup(u"mathbb"));
        expect(std::get<std::u16string>(definition->arguments.front()) == u"double-struck");
        expect(std::get<TokenInputHandler>(*fixture.maps.getMap(u"lcGreek")->parserFor(u"alpha")) ==
               &ParseMethods::lcGreek);
        expect(std::get<EnvironmentBodyHandler>(
                   *fixture.maps.getMap(u"environment")->parserFor(u"array")) ==
               &BaseMethods::AlignedArray);
        expect(std::get<Token const *>(fixture.maps.getMap(u"not_remap")->lookup(u"="))
                   ->character.value() == u"≠");
    };
    "base mappings dispatch mathbb fractions roots through the actual parser"_test = [] {
        Fixture fixture;
        setupMappings(fixture);
        TexParser parser(u"\\mathbb{R}\\frac{x}{y}\\sqrt[3]{x}", ParserEnvironmentSeed{},
                         *fixture.options);
        auto root = parser.mml();
        expect(root != nullptr);
        auto identifiers = root->findNodes(NodeKind::mi);
        expect(std::ranges::any_of(identifiers, [](auto node) {
            return NodeUtil::getText(*node) == u"R" &&
                   attr(node->attributes(), u"mathvariant") == u"double-struck";
        }));
        auto fractions = root->findNodes(NodeKind::mfrac);
        expect(fractions.size() == 1_u);
        expect(NodeUtil::getText(*fractions[0]->children()[0]) == u"x");
        expect(NodeUtil::getText(*fractions[0]->children()[1]) == u"y");
        expect(root->findNodes(NodeKind::mroot).size() == 1_u);
    };
    "base mappings preserve character definition properties and escaped delimiters"_test = [] {
        Fixture fixture;
        setupMappings(fixture);
        TexParser parser(u"\\hbar\\sum\\langle", ParserEnvironmentSeed{}, *fixture.options);
        auto root = parser.mml();
        auto ids = root->findNodes(NodeKind::mi);
        expect(ids.size() == 1_u);
        expect(ids[0]->properties().tex.variant_form.value_or(false));
        expect(ids[0]->attributes().own(u"variantForm") == nullptr);
        auto ops = root->findNodes(NodeKind::mo);
        expect(ops.size() == 2_u);
        expect(ops[0]->properties().tex.move_sup_sub.value_or(false));
        expect(ops[0]->properties().tex.fix_stretchy.value_or(false));
        expect(NodeUtil::getText(*ops[1]) == u"⟨");
        expect(!std::get<bool>(*ops[1]->attributes().own(u"stretchy")));
    };
};
