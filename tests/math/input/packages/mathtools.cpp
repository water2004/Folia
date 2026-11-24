#include "math/support/tex_fixture.hpp"
#include "math/support/fixture_values.hpp"
import folia.math.input.tex.mathtools.configuration;
import folia.math.input.tex.ams.configuration;
import folia.math.input.tex.mathtools.util;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.boldsymbol.configuration;
import folia.math.input.tex.extpfeil.configuration;
import folia.math.util.utf16;
namespace {
using namespace std::literals;
constexpr std::array<MacroHandler, 5> fullCallbacks{
    &BaseMethods::Macro, &AmsMethods::HandleDeclareOp, &NewcommandMethods::MacroWithTemplate,
    &MathtoolsMethods::PairedDelimiters, &MathtoolsMethods::Aboxed};
thread_local std::string *fullError{};
MmlNode &fullFormatError(TeX &jax, TexError const &error) {
    if (fullError)
        *fullError = error.id();
    return jax.formatError(error);
}
struct MathtoolsFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{fullCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit MathtoolsFixture(TexOptionValues options = {}, std::optional<bool> reverseOrder = {}) {
        AmsConfiguration::initializeModule(registry, maps);
        NewcommandConfiguration::initializeModule(registry, maps);
        MathtoolsConfiguration::initializeModule(registry, maps);
        BoldsymbolConfiguration::initializeModule(registry, maps);
        ExtpfeilConfiguration::initializeModule(registry, maps);
        options.packages = std::vector<PackageSelection>{{u"base", {}},
                                                         {u"ams", {}},
                                                         {u"newcommand", {}},
                                                         {u"mathtools", {}},
                                                         {u"boldsymbol", {}}};
        if (reverseOrder)
            options.packages->insert(options.packages->begin() + (*reverseOrder ? 4 : 3),
                                     {u"extpfeil", {}});
        options.tags = u"ams";
        options.maxMacros = 1000;
        options.formatError = &fullFormatError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        const auto diagnostic = folia::math::utf16::encode(source);
        error.clear();
        struct Restore {
            std::string *old{fullError};
            ~Restore() {
                fullError = old;
            }
        } restore;
        fullError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(true);
        try {
            auto &tree = jax->compile(math, document);
            expect(jax->parseOptions().request().parsers.empty());
            return tree;
        } catch (std::exception const &e) {
            throw std::runtime_error(diagnostic + ": " + e.what());
        }
    }
    MmlNode &ok(std::u16string source) {
        auto &tree = compile(std::move(source));
        expect(error.empty()) << error;
        return tree;
    }
};
std::u16string fullNumber(std::size_t n) {
    auto s = std::to_string(n);
    return {s.begin(), s.end()};
}
std::u16string fullSnapshot(MmlNode const &root) {
    std::u16string out;
    auto add = [&](std::u16string_view s) {
        out += fullNumber(s.size()) + u":" + std::u16string(s);
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
        add(fullNumber(attrs.size()));
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
        add(fullNumber(node->isToken() ? 0 : node->children().size()));
        if (!node->isToken())
            for (auto child : node->children())
                self(self, child);
    };
    visit(visit, &root);
    return out;
}
void checkFullTree(MmlNode &root, std::u16string_view expected) {
    auto actual = fullSnapshot(root);
    if (!std::ranges::equal(actual, expected)) {
        auto mismatch = std::ranges::mismatch(actual, expected);
        auto offset = static_cast<std::size_t>(mismatch.in1 - actual.begin());
        auto start = offset > 40 ? offset - 40 : 0;
        expect(false) << "Mathtools Source difference at " << offset << " actual "
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
struct MathtoolsTree {
    std::u16string source, snapshot;
};
struct MathtoolsError {
    std::u16string source;
    std::string id;
};
struct MathtoolsStep {
    int group;
    std::u16string source, snapshot;
    std::string error;
};
struct MathtoolsOrder {
    bool reverse;
    std::u16string source, snapshot;
};
const auto& mathtoolsTrees() {
    static const auto result = math_test::fixtures::records<MathtoolsTree>(
        "math/fixtures/packages/mathtools/trees.tsv", 2, 218, [](const auto& r) {
            return MathtoolsTree{folia::test::data::decode_utf16(r[0]), folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

const auto& mathtoolsErrors() {
    static const auto result = math_test::fixtures::records<MathtoolsError>(
        "math/fixtures/packages/mathtools/errors.tsv", 2, 61, [](const auto& r) {
            return MathtoolsError{folia::test::data::decode_utf16(r[0]), math_test::fixtures::bytes(r[1])};
        });
    return result;
}

const auto& mathtoolsSteps() {
    static const auto result = math_test::fixtures::records<MathtoolsStep>(
        "math/fixtures/packages/mathtools/steps.tsv", 4, 37, [](const auto& r) {
            return MathtoolsStep{math_test::fixtures::integer<int>(r[0]), folia::test::data::decode_utf16(r[1]), folia::test::data::decode_utf16(r[2]), math_test::fixtures::bytes(r[3])};
        });
    return result;
}

const auto& mathtoolsOrder() {
    static const auto result = math_test::fixtures::records<MathtoolsOrder>(
        "math/fixtures/packages/mathtools/order.tsv", 3, 4, [](const auto& r) {
            return MathtoolsOrder{math_test::fixtures::boolean(r[0]), folia::test::data::decode_utf16(r[1]), folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

} // namespace
suite mathtools = [] {
    "mathtools package registers original handlers factory defaults aliases and prescript filter"_test =
        [] {
            MathtoolsFixture f;
            auto config = f.registry.get(u"mathtools");
            expect(config->config() == &configMathtools);
            expect(config->configMethod->priority == 5.);
            expect(config->postprocessors.size() == 1_u);
            expect(config->postprocessors[0].priority == -6.);
            expect(std::get<FilterList::VoidCallback>(config->postprocessors[0].method) ==
                   &fixPrescripts);
            expect(config->options.mathtools->multlinedGap.value() == u"1em");
            expect(config->options.mathtools->allowMathtoolsset.value());
            expect(config->options.mathtools->pairedDelimiters->empty());
            expect(config->options.mathtools->tagforms->empty());
            for (auto name :
                 {u"mathtools-macros", u"mathtools-legacycolonsymbols", u"mathtools-environments",
                  u"mathtools-delimiters", u"mathtools-characters"})
                expect(f.maps.getMap(name) != nullptr);
            expect(MathtoolsMethods::Array == &BaseMethods::Array);
            expect(MathtoolsMethods::Macro == &BaseMethods::Macro);
            expect(MathtoolsMethods::xArrow == &AmsMethods::xArrow);
            expect(MathtoolsMethods::HandleRef == AmsMethods::HandleRef);
            expect(MathtoolsMethods::AmsEqnArray == &AmsMethods::AmsEqnArray);
            expect(MathtoolsMethods::MacroWithTemplate == &NewcommandMethods::MacroWithTemplate);
            auto product = f.jax->parseOptions().itemFactory->create(StackKind::multlined,
                                                                     MultlinedItemInit{});
            expect(dynamic_cast<MultlinedItem *>(product.get()) != nullptr);
            expect(product->kind() == StackKind::multlined);
            dynamic_cast<MultlinedItem &>(*product).EndTable();
        };
    "mathtools package mathematical families match pinned Source MML"_test = [] {
        for (auto const &row : mathtoolsTrees()) {
            MathtoolsFixture f;
            auto &root = f.ok(std::u16string(row.source));
            checkFullTree(root, row.snapshot);
        }
    };
    "mathtools package Source errors leave next formula usable"_test = [] {
        for (auto const &row : mathtoolsErrors()) {
            MathtoolsFixture f;
            f.compile(std::u16string(row.source));
            expect(f.error == row.id) << folia::math::utf16::encode(row.source) << " actual "
                                      << f.error << " expected " << row.id;
            f.ok(u"x");
        }
    };
    "mathtools package Source sequences preserve definitions options and tag forms"_test = [] {
        std::unique_ptr<MathtoolsFixture> fixture;
        int current = -1;
        for (auto const &row : mathtoolsSteps()) {
            if (!fixture || current != row.group) {
                fixture = std::make_unique<MathtoolsFixture>();
                current = row.group;
            }
            auto &root = fixture->compile(std::u16string(row.source));
            expect(fixture->error == row.error)
                << folia::math::utf16::encode(row.source) << " actual " << fixture->error
                << " expected " << row.error;
            if (row.error.empty())
                checkFullTree(root, row.snapshot);
        }
    };
    "mathtools package preserves extpfeil override order in actual Source MML"_test = [] {
        for (auto const &row : mathtoolsOrder()) {
            MathtoolsFixture f({}, row.reverse);
            checkFullTree(f.ok(std::u16string(row.source)), row.snapshot);
        }
    };
    "mathtools package rejects untyped option replacement at its C++ boundary"_test = [] {
        for (auto source :
             {u"\\mathtoolsset{multlined-pos=true}", u"\\mathtoolsset{smallmatrix-align=true}",
              u"\\mathtoolsset{pairedDelimiters=value}"}) {
            MathtoolsFixture f;
            f.compile(source);
            expect(f.error == "InvalidValue");
            f.ok(u"x");
        }
        TexOptionValues options;
        options.mathtools.emplace().allowMathtoolsset = false;
        MathtoolsFixture f(options);
        f.compile(u"\\mathtoolsset");
        expect(f.error == "ForbiddenMathtoolsSet");
    };
    "mathtools package configured paired delimiters are actual persistent native aliases"_test =
        [] {
            TexOptionValues options;
            options.mathtools.emplace().pairedDelimiters.emplace().emplace(
                u"abs", PairedDelimiterOptions{.left = u"|", .right = u"|"});
            options.mathtools->tagforms.emplace().emplace(u"box",
                                                          MathtoolsTags::Format{u"[", u"]", u""});
            MathtoolsFixture f(options);
            auto definition = f.jax->session().findCommand(u"abs");
            expect(definition != nullptr);
            if (definition) {
                auto alias = std::get_if<NativeMacroAlias>(&definition->body);
                expect(alias != nullptr);
                if (alias)
                    expect(f.callbacks.macroHandler(alias->handler) ==
                           &MathtoolsMethods::PairedDelimiters);
            }
            f.ok(u"\\abs{x}");
            f.ok(u"\\abs*{y}");
            auto &forms = dynamic_cast<MathtoolsTags &>(*f.jax->parseOptions().tags->tagForm());
            expect(forms.mtFormats.contains(u"box"));
            f.ok(u"\\begin{equation}\\usetagform{box}x\\end{equation}");
            MathtoolsFixture independent;
            expect(independent.jax->session().findCommand(u"abs") == nullptr);
        };
    "mathtools package prescript postfilter fills only missing postscript slots"_test = [] {
        MathtoolsFixture f;
        auto &options = f.jax->parseOptions();
        auto &factory = options.request().nodeFactory;
        TexParser active(u"", {}, options);
        for (int present = 0; present < 4; ++present) {
            options.request().nodeLists.clear();
            auto &base = factory.makeToken(NodeKind::mi, u"x");
            std::array<MmlNode *, 1> first{&base};
            auto &node = factory.makeNode(NodeKind::mmultiscripts, first);
            auto &marker = factory.makeNode(NodeKind::mprescripts);
            NodeUtil::setChild(node, 1,
                               (present & 1) ? &factory.makeToken(NodeKind::mi, u"a") : nullptr);
            NodeUtil::setChild(node, 2,
                               (present & 2) ? &factory.makeToken(NodeKind::mi, u"b") : nullptr);
            NodeUtil::setChild(node, 3, marker);
            NodeUtil::setChild(node, 4, factory.makeNode(NodeKind::none));
            NodeUtil::setChild(node, 5, factory.makeNode(NodeKind::none));
            node.properties().tex.fix_prescript = true;
            options.request().root = &node;
            f.document.setRoot(node);
            folia::math::core::AbstractMathItem math(u"", f.jax.get());
            TexFilterData data{math, f.document, options};
            fixPrescripts(data);
            if (present == 0) {
                expect(node.children().size() == 4_u);
                expect(node.children()[1] == &marker);
            } else {
                expect(node.children().size() == 6_u);
                expect(node.children()[3] == &marker);
                if (!(present & 1))
                    expect(node.children()[1]->kind() == NodeKind::none);
                if (!(present & 2))
                    expect(node.children()[2]->kind() == NodeKind::none);
            }
        }
        active.mml();
    };
};
