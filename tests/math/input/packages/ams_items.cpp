#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.ams.configuration;
import folia.math.input.tex.newcommand.configuration;

namespace {
thread_local std::string *arrayError{};
MmlNode &formatArrayError(TeX &jax, TexError const &error) {
    if (arrayError)
        *arrayError = error.id();
    return jax.formatError(error);
}
struct AmsArrayFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit AmsArrayFixture(bool left = false) {
        AmsConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"ams", {}}};
        options.tags = u"ams";
        options.tagSide = left ? u"left" : u"right";
        options.formatError = &formatArrayError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{arrayError};
            ~Restore() {
                arrayError = old;
            }
        } restore;
        arrayError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(true);
        auto &tree = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return tree;
    }
};
std::string explicitText(MmlNode const &node, std::u16string_view key) {
    auto text = attr(node.attributes(), key);
    return {text.begin(), text.end()};
}
std::string tableShape(MmlNode &table, std::size_t scopes) {
    auto result = explicitText(table, u"width") + "|" + explicitText(table, u"columnalign") + "|" +
                  explicitText(table, u"data-array-padding") + "|" + std::to_string(scopes);
    for (auto row : table.children()) {
        result += "|" + std::string(nodeKindName(row->kind())) + ":" +
                  std::to_string(row->children().size());
        for (auto cell : row->children())
            result += ":" + explicitText(*cell, u"columnalign");
    }
    return result;
}
std::vector<MmlNode *> arrayTables(MmlNode &root) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == NodeKind::mtable)
            result.push_back(&node);
    });
    return result;
}
} // namespace

suite ams_items = [] {
    "ams configuration assembles original handlers items tags options and definition service"_test =
        [] {
            AmsArrayFixture f;
            auto package = f.registry.get(u"ams");
            expect(package->config() == &NewcommandConfig);
            expect(package->items.size() == 2_u);
            expect(package->tags.size() == 1_u);
            expect(dynamic_cast<AmsTags *>(f.jax->parseOptions().tags.get()) != nullptr);
            expect(f.jax->options.multlineWidth->empty());
            expect(f.jax->options.ams->multlineWidth.value() == u"100%");
            expect(f.jax->options.ams->multlineIndent.value() == u"1em");
            expect(f.jax->options.ams->operatornamePattern.value()->pattern() ==
                   u"^[-*a-zA-Z0-9]+");
            const HandlerConfig expected{
                {HandlerKind::character, {u"AMSmath-operatorLetter"}},
                {HandlerKind::delimiter, {u"AMSsymbols-delimiter", u"AMSmath-delimiter"}},
                {HandlerKind::macro,
                 {u"AMSsymbols-mathchar0mi", u"AMSsymbols-mathchar0mo", u"AMSsymbols-delimiter",
                  u"AMSsymbols-macros", u"AMSmath-mathchar0mo", u"AMSmath-macros",
                  u"AMSmath-delimiter"}},
                {HandlerKind::environment, {u"AMSmath-environment"}}};
            for (auto const &handler : expected) {
                auto actual = std::ranges::find(package->handler, handler.kind, &HandlerMaps::kind);
                expect(actual != package->handler.end());
                if (actual != package->handler.end())
                    expect(std::ranges::equal(actual->maps, handler.maps));
            }
            for (auto name :
                 {u"AMSmath-mathchar0mo", u"AMSmath-operatorLetter", u"AMSmath-macros",
                  u"AMSmath-environment", u"AMSmath-delimiter", u"AMSsymbols-mathchar0mi",
                  u"AMSsymbols-mathchar0mo", u"AMSsymbols-delimiter", u"AMSsymbols-macros"})
                expect(f.maps.getMap(name) != nullptr);
            auto commands = std::dynamic_pointer_cast<MacroMap>(f.maps.getMap(u"new-Command"));
            expect(commands != nullptr);
            expect(commands->sessionOwner() == &f.jax->session());
        };
    "ams array items preserve Source row shapes alignment label side and column padding"_test = [] {
        struct Case {
            std::u16string_view source;
            bool left;
            std::string_view shape;
            bool zero;
        };
        const Case cases[]{
            {u"\\begin{multline}a\\\\b\\\\c\\end{multline}", false,
             "100%||1em 1em|0|mtr:1:left|mtr:1:|mlabeledtr:2::right", false},
            {u"\\begin{multline}a\\\\b\\\\c\\end{multline}", true,
             "100%||1em 1em|0|mlabeledtr:2::left|mtr:1:|mtr:1:right", false},
            {u"\\begin{multline*}\\shoveright a\\\\b\\\\\\shoveleft c\\end{multline*}", false,
             "100%||1em 1em|0|mtr:1:right|mtr:1:|mtr:1:left", false},
            {u"\\begin{flalign}a&=b\\end{flalign}", false, "|right left||1|mlabeledtr:3:::", false},
            {u"\\begin{flalign}a&=b&c&=d\\end{flalign}", false,
             "100%|right left center right left||1|mlabeledtr:6::::::", false},
            {u"\\begin{xalignat}{2}a&=b\\end{xalignat}", false,
             "100%|center right left center right left center||1|mlabeledtr:8::::::::", true},
            {u"\\begin{xalignat}{2}a&=b\\end{xalignat}", true,
             "100%|center right left center right left center||1|mlabeledtr:8::::::::", true},
            {u"\\begin{xxalignat}{2}a&=b\\end{xxalignat}", false,
             "100%|right left center right left||1|mtr:5:::::", false},
            {u"\\begin{xalignat}{}a&=b\\end{xalignat}", false,
             "100%|center right left center||1|mlabeledtr:5:::::", true}};
        for (auto const &item : cases) {
            AmsArrayFixture f(item.left);
            auto tables = arrayTables(f.compile(std::u16string(item.source)));
            expect(f.error.empty());
            expect(tables.size() == 1_u);
            if (tables.size() != 1)
                continue;
            expect(tableShape(*tables[0], f.jax->parseOptions().request().equation.stack.size()) ==
                   item.shape);
            std::vector<MmlNode *> labels;
            for (auto row : tables[0]->children())
                if (row->kind() == NodeKind::mlabeledtr)
                    row->children()[0]->walkTree([&](auto &node) {
                        if (node.kind() == NodeKind::mpadded)
                            labels.push_back(&node);
                    });
            expect(labels.size() == (item.zero ? std::size_t{1} : std::size_t{0}));
            if (item.zero && labels.size() == 1) {
                auto width = labels[0]->attributes().own(u"width");
                expect(width && std::holds_alternative<double>(*width) &&
                       std::get<double>(*width) == 0);
                expect(attr(labels[0]->attributes(), u"lspace") == (item.left ? u"" : u"-1width"));
            }
        }
    };
    "ams array items report original column errors and reset equation state for following input"_test =
        [] {
            const std::pair<std::u16string_view, std::string_view> cases[]{
                {u"\\begin{multline}a&b\\end{multline}", "MultlineRowsOneCol"},
                {u"\\begin{xalignat}{1}a&b&c\\end{xalignat}", "XalignOverflow"},
                {u"\\begin{xxalignat}{1}a&b&c\\end{xxalignat}", "XalignOverflow"}};
            AmsArrayFixture f;
            for (auto const &[source, id] : cases) {
                f.compile(std::u16string(source));
                expect(f.error == id);
                auto tables = arrayTables(f.compile(u"\\begin{multline*}a\\\\b\\end{multline*}"));
                expect(f.error.empty());
                expect(tables.size() == 1_u);
                expect(f.jax->parseOptions().request().equation.stack.empty());
            }
        };
};
