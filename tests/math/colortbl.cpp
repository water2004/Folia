#include "math/support/tex_fixture.hpp"
#include "math/support/fixture_values.hpp"
import folia.math.input.tex.colortbl.configuration;
import folia.math.input.tex.color.configuration;
import folia.math.input.tex.ams.configuration;
import folia.math.util.utf16;

namespace {
using namespace std::literals;
thread_local std::string *tableColorError{};
MmlNode &tableColorFormatError(TeX &jax, TexError const &e) {
    if (tableColorError)
        *tableColorError = e.id();
    return jax.formatError(e);
}
thread_local ConfigMethod originalColorConfig{};
thread_local int *colorConfigCalls{};
void countColorConfig(ParserConfiguration &config, TeX &jax) {
    if (!originalColorConfig || !colorConfigCalls)
        throw std::logic_error("Missing color callback observation");
    ++*colorConfigCalls;
    originalColorConfig(config, jax);
}
struct ObserveConfig {
    ConfigMethod old{originalColorConfig};
    int *calls{colorConfigCalls};
    ObserveConfig(ConfigMethod fn, int &count) {
        originalColorConfig = fn;
        colorConfigCalls = &count;
    }
    ~ObserveConfig() {
        originalColorConfig = old;
        colorConfigCalls = calls;
    }
};
struct ColortblFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    ConfigMethod colorConfigurer{};
    int configCalls{};
    explicit ColortblFixture(bool color = false, bool colortbl = true, bool reversed = false,
                             bool count = false) {
        ColorConfiguration::initializeModule(registry, maps);
        AmsConfiguration::initializeModule(registry, maps);
        ColortblConfiguration::initializeModule(registry, maps);
        auto original = registry.get(u"color");
        colorConfigurer = original->config();
        if (count) {
            ConfigurationDefinition definition;
            definition.handler = original->handler;
            definition.options = original->options;
            definition.priority = original->priority;
            definition.config =
                ProcessorSpec<ConfigMethod>{&countColorConfig, original->configMethod->priority};
            Configuration::create(registry, u"color", std::move(definition));
        }
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{};
        if (colortbl && reversed)
            options.packages->push_back({u"colortbl", {}});
        options.packages->push_back({u"base", {}});
        options.packages->push_back({u"ams", {}});
        if (color)
            options.packages->push_back({u"color", {}});
        if (colortbl && !reversed)
            options.packages->push_back({u"colortbl", {}});
        options.maxMacros = 1000;
        options.formatError = &tableColorFormatError;
        ObserveConfig observe(colorConfigurer, configCalls);
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    void configure() {
        ObserveConfig observe(colorConfigurer, configCalls);
        colortblConfig(jax->configuration(), *jax);
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{tableColorError};
            ~Restore() {
                tableColorError = old;
            }
        } restore;
        tableColorError = &error;
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
std::u16string tableNumber(std::size_t n) {
    auto s = std::to_string(n);
    return {s.begin(), s.end()};
}
std::u16string tableSnapshot(MmlNode const &root) {
    std::u16string out;
    auto add = [&](std::u16string_view s) {
        out += tableNumber(s.size()) + u":" + std::u16string(s);
    };
    auto number = [](double n) -> std::u16string {
        if (n == 0)
            return u"0";
        std::array<char, 64> b;
        auto last = std::to_chars(b.data(), b.data() + b.size(), n).ptr;
        return {b.data(), last};
    };
    auto visit = [&](auto &&self, MmlNode const *node) -> void {
        if (!node) {
            add(u"null");
            return;
        }
        auto name = nodeKindName(node->kind());
        add(std::u16string(name.begin(), name.end()));
        auto attrs = node->attributes().ownEntries();
        std::ranges::sort(attrs, {}, &AttributeSet::Entry::first);
        add(tableNumber(attrs.size()));
        for (auto const &[key, value] : attrs) {
            add(key);
            std::visit(
                [&](auto const &v) {
                    using T = std::remove_cvref_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, AttributeNull>) {
                        add(u"null");
                        add(u"");
                    } else if constexpr (std::is_same_v<T, bool>) {
                        add(u"boolean");
                        add(v ? u"true" : u"false");
                    } else if constexpr (std::is_same_v<T, double>) {
                        add(u"number");
                        add(number(v));
                    } else {
                        add(u"string");
                        add(v);
                    }
                },
                value);
        }
        add(node->isToken() ? NodeUtil::getText(*node) : u"");
        add(tableNumber(node->isToken() ? 0 : node->children().size()));
        if (!node->isToken())
            for (auto child : node->children())
                self(self, child);
    };
    visit(visit, &root);
    return out;
}
void checkTableTree(MmlNode &root, std::u16string_view expected) {
    auto actual = tableSnapshot(root);
    if (!std::ranges::equal(actual, expected)) {
        auto mismatch = std::ranges::mismatch(actual, expected);
        auto offset = static_cast<std::size_t>(mismatch.in1 - actual.begin());
        auto start = offset > 40 ? offset - 40 : 0;
        expect(false) << "Colortbl Source difference at " << offset << " actual "
                      << folia::math::utf16::encode(std::u16string_view(actual).substr(start, 180))
                      << " expected " << folia::math::utf16::encode(expected.substr(start, 180));
    } else
        expect(true);
    root.walkTree([&](auto &node) {
        for (auto child : node.children())
            if (child)
                expect(child->parent() == &node);
    });
}
struct ColortblTree {
    std::u16string source;
    bool color, reversed;
    std::u16string snapshot;
};
struct ColortblError {
    std::u16string source;
    std::string id;
};
struct ColortblSequence {
    std::u16string source, snapshot;
};
struct ColortblDirect {
    std::u16string name, snapshot;
};
const auto& colortblTrees() {
    static const auto result = math_test::fixtures::records<ColortblTree>(
        "math/fixtures/packages/colortbl/trees.tsv", 4, 27, [](const auto& r) {
            return ColortblTree{folia::test::data::decode_utf16(r[0]), math_test::fixtures::boolean(r[1]), math_test::fixtures::boolean(r[2]), folia::test::data::decode_utf16(r[3])};
        });
    return result;
}

const auto& colortblErrors() {
    static const auto result = math_test::fixtures::records<ColortblError>(
        "math/fixtures/packages/colortbl/errors.tsv", 2, 14, [](const auto& r) {
            return ColortblError{folia::test::data::decode_utf16(r[0]), math_test::fixtures::bytes(r[1])};
        });
    return result;
}

const auto& colortblSequence() {
    static const auto result = math_test::fixtures::records<ColortblSequence>(
        "math/fixtures/packages/colortbl/sequence.tsv", 2, 4, [](const auto& r) {
            return ColortblSequence{folia::test::data::decode_utf16(r[0]), folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

const auto& colortblDirect() {
    static const auto result = math_test::fixtures::records<ColortblDirect>(
        "math/fixtures/packages/colortbl/direct.tsv", 2, 9, [](const auto& r) {
            return ColortblDirect{folia::test::data::decode_utf16(r[0]), folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

std::u16string_view directExpected(std::u16string_view name) {
    for (auto const &c : colortblDirect())
        if (std::ranges::equal(c.name, name))
            return c.snapshot;
    throw std::logic_error("Missing Source direct case");
}
template <class Fn> void withColorArray(ColortblFixture &f, Fn action) {
    TexParser parser(u"", {}, f.jax->parseOptions());
    auto stop = parser.stack.Pop();
    auto frame = parser.itemFactory().create(StackKind::array);
    parser.stack.Push(frame);
    auto &array = dynamic_cast<ColorArrayItem &>(*frame);
    action(parser, array);
    parser.stack.Pop();
    parser.stack.Push(stop);
    parser.mml();
    expect(f.jax->parseOptions().request().parsers.empty());
}
void addColorToken(TexParser &parser, ArrayItem &array, std::u16string_view text) {
    auto &node = parser.createToken(NodeKind::mi, text);
    const std::array children{&node};
    array.Push(children);
}
void invokeTableColor(TexParser &parser, std::u16string_view target, std::u16string_view source) {
    parser.setString(std::u16string(source));
    parser.setPosition(0);
    const std::array<TexArgument, 1> args{std::u16string(target)};
    TableColor(parser, MacroInvocation{u"\\columncolor", u"\\columncolor",
                                       std::span<TexArgument const>(args)});
}
suite colortbl_suite = [] {
    "colortbl registers original callbacks priority and real array override"_test = [] {
        ColortblFixture f;
        auto config = f.registry.get(u"colortbl");
        expect(config->priority == 10.0);
        expect(config->config() == &colortblConfig);
        expect(config->configMethod->priority == 10.0);
        expect(config->items.size() == 1_u);
        auto frame = f.jax->parseOptions().itemFactory->create(StackKind::array);
        expect(dynamic_cast<ColorArrayItem *>(frame.get()) != nullptr);
        expect(frame->kind() == StackKind::array);
        auto map = f.maps.getMap(u"colortbl");
        struct Binding {
            std::u16string_view name, type;
        };
        for (auto c : std::array{Binding{u"cellcolor", u"cell"}, Binding{u"rowcolor", u"row"},
                                 Binding{u"columncolor", u"col"}}) {
            auto macro = std::get<Macro const *>(map->lookup(c.name));
            expect(std::get<MacroHandler>(macro->function) == &TableColor);
            expect(macro->arguments.size() == 1_u);
            expect(std::ranges::equal(std::get<std::u16string>(macro->arguments[0]), c.type));
        }
        for (auto const &handler : config->handler)
            if (handler.kind == HandlerKind::macro) {
                expect(handler.maps.size() == 1_u);
                expect(std::ranges::equal(handler.maps[0], u"colortbl"sv));
            }
    };
    "colortbl actual arrays precedence models overlaps and wrappers match Source"_test = [] {
        expect(colortblTrees().size() == 27_u);
        for (auto const &c : colortblTrees()) {
            ColortblFixture f(c.color, true, c.reversed);
            checkTableTree(f.ok(std::u16string(c.source)), c.snapshot);
        }
    };
    "colortbl calls registered color configuration only when its model is absent"_test = [] {
        ColortblFixture f(false, true, false, true);
        expect(f.configCalls == 1);
        expect(f.jax->session().colorModel() != nullptr);
        for (auto name : {u"color", u"definecolor", u"textcolor", u"colorbox"})
            expect(!f.jax->parseOptions().handlers.get(HandlerKind::macro)->contains(name));
        auto model = f.jax->session().colorModel();
        model->defineColor(u"rgb", u"Stored", u"0,0.5,1");
        f.configure();
        expect(f.configCalls == 1);
        expect(f.jax->session().colorModel() == model);
        expect(std::ranges::equal(model->getColor(u"named", u"Stored"), u"#007fff"sv));
        ColortblFixture selected(true, true, false, true);
        expect(selected.configCalls == 1);
        ColortblFixture missing(false, false, false, true);
        expect(missing.configCalls == 0);
        expect(missing.jax->session().colorModel() == nullptr);
        auto plain = missing.jax->parseOptions().itemFactory->create(StackKind::array);
        expect(dynamic_cast<ColorArrayItem *>(plain.get()) == nullptr);
        missing.configure();
        expect(missing.configCalls == 1);
        expect(missing.jax->session().colorModel() != nullptr);
    };
    "colortbl user color definitions persist across formulas and reset with isolated jaxes"_test =
        [] {
            ColortblFixture f(true);
            for (auto const &c : colortblSequence())
                checkTableTree(f.ok(std::u16string(c.source)), c.snapshot);
            auto model = f.jax->session().colorModel();
            f.jax->reset();
            expect(f.jax->session().colorModel() == model);
            expect(std::ranges::equal(model->getColor(u"named", u"Stored"), u"#ff0c00"sv));
            ColortblFixture other;
            expect(std::ranges::equal(
                other.jax->session().colorModel()->getColor(u"named", u"Stored"), u"Stored"sv));
        };
    "colortbl errors preserve parsing order and recover fresh array colors"_test = [] {
        expect(colortblErrors().size() == 14_u);
        for (auto const &c : colortblErrors()) {
            ColortblFixture f;
            f.compile(std::u16string(c.source));
            expect(f.error == c.id);
            expect(f.jax->parseOptions().request().parsers.empty());
            f.ok(u"\\begin{array}{c}\\cellcolor{blue}x\\end{array}");
        }
    };
    "colortbl array frames apply cell row column precedence and reset only local colors"_test = [] {
        ColortblFixture f;
        withColorArray(f, [&](TexParser &p, ColorArrayItem &a) {
            expect(a.color.cell.empty());
            expect(a.color.row.empty());
            expect(a.color.col.empty());
            expect(!a.hasColor);
            a.color.col.resize(1);
            a.color.col[0] = u"green";
            a.color.row = u"blue";
            a.color.cell = u"red";
            addColorToken(p, a, u"a");
            a.EndEntry();
            expect(std::ranges::equal(attr(*a.row[0], u"mathbackground"), u"red"sv));
            expect(a.color.cell.empty());
            addColorToken(p, a, u"b");
            a.EndEntry();
            expect(std::ranges::equal(attr(*a.row[1], u"mathbackground"), u"blue"sv));
            a.EndRow();
            expect(a.color.row.empty());
            expect(std::ranges::equal(*a.color.col[0], u"green"sv));
            addColorToken(p, a, u"c");
            a.EndEntry();
            expect(std::ranges::equal(attr(*a.row[0], u"mathbackground"), u"green"sv));
            a.EndRow();
            expect(a.hasColor);
            ArrayItem &base = a;
            checkTableTree(base.createMml(), directExpected(u"precedence"));
        });
        withColorArray(f, [&](TexParser &p, ColorArrayItem &a) {
            a.color.col.resize(4);
            a.color.col[2] = u"yellow";
            a.color.col[3] = u"";
            expect(!a.color.col[0]);
            expect(a.color.col[3].has_value());
            for (auto s : {u"a", u"b", u"c"}) {
                addColorToken(p, a, s);
                a.EndEntry();
            }
            a.EndRow();
            ArrayItem &base = a;
            checkTableTree(base.createMml(), directExpected(u"sparse"));
        });
    };
    "colortbl partial row writes empty column presence and conditional overlap consumption"_test =
        [] {
            ColortblFixture f;
            withColorArray(f, [&](TexParser &p, ColorArrayItem &a) {
                addColorToken(p, a, u"x");
                texError([&] { invokeTableColor(p, u"row", u"{blue}"); }, "RowColorNotFirst");
                expect(std::ranges::equal(a.color.row, u"blue"sv));
                expect(p.position() == 6_u);
                a.EndEntry();
                a.EndRow();
                ArrayItem &base = a;
                checkTableTree(base.createMml(), directExpected(u"partialRow"));
            });
            withColorArray(f, [&](TexParser &p, ColorArrayItem &a) {
                a.color.col.resize(1);
                a.color.col[0] = u"";
                addColorToken(p, a, u"a");
                a.EndEntry();
                a.EndRow();
                invokeTableColor(p, u"col", u"{}");
                expect(a.color.col[0].has_value());
                expect(a.color.col[0]->empty());
                expect(!a.hasColor);
                a.color.col[0].reset();
                texError([&] { invokeTableColor(p, u"col", u"{}"); }, "ColumnColorNotTop");
                expect(!a.color.col[0]);
            });
            struct Overlap {
                std::u16string_view target, input, rest;
            };
            for (auto c : std::array{Overlap{u"col", u"{red}[left][right]z", u"z"},
                                     Overlap{u"col", u"{red}[][right]z", u"[right]z"},
                                     Overlap{u"cell", u"{red}[left]z", u"[left]z"},
                                     Overlap{u"row", u"{red}[left]z", u"[left]z"}})
                withColorArray(f, [&](TexParser &p, ColorArrayItem &) {
                    invokeTableColor(p, c.target, c.input);
                    expect(std::ranges::equal(p.string().substr(p.position()), c.rest));
                });
        };
    "colortbl virtual createMml preserves fenced styled and framed array assembly"_test = [] {
        ColortblFixture f;
        using Edge = ArrayFrameRule::Edge;
        for (std::u16string_view mode :
             {u"plain", u"fenced", u"style", u"menclose", u"fullframe", u"mixed"})
            withColorArray(f, [&](TexParser &p, ColorArrayItem &a) {
                a.color.cell = u"red";
                addColorToken(p, a, u"x");
                a.EndEntry();
                a.EndRow();
                if (std::ranges::equal(mode, u"fenced"sv)) {
                    a.open = u"(";
                    a.close = u")";
                }
                if (std::ranges::equal(mode, u"style"sv))
                    a.arraydef.set(u"scriptlevel", 1.0);
                if (std::ranges::equal(mode, u"menclose"sv))
                    a.frame = {{Edge::left, u"solid"}};
                if (std::ranges::equal(mode, u"fullframe"sv))
                    a.frame = {{Edge::top, u"solid"},
                               {Edge::right, u"solid"},
                               {Edge::bottom, u"solid"},
                               {Edge::left, u"solid"}};
                if (std::ranges::equal(mode, u"mixed"sv))
                    a.frame = {{Edge::left, u"solid"}, {Edge::right, u"dashed"}};
                ArrayItem &base = a;
                checkTableTree(base.createMml(), directExpected(mode));
            });
    };
};
} // namespace
