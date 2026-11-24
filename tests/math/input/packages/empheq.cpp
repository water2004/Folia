#include "math/support/tex_fixture.hpp"
#include "math/support/fixture_values.hpp"
import folia.math.input.tex.empheq.configuration;
import folia.math.input.tex.empheq.util;
import folia.math.input.tex.ams.configuration;
import folia.math.input.tex.boldsymbol.configuration;
import folia.math.input.tex.parse_methods;

namespace {
using namespace std::literals;
constexpr std::array<MacroHandler, 2> emphCallbacks{&BaseMethods::Macro,
                                                    &AmsMethods::HandleDeclareOp};
thread_local std::string *emphError{};
MmlNode &formatEmpheqError(TeX &jax, TexError const &error) {
    if (emphError)
        *emphError = error.id();
    return jax.formatError(error);
}
struct EmpheqFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{emphCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit EmpheqFixture(std::u16string_view side = u"right", bool bold = false) {
        AmsConfiguration::initializeModule(registry, maps);
        EmpheqConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages =
            std::vector<PackageSelection>{{u"base", {}}, {u"ams", {}}, {u"empheq", {}}};
        if (bold) {
            BoldsymbolConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"boldsymbol", {}});
        }
        options.tags = u"ams";
        options.tagSide = std::u16string(side);
        options.maxMacros = 1000;
        options.formatError = &formatEmpheqError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{emphError};
            ~Restore() {
                emphError = old;
            }
        } restore;
        emphError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(true);
        auto &result = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return result;
    }
    MmlNode &ok(std::u16string source) {
        auto &result = compile(std::move(source));
        expect(error.empty()) << error;
        return result;
    }
};
std::vector<MmlNode *> emphNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> nodes;
    root.walkTree([&](auto &n) {
        if (n.kind() == kind)
            nodes.push_back(&n);
    });
    return nodes;
}
std::u16string emphText(MmlNode &root) {
    std::u16string text;
    root.walkTree([&](auto &n) {
        if (n.isToken())
            text += NodeUtil::getText(n);
    });
    return text;
}
std::u16string decimal(std::size_t n) {
    auto s = std::to_string(n);
    return {s.begin(), s.end()};
}
std::u16string number(double n) {
    if (n == 0)
        return u"0";
    std::array<char, 64> buffer;
    auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), n);
    return {buffer.data(), converted.ptr};
}
// Length-prefixed primitive records avoid a JSON dependency and preserve UTF-16 text exactly.
std::u16string emphSnapshot(MmlNode const &root) {
    std::u16string out;
    auto add = [&](std::u16string_view value) {
        out += decimal(value.size()) + u":" + std::u16string(value);
    };
    auto visit = [&](auto &&self, MmlNode const *node) -> void {
        if (!node) {
            add(u"null");
            return;
        }
        auto name = nodeKindName(node->kind());
        add(std::u16string(name.begin(), name.end()));
        auto entries = node->attributes().ownEntries();
        std::ranges::sort(entries, {}, &AttributeSet::Entry::first);
        add(decimal(entries.size()));
        for (auto const &[key, value] : entries) {
            add(key);
            std::visit(
                [&](auto const &scalar) {
                    using T = std::remove_cvref_t<decltype(scalar)>;
                    if constexpr (std::is_same_v<T, AttributeNull>) {
                        add(u"null");
                        add(u"");
                    } else if constexpr (std::is_same_v<T, bool>) {
                        add(u"boolean");
                        add(scalar ? u"true" : u"false");
                    } else if constexpr (std::is_same_v<T, double>) {
                        add(u"number");
                        add(number(scalar));
                    } else {
                        add(u"string");
                        add(scalar);
                    }
                },
                value);
        }
        add(node->isToken() ? NodeUtil::getText(*node) : u"");
        add(decimal(node->isToken() ? 0 : node->children().size()));
        if (!node->isToken())
            for (auto child : node->children())
                self(self, child);
    };
    visit(visit, &root);
    return out;
}
void matchesSource(MmlNode const &root, std::u16string_view expected) {
    auto actual = emphSnapshot(root);
    expect(std::ranges::equal(actual, expected)) << "Empheq Source structure";
}
void parentLinks(MmlNode &root) {
    root.walkTree([&](auto &node) {
        for (auto child : node.children())
            if (child)
                expect(child->parent() == &node);
    });
}
struct EmpheqCase {
    std::u16string source, tagSide, snapshot;
    bool bold{};
};
struct EmpheqSymbol {
    std::u16string name, glyph, snapshot;
};
struct EmpheqUtilityCase {
    std::u16string name, shape, snapshot;
};
struct EmpheqError {
    std::u16string source;
    std::string id;
};
const auto& empheqCases() {
    static const auto result = math_test::fixtures::records<EmpheqCase>(
        "math/fixtures/packages/empheq/cases.tsv", 4, 68, [](const auto& r) {
            return EmpheqCase{folia::test::data::decode_utf16(r[0]), folia::test::data::decode_utf16(r[1]), folia::test::data::decode_utf16(r[2]), math_test::fixtures::boolean(r[3])};
        });
    return result;
}

const auto& empheqSymbols() {
    static const auto result = math_test::fixtures::records<EmpheqSymbol>(
        "math/fixtures/packages/empheq/symbols.tsv", 3, 32, [](const auto& r) {
            return EmpheqSymbol{folia::test::data::decode_utf16(r[0]), folia::test::data::decode_utf16(r[1]), folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

const auto& empheqUtilityCases() {
    static const auto result = math_test::fixtures::records<EmpheqUtilityCase>(
        "math/fixtures/packages/empheq/utilities.tsv", 3, 8, [](const auto& r) {
            return EmpheqUtilityCase{folia::test::data::decode_utf16(r[0]), folia::test::data::decode_utf16(r[1]), folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

const auto& empheqErrors() {
    static const auto result = math_test::fixtures::records<EmpheqError>(
        "math/fixtures/packages/empheq/errors.tsv", 2, 9, [](const auto& r) {
            return EmpheqError{folia::test::data::decode_utf16(r[0]), math_test::fixtures::bytes(r[1])};
        });
    return result;
}

const auto& empheqTypeErrors() {
    static const auto result = math_test::fixtures::records<std::u16string>(
        "math/fixtures/packages/empheq/type-errors.tsv", 1, 3, [](const auto& r) {
            return folia::test::data::decode_utf16(r[0]);
        });
    return result;
}

MmlNode &utilityTable(EmpheqFixture &f, std::u16string_view shape = u"ragged") {
    auto &factory = f.document.factory();
    auto cell = [&](std::u16string_view content) -> MmlNode & {
        auto &text = factory.make(NodeKind::text);
        text.setText(std::u16string(content));
        const std::array textChildren{&text};
        auto &token = factory.make(NodeKind::mi, {}, textChildren);
        const std::array tokens{&token};
        return factory.make(NodeKind::mtd, {}, tokens);
    };
    std::vector<MmlNode *> rows;
    if (!std::ranges::equal(shape, u"empty"sv)) {
        std::vector<MmlNode *> first;
        if (std::ranges::equal(shape, u"labeled"sv))
            first.push_back(&cell(u"label"));
        first.push_back(&cell(u"a"));
        rows.push_back(&factory.make(std::ranges::equal(shape, u"labeled"sv) ? NodeKind::mlabeledtr
                                                                             : NodeKind::mtr,
                                     {}, first));
        const std::array second{&cell(u"b"), &cell(u"c"), &cell(u"d")};
        rows.push_back(&factory.make(NodeKind::mtr, {}, second));
    }
    AttributeSet attrs;
    attrs.set(u"columnalign", u"center right left");
    attrs.set(u"columnspacing", u"1em 2em");
    return factory.make(NodeKind::mtable, std::move(attrs), rows);
}
thread_local std::vector<NodeKind> *emphRequests{};
MmlNode &observeEmphNode(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                         NodeDefinition definition, MmlNode *text) {
    if (emphRequests)
        emphRequests->push_back(kind);
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
suite empheq_suite = [] {
    "empheq original configuration environment table and all fixed command bindings"_test = [] {
        EmpheqFixture f;
        auto config = f.registry.get(u"empheq");
        expect(config->priority == 5.0);
        expect(!config->init());
        expect(!config->config());
        for (auto const &h : config->handler) {
            if (h.kind == HandlerKind::macro) {
                expect(h.maps.size() == 1_u);
                expect(std::ranges::equal(h.maps[0], u"empheq-macros"sv));
            }
            if (h.kind == HandlerKind::environment) {
                expect(h.maps.size() == 1_u);
                expect(std::ranges::equal(h.maps[0], u"empheq-env"sv));
            }
        }
        auto map = f.maps.getMap(u"empheq-macros");
        for (auto const &c : empheqSymbols()) {
            auto lookup = map->lookup(c.name);
            auto macro = std::get<Macro const *>(lookup);
            expect(std::get<MacroHandler>(macro->function) == &EmpheqMethods::EmpheqMO);
            expect(macro->arguments.size() == 1_u);
            expect(std::ranges::equal(std::get<std::u16string>(macro->arguments[0]), c.glyph));
        }
        for (auto name : {u"empheql", u"empheqr", u"empheqbigl", u"empheqbigr"}) {
            auto macro = std::get<Macro const *>(map->lookup(name));
            expect(std::get<MacroHandler>(macro->function) == &EmpheqMethods::EmpheqDelim);
            expect(macro->arguments.empty());
        }
        auto environment = f.maps.getMap(u"empheq-env");
        expect(std::get<EnvironmentHandler>(*environment->parser()) == &ParseMethods::environment);
        auto env = std::get<Macro const *>(environment->lookup(u"empheq"));
        expect(std::get<EnvironmentBodyHandler>(env->function) == &EmpheqMethods::Empheq);
        expect(env->arguments.size() == 1_u);
        expect(std::ranges::equal(std::get<std::u16string>(env->arguments[0]), u"empheq"sv));
        for (auto allowed : EmpheqUtil::allowEnv) {
            expect(EmpheqUtil::checkEnv(allowed));
            expect(EmpheqUtil::checkEnv(std::u16string(allowed) + u"*"));
        }
        for (auto invalid :
             {u"aligned", u"split", u"array", u"cases", u"align**", u" align", u"toString"})
            expect(!EmpheqUtil::checkEnv(invalid));
    };
    "empheq six environments stars side combinations tags and exact Source trees"_test = [] {
        expect(empheqCases().size() == 68_u);
        for (auto const &c : empheqCases()) {
            EmpheqFixture f(c.tagSide, c.bold);
            auto &root = f.ok(std::u16string(c.source));
            matchesSource(root, c.snapshot);
            parentLinks(root);
        }
    };
    "empheq all thirty two fixed symbols preserve real glyphs and definitions"_test = [] {
        expect(empheqSymbols().size() == 32_u);
        for (auto const &c : empheqSymbols()) {
            EmpheqFixture f;
            auto &root = f.ok(u"\\" + std::u16string(c.name));
            matchesSource(root, c.snapshot);
            auto nodes = emphNodes(root, NodeKind::mo);
            expect(nodes.size() == 1_u);
            if (!nodes.empty())
                expect(std::ranges::equal(NodeUtil::getText(*nodes[0]), c.glyph));
        }
    };
    "empheq direct utility transformations match independent Source trees"_test = [] {
        for (auto const &c : empheqUtilityCases()) {
            EmpheqFixture f;
            TexParser parser(u"x", {}, f.jax->parseOptions());
            auto &original = utilityTable(f, c.shape);
            auto before = emphSnapshot(original);
            MmlNode *result{};
            if (std::ranges::equal(c.name, u"cellBlock"sv)) {
                result = &EmpheqUtil::cellBlock(u"a+b", original, parser);
                expect(result->children().size() == 1_u);
                expect(result->children()[0]->isInferred());
                expect(result->children()[0]->children().size() == 4_u);
                auto tables = emphNodes(*result, NodeKind::mtable);
                expect(tables.size() == 1_u);
                if (!tables.empty())
                    expect(tables[0] == &original);
            } else if (std::ranges::equal(c.name, u"topRowTable"sv)) {
                result = &EmpheqUtil::topRowTable(original, parser);
                auto copy = emphNodes(*result, NodeKind::mtable)[0];
                expect(copy != &original);
                expect(copy->children().size() == 1_u);
                expect(original.children().size() == 2_u);
                expect(copy->children()[0] != original.children()[0]);
                expect(std::ranges::equal(attr(*copy, u"align"), u"baseline 1"sv));
                copy->children()[0]->attributes().set(u"data-copy", u"yes");
                expect(!original.children()[0]->attributes().own(u"data-copy"));
                copy->children()[0]->attributes().erase(u"data-copy");
                auto const &list = f.jax->parseOptions().request().nodeLists.at(u"mtable");
                expect(std::ranges::find(list, copy) != list.end());
            } else if (std::ranges::equal(c.name, u"rowspanCell"sv)) {
                result = &f.document.factory().make(NodeKind::mtd);
                EmpheqUtil::rowspanCell(*result, u"L", original, parser);
            } else {
                result = &parse_util::copyNode(original, parser);
                if (std::ranges::equal(c.name, u"left"sv))
                    EmpheqUtil::left(*result, original, u"L", parser);
                else
                    EmpheqUtil::right(*result, original, u"L", parser);
            }
            if (!std::ranges::equal(c.name, u"cellBlock"sv))
                expect(std::ranges::equal(emphSnapshot(original), before));
            matchesSource(*result, c.snapshot);
            parentLinks(*result);
            parser.mml();
            expect(f.jax->parseOptions().request().parsers.empty());
        }
    };
    "empheq utility column counts options tag creation and configured creator boundaries"_test =
        [] {
            EmpheqFixture f;
            TexParser parser(u"x", {}, f.jax->parseOptions());
            expect(EmpheqUtil::columnCount(utilityTable(f, u"empty")) == 0_u);
            expect(EmpheqUtil::columnCount(utilityTable(f, u"ragged")) == 3_u);
            expect(EmpheqUtil::columnCount(utilityTable(f, u"labeled")) == 3_u);
            auto &labeled = utilityTable(f, u"labeled");
            const std::array first{labeled.children()[0]};
            labeled.setChildren(first);
            expect(EmpheqUtil::columnCount(labeled) == 1_u);
            const std::array<KeyValueOption, 2> allowed{{{u"left", {}}, {u"right", {}}}};
            auto attrs = EmpheqUtil::splitOptions(u"left={a=b},right={c,d}", allowed);
            expect(std::ranges::equal(attr(attrs, u"left"), u"a=b"sv));
            expect(std::ranges::equal(attr(attrs, u"right"), u"c,d"sv));
            texError([&] { EmpheqUtil::splitOptions(u"bad=x", allowed); }, "InvalidOption");
            std::vector<NodeKind> requests;
            emphRequests = &requests;
            struct Reset {
                ~Reset() {
                    emphRequests = nullptr;
                }
            } reset;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.node = &observeEmphNode});
            auto &table = utilityTable(f);
            EmpheqUtil::topRowTable(table, parser);
            expect(std::ranges::count(requests, NodeKind::mphantom) == 0);
            expect(std::ranges::count(requests, NodeKind::mpadded) == 1);
            requests.clear();
            EmpheqUtil::cellBlock(u"a+b", table, parser);
            expect(std::ranges::count(requests, NodeKind::mphantom) == 1);
            parser.tags().setLabel(u"side");
            EmpheqUtil::cellBlock(u"L", utilityTable(f), parser, u"align");
            auto const &equation = f.jax->parseOptions().request().equation;
            expect(equation.current_tag.environment == "align");
            expect(std::ranges::equal(equation.labels.at(u"side").tag, u"1"sv));
            expect(std::ranges::equal(equation.labels.at(u"side").id, u"mjx-eqn:side"sv));
            parser.mml();
            expect(f.jax->parseOptions().request().parsers.empty());
        };
    "empheq label next reference and original errors recover parser borrows"_test = [] {
        EmpheqFixture labels(u"left");
        labels.ok(u"\\begin{empheq}[left=L,right=R]{align}a&=b\\tag{A}\\label{first}\\end{empheq}");
        expect(std::ranges::equal(emphText(labels.ok(u"\\ref{first}+\\eqref{first}")), u"A+(A)"sv));
        for (auto const &c : empheqErrors()) {
            EmpheqFixture f;
            f.compile(std::u16string(c.source));
            expect(f.error == c.id);
            expect(f.jax->parseOptions().request().parsers.empty());
            f.ok(u"x");
        }
        for (auto source : empheqTypeErrors()) {
            EmpheqFixture f;
            try {
                f.compile(std::u16string(source));
                expect(false) << "Expected Empheq argument type error";
            } catch (std::invalid_argument const &) {
            }
            expect(f.jax->parseOptions().request().parsers.empty());
            f.ok(u"x");
            auto &recovered = f.ok(u"\\begin{equation}z\\label{after}\\end{equation}");
            expect(emphNodes(recovered, NodeKind::mlabeledtr).size() == 1_u);
            expect(std::ranges::equal(f.jax->session().labels().at(u"after").tag, u"1"sv));
        }
    };
    "empheq delimiters and fixed symbols use the real boldsymbol creator"_test = [] {
        EmpheqFixture f(u"right", true);
        const auto source = u"\\boldsymbol{\\empheqlbrace \\empheql(}";
        auto tokens = emphNodes(f.ok(source), NodeKind::mo);
        expect(tokens.size() == 2_u);
        if (tokens.size() == 2) {
            for (auto node : tokens)
                expect(std::ranges::equal(attr(*node, u"mathvariant"), u"bold"sv));
            expect(std::ranges::equal(attr(*tokens[0], u"data-latex"), u"\\empheqlbrace"sv));
            expect(
                std::ranges::equal(attr(*tokens[1], u"data-latex"), std::u16string_view(source)));
        }
    };
};
} // namespace
