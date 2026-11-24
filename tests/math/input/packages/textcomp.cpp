#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.textcomp.configuration;
import folia.math.input.tex.textcomp.mappings;
import folia.math.input.tex.textmacros.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.base_methods;

namespace {
struct TextcompCase {
    std::u16string_view name, glyph, font;
};
#include "math/fixtures/textcomp.inc"
constexpr std::array<MacroHandler, 2> textcompCallbacks{&TextcompMappings::Insert,
                                                        &BaseMethods::Macro};
thread_local std::string *textcompError{};
MmlNode &formatTextcompError(TeX &jax, TexError const &error) {
    if (textcompError)
        *textcompError = error.id();
    return jax.formatError(error);
}
struct TextcompFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{textcompCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit TextcompFixture(bool text = true, bool aliases = false, bool reversed = false) {
        TextcompConfiguration::initializeModule(registry, maps);
        TextMacrosConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (text && !reversed)
            options.packages->push_back({u"textmacros", {}});
        options.packages->push_back({u"textcomp", {}});
        if (text && reversed)
            options.packages->push_back({u"textmacros", {}});
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        options.formatError = &formatTextcompError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{textcompError};
            ~Restore() {
                textcompError = old;
            }
        } restore;
        textcompError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        auto &root = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        if (auto text = jax->parseOptions().textmacros) {
            expect(text->parseOptions->request().texParser == nullptr);
            expect(text->parseOptions->request().parsers.empty());
        }
        return root;
    }
    MmlNode &ok(std::u16string source) {
        auto &root = compile(std::move(source));
        expect(error.empty()) << error;
        return root;
    }
};
std::vector<MmlNode *> textcompTexts(MmlNode &root) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == NodeKind::mtext)
            result.push_back(&node);
    });
    return result;
}
struct TextcompRequest {
    NodeDefinition definition;
    MmlNode *text{};
};
thread_local std::vector<TextcompRequest> *textcompRequests{};
MmlNode &observeTextcomp(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                         NodeDefinition definition, MmlNode *text) {
    if (textcompRequests && kind == NodeKind::mtext)
        textcompRequests->push_back({definition, text});
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
} // namespace

suite textcomp = [] {
    "textcomp all 117 original map arguments and math or text symbols match the independent Source table"_test =
        [] {
            TextcompFixture text, math(false);
            auto map = text.maps.getMap(u"textcomp-macros");
            expect(std::size(textcompCases) == 117_u);
            for (auto const &entry : textcompCases) {
                auto macro = std::get<Macro const *>(map->lookup(entry.name));
                expect(std::get<MacroHandler>(macro->function) == &TextcompMappings::Insert);
                expect(macro->arguments.size() ==
                       (entry.font.empty() ? std::size_t{1} : std::size_t{2}));
                expect(
                    std::ranges::equal(std::get<std::u16string>(macro->arguments[0]), entry.glyph));
                if (!entry.font.empty() && macro->arguments.size() > 1)
                    expect(std::ranges::equal(std::get<std::u16string>(macro->arguments[1]),
                                              entry.font));
                for (auto const &[fixture, source] :
                     {std::pair<TextcompFixture *, std::u16string>{
                          &math, u"\\" + std::u16string(entry.name)},
                      {&text, u"\\text{\\" + std::u16string(entry.name) + u"}"}}) {
                    auto texts = textcompTexts(fixture->ok(source));
                    expect(texts.size() == 1_u);
                    if (!texts.empty()) {
                        expect(std::ranges::equal(NodeUtil::getText(*texts[0]), entry.glyph));
                        if (!entry.font.empty())
                            expect(std::ranges::equal(attr(*texts[0], u"mathvariant"), entry.font));
                    }
                }
            }
        };
    "textcomp text inserts coalesce the current text buffer and keep font-specific segments separate"_test =
        [] {
            TextcompFixture f;
            for (auto const &entry : textcompCases) {
                auto texts =
                    textcompTexts(f.ok(u"\\text{L\\" + std::u16string(entry.name) + u" R}"));
                expect(texts.size() == (entry.font.empty() ? std::size_t{1} : std::size_t{3}));
                if (entry.font.empty() && texts.size() == 1)
                    expect(std::ranges::equal(NodeUtil::getText(*texts[0]),
                                              u"L" + std::u16string(entry.glyph) + u"R"));
                if (!entry.font.empty() && texts.size() == 3) {
                    expect(NodeUtil::getText(*texts[0]) == u"L");
                    expect(std::ranges::equal(NodeUtil::getText(*texts[1]), entry.glyph));
                    expect(NodeUtil::getText(*texts[2]) == u"R");
                    expect(std::ranges::equal(attr(*texts[1], u"mathvariant"), entry.font));
                }
            }
            auto data = f.jax->parseOptions().textmacros;
            expect(data != nullptr);
            if (data)
                expect(data->parseOptions->options.textmacros->packages->size() == 2_u);
        };
    "textcomp parser-specific registration works in either package order and without a text parser"_test =
        [] {
            TextcompFixture f(true, false, true), math(false);
            expect(f.registry.get(u"text-textcomp")->parser == u"text");
            expect(math.jax->parseOptions().textmacros == nullptr);
            auto texts = textcompTexts(f.ok(u"\\text{\\texteuro}"));
            expect(texts.size() == 1_u);
            if (!texts.empty())
                expect(NodeUtil::getText(*texts[0]) == u"\u20AC");
        };
    "textcomp real creator receives explicit oldstyle definition and the final text child borrow"_test =
        [] {
            TextcompFixture f;
            std::vector<TextcompRequest> requests;
            struct Restore {
                ~Restore() {
                    textcompRequests = nullptr;
                }
            } restore;
            textcompRequests = &requests;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.node = &observeTextcomp});
            auto texts = textcompTexts(f.ok(u"\\textoneoldstyle"));
            expect(requests.size() == 1_u && texts.size() == 1_u);
            if (requests.size() == 1 && texts.size() == 1) {
                expect(attr(requests[0].definition.attributes, u"mathvariant") == u"-tex-oldstyle");
                expect(texts[0]->children().size() == 1_u);
                if (!texts[0]->children().empty())
                    expect(requests[0].text == texts[0]->children()[0]);
            }
        };
    "textcomp aliases preserve math use while text fallback only admits string replacement macros"_test =
        [] {
            TextcompFixture f(true, true);
            f.ok(u"\\let\\saved\\texteuro\\newcommand{\\curr}{\\texteuro}");
            for (auto source : {u"\\saved", u"\\text{\\curr}"}) {
                auto texts = textcompTexts(f.ok(source));
                expect(texts.size() == 1_u);
                if (!texts.empty())
                    expect(NodeUtil::getText(*texts[0]) == u"\u20AC");
            }
            f.compile(u"\\text{\\saved}");
            expect(f.error == "MathMacro");
            f.ok(u"\\text{\\texteuro}");
        };
};

namespace {
struct TextcompLiveOwnerProbe { ParseOptions* outer; TeX* jax; std::size_t calls{}; };
thread_local TextcompLiveOwnerProbe* textcompLiveOwner{};
MmlNode& observeTextcompLiveOwner(NodeFactory& factory, NodeKind kind,
    std::span<MmlNode* const> children, NodeDefinition definition, MmlNode* text) {
    if (textcompLiveOwner && kind == NodeKind::mtext) {
        auto& probe = *textcompLiveOwner;
        auto& inner = dynamic_cast<ParseOptions&>(factory.configuration());
        auto* primary = inner.request().texParser;
        expect(primary != nullptr);
        if (primary) {
            expect(&primary->configuration == probe.outer);
            expect(primary == probe.outer->parser());
        }
        expect(probe.outer->request().mathItem != nullptr);
        if (probe.outer->request().mathItem)
            expect(probe.outer->request().mathItem->inputJax == probe.jax);
        ++probe.calls;
    }
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
}
suite textcomp_compile_owner_lifetime = [] {
    "textcomp_compile_borrows_current_jax_after_an_uncompiled_warmup"_test = [] {
        for (bool warmupFirst : {false, true}) {
            TextcompFixture f;
            std::unique_ptr<TeX> warmup;
            if (warmupFirst) {
                TexOptionValues options;
                options.packages = std::vector<PackageSelection>{
                    {u"base", {}}, {u"textmacros", {}}, {u"textcomp", {}}};
                f.request.reset();
                warmup = std::move(f.jax);
                f.jax = std::make_unique<TeX>(f.registry, f.maps, f.callbacks, f.warnings, options);
                f.request = std::make_unique<TeXRequest>(*f.jax, f.document.factory());
            }
            auto& outer = f.jax->parseOptions();
            auto* text = outer.textmacros;
            expect(text != nullptr);
            if (!text) continue;
            auto& inner = *text->parseOptions;
            expect(&inner != &outer);
            expect(&inner.options == &f.jax->options);
            expect(&inner.packageData == &outer.packageData);
            if (warmup) expect(warmup->parseOptions().textmacros != text);
            TextcompLiveOwnerProbe probe{&outer, f.jax.get()};
            struct RestoreProbe { ~RestoreProbe() { textcompLiveOwner = nullptr; } } restore;
            textcompLiveOwner = &probe;
            inner.setNodeCreators(NodeCreators{.node = &observeTextcompLiveOwner});
            for (int repeat = 0; repeat < 2; ++repeat) {
                auto& root = f.ok(u"\\text{\\textcopyright\\textregistered\\texttrademark}");
                auto texts = textcompTexts(root);
                std::u16string actual;
                for (auto* node : texts) actual += NodeUtil::getText(*node);
                expect(actual == u"©®™");
                expect(outer.request().root == &root);
                expect(outer.request().parsers.empty() && inner.request().parsers.empty());
                expect(inner.request().texParser == nullptr);
                expect(probe.calls > 0_u);
            }
        }
    };
};
