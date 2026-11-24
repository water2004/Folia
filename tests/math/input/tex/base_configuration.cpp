#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.base_configuration;
import folia.math.input.tex.filter_util;

namespace {
using folia::math::regexp_host::RegExp;
struct BaseFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    explicit BaseFixture(TexOptionValues supplied = {}) {
        BasePackage::initialize(registry, maps);
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, std::move(supplied));
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &parse(std::u16string source, TexEnvironment seed = {}) {
        TexParser parser(std::move(source), ParserEnvironmentSeed{std::move(seed)},
                         jax->parseOptions());
        auto root = parser.mml();
        if (!root)
            throw std::runtime_error("Expected actual parser MML");
        return *root;
    }
    MmlNode &compile(std::u16string source, bool display = false) {
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(display);
        return jax->compile(math, document);
    }
};
std::vector<MmlNode *> all(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
std::u16string content(MmlNode &root) {
    if (root.isToken())
        return NodeUtil::getText(root);
    std::u16string result;
    for (auto child : root.children())
        if (child)
            result += content(*child);
    return result;
}
} // namespace

suite base_configuration = [] {
    "base configuration registers original handler order items tags and processor priority"_test =
        [] {
            BaseFixture f;
            auto package = f.registry.get(u"base");
            expect(package != nullptr);
            expect(package->name == u"base");
            expect(package->items.size() == 26_u);
            auto macro =
                std::find_if(package->handler.begin(), package->handler.end(),
                             [](auto const &entry) { return entry.kind == HandlerKind::macro; });
            const std::vector<std::u16string> expected{u"delimiter", u"macros",      u"lcGreek",
                                                       u"ucGreek",   u"mathchar0mi", u"mathchar0mo",
                                                       u"mathchar7"};
            expect(macro->maps == expected);
            expect(package->postprocessors.size() == 1_u);
            expect(package->postprocessors.front().priority == -4.0);
            expect(package->options.maxMacros.value() == 1000_u);
            expect(f.jax->configuration()
                       .handlers.get(HandlerKind::character)
                       ->applicable(u"x")
                       ->name() == u"letter");
            expect(f.jax->configuration()
                       .handlers.get(HandlerKind::character)
                       ->applicable(u"2")
                       ->name() == u"digit");
            auto tag = package->tags.front().create(f.jax->parseOptions());
            expect(dynamic_cast<BaseTags *>(tag.get()) != nullptr);
            auto formatted = tag->formatRef(u"3");
            expect(std::get<std::vector<std::u16string>>(formatted) ==
                   std::vector<std::u16string>{u"(", u"3", u")"});
        };
    "base configuration actual TeX default numbers remapping and undefined errors"_test = [] {
        BaseFixture f;
        auto &parsed = f.parse(u"12{,}345.6 .7 , -*`");
        auto numbers = all(parsed, NodeKind::mn);
        expect(numbers.size() == 2_u);
        expect(NodeUtil::getText(*numbers[0]) == u"12,345.6");
        expect(NodeUtil::getText(*numbers[1]) == u".7");
        expect(content(parsed).find(u"−∗‘") != std::u16string::npos);
        auto operators = all(parsed, NodeKind::mo);
        expect(std::any_of(operators.begin(), operators.end(),
                           [](auto node) { return node->properties().tex.fix_stretchy == true; }));
        texError([&] { f.parse(u"\\unknowncs"); }, "UndefinedControlSequence");
        texError([&] { f.parse(u"\\begin{unknownenv}"); }, "UnknownEnv");
        auto &error = f.compile(u"\\unknowncs");
        expect(all(error, NodeKind::merror).size() == 1_u);
        expect(content(error).find(u"Undefined control sequence \\unknowncs") !=
               std::u16string::npos);
    };
    "base configuration Other takes dictionary variants and configured font precedence"_test = [] {
        BaseFixture f;
        TexEnvironment seed;
        seed.font = u"bold";
        auto &root = f.parse(u"中-", seed);
        auto ids = all(root, NodeKind::mi), ops = all(root, NodeKind::mo);
        expect(ids.size() == 1_u);
        expect(attr(*ids[0], u"mathvariant") == u"normal");
        expect(attr(*ops[0], u"mathvariant") == u"bold");
        TexOptionValues options;
        options.initialLetter = std::make_shared<RegExp>(u"z");
        BaseFixture forced(options);
        TexEnvironment italic;
        italic.italic_font = u"bold-italic";
        auto &greek = forced.parse(u"α", italic);
        auto letters = all(greek, NodeKind::mi);
        expect(letters.size() == 1_u);
        expect(attr(*letters[0], u"mathvariant") == u"bold-italic");
        italic.font = u"bold";
        auto &font = forced.parse(u"Γ", italic);
        auto withFont = all(font, NodeKind::mi);
        expect(attr(*withFont[0], u"mathvariant") == u"bold");
    };
    "base configuration configures digit patterns and letter digit dispatch maps"_test = [] {
        for (bool useString : {false, true}) {
            TexOptionValues options;
            options.initialLetter = std::make_shared<RegExp>(u"[x-z]");
            options.initialDigit = std::make_shared<RegExp>(u"[A-F]");
            auto expression = std::make_shared<RegExp>(u"^[A-F]+");
            if (useString)
                options.digits = std::u16string(u"^[A-F]+");
            else
                options.digits = expression;
            BaseFixture f(options);
            auto &root = f.parse(u"ABCx");
            auto numbers = all(root, NodeKind::mn);
            expect(numbers.size() == 1_u);
            expect(NodeUtil::getText(*numbers[0]) == u"ABC");
            expect(all(root, NodeKind::mi).size() == 1_u);
            if (!useString)
                expect(f.jax->parseOptions().options.numberPattern.value() == expression);
            expect(f.jax->configuration()
                       .handlers.get(HandlerKind::character)
                       ->applicable(u"A")
                       ->name() == u"digit");
        }
    };
    "base configuration nonscript removes wrapped and direct script spaces and retains textstyle spaces"_test =
        [] {
            BaseFixture f;
            auto &scripts = f.compile(u"x_{a\\nonscript\\,b\\nonscript\\hskip 2pt c}");
            expect(all(scripts, NodeKind::mspace).empty());
            auto &text = f.compile(u"a\\nonscript\\,b\\nonscript\\hskip 2pt c");
            auto spaces = all(text, NodeKind::mspace);
            expect(spaces.size() == 2_u);
            expect(std::any_of(spaces.begin(), spaces.end(),
                               [](auto node) { return attr(*node, u"width") == u"2pt"; }));
            expect(f.jax->parseOptions().getList(u"mspace").size() == 2_u);
        };
    "base configuration nonscript postprocessor updates original node kind lists"_test = [] {
        BaseFixture f;
        auto &options = f.jax->parseOptions();
        auto &factory = options.request().nodeFactory;
        auto &space = factory.makeNode(NodeKind::mspace);
        std::array spaceChildren{&space};
        auto &style = factory.makeNode(NodeKind::mstyle, spaceChildren);
        std::array styleChildren{&style};
        auto &wrapped = factory.makeNode(NodeKind::mrow, styleChildren);
        auto &direct = factory.makeNode(NodeKind::mspace);
        std::array rootChildren{&wrapped, &direct};
        auto &root = factory.makeNode(NodeKind::math, rootChildren);
        options.request().root = &root;
        wrapped.attributes().set(u"scriptlevel", 1.0);
        direct.attributes().set(u"scriptlevel", 1.0);
        options.addNode(u"nonscript", wrapped);
        options.addNode(u"nonscript", direct);
        folia::math::core::AbstractMathItem math(u"", f.jax.get());
        TexFilterData data{math, f.document, options};
        auto package = f.registry.get(u"base");
        auto callback = std::get<FilterList::VoidCallback>(package->postprocessors.front().method);
        callback(data);
        expect(all(root, NodeKind::mspace).empty());
        expect(options.request().nodeLists[u"mspace"].empty());
        expect(options.request().nodeLists[u"mstyle"].empty());
        expect(options.request().nodeLists[u"mrow"].empty());
    };
};
