#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.filter_util;

namespace {
struct JaxFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    void configure(TexOptionValues supplied = {}, bool bindDefaultDocument = true) {
        request.reset();
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, std::move(supplied));
        if (bindDefaultDocument)
            bind(document);
    }
    void bind(MmlDocument &target) {
        request.reset();
        request = std::make_unique<TeXRequest>(*jax, target.factory());
    }
    MmlNode &compile(std::u16string text, bool display = true) {
        if (!request)
            bind(document);
        folia::math::core::AbstractMathItem math(std::move(text), jax.get());
        math.setDisplay(display);
        return jax->compile(math, document);
    }
};
std::vector<MmlNode *> findNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
std::u16string tokenText(MmlNode &root) {
    std::u16string text;
    root.walkTree([&](auto &node) {
        if (node.isToken())
            text += NodeUtil::getText(node);
    });
    return text;
}
void mark(TexFilterData &data, std::u16string_view step) {
    auto &trace = data.data.options.baseURL;
    trace = trace.value_or(u"") + std::u16string(step);
}
void userPre(TexFilterData &data) {
    mark(data, u"user-pre;");
    data.math.math = u"x^2";
}
void configuredPre(TexFilterData &data) {
    mark(data, u"config-pre;");
}
void packagePre(TexFilterData &data) {
    mark(data, u"package-pre;");
}
void userPost(TexFilterData &data) {
    mark(data, u"user-post;");
}
void packagePost(TexFilterData &data) {
    mark(data, u"package-post;");
}
void configurePackage(ParserConfiguration &, TeX &jax) {
    jax.options.baseURL = jax.options.baseURL.value_or(u"") + u"config;";
    jax.preFilters.add(configuredPre);
}
void failPre(TexFilterData &) {
    throw std::runtime_error("preprocessor exception");
}
bool stopPost(TexFilterData &data) {
    mark(data, u"stop;");
    return false;
}
MmlNode &customError(TeX &jax, TexError const &error) {
    return jax.parseOptions().request().nodeFactory.makeToken(
        NodeKind::mtext, std::u16string(error.message()).substr(0, 10));
}
void dynamicInit(ParserConfiguration &config) {
    config.options.maxMacros = config.options.maxMacros.value_or(0) + 1;
}
void dynamicConfig(ParserConfiguration &, TeX &jax) {
    jax.options.baseURL = jax.options.baseURL.value_or(u"") + u"dynamic;";
}
MmlNode &dynamicNode(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                     NodeDefinition definition, MmlNode *text) {
    definition.attributes.set(u"data-created", u"dynamic");
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
struct CompileOutput final : folia::math::core::MathOutputJaxService {
    bool *destroyed{};
    explicit CompileOutput(bool *flag = nullptr) : destroyed(flag) {}
    ~CompileOutput() override {
        if (destroyed)
            *destroyed = true;
    }
    folia::math::output::scene::Scene typeset(folia::math::core::MathItem &,
                                              MmlDocument &) override {
        throw std::logic_error("Unexpected typeset in compile borrow test");
    }
    folia::math::output::scene::Scene escaped(folia::math::core::MathItem &,
                                              MmlDocument &) override {
        throw std::logic_error("Unexpected escaped output in compile borrow test");
    }
    folia::math::BBox getBBox(folia::math::core::MathItem &, MmlDocument &) override {
        throw std::logic_error("Unexpected measurement in compile borrow test");
    }
};
struct CompileObservation {
    folia::math::core::MathOutputJaxService *output;
    MmlDocument *document;
    folia::math::core::MathItem *math;
    bool post;
};
struct CompileProbe {
    std::vector<CompileObservation> observations;
    bool failPre{}, failPost{};
    TeX *nestedJax{};
    MmlDocument *nestedDocument{};
    folia::math::core::MathOutputJaxService *nestedOutput{};
};
thread_local CompileProbe *compileProbe{};
struct ObserveCompilation {
    CompileProbe *previous{compileProbe};
    explicit ObserveCompilation(CompileProbe &probe) {
        compileProbe = &probe;
    }
    ~ObserveCompilation() {
        compileProbe = previous;
    }
};
void observeCompilePre(TexFilterData &data) {
    auto &probe = *compileProbe;
    probe.observations.push_back({data.output, &data.document, &data.math, false});
    if (probe.failPre)
        throw std::runtime_error("Compile prefilter failed");
    if (probe.nestedJax && &data.document != probe.nestedDocument) {
        folia::math::core::AbstractMathItem nested(u"y", probe.nestedJax);
        nested.compile(*probe.nestedDocument, {probe.nestedOutput});
    }
}
void observeCompilePost(TexFilterData &data) {
    auto &probe = *compileProbe;
    probe.observations.push_back({data.output, &data.document, &data.math, true});
    if (probe.failPost)
        throw std::runtime_error("Compile postfilter failed");
}
} // namespace

suite tex_jax = [] {
    "TeX compile borrows output through both filter phases and does not retain it"_test = [] {
        JaxFixture f;
        f.configure();
        f.jax->preFilters.add(observeCompilePre);
        f.jax->postFilters.add(observeCompilePost);
        CompileProbe probe;
        ObserveCompilation observer(probe);
        bool destroyed = false;
        {
            CompileOutput output(&destroyed);
            folia::math::core::AbstractMathItem math(u"x", f.jax.get());
            math.compile(f.document, {&output});
            expect(probe.observations.size() == 2_u);
            for (auto const &observation : probe.observations) {
                expect(observation.output == &output);
                expect(observation.document == &f.document);
                expect(observation.math == &math);
            }
            expect(!probe.observations[0].post);
            expect(probe.observations[1].post);
            expect(math.root == f.jax->parseOptions().request().root);
        }
        expect(destroyed) << "TeX holds no ownership of the compile output service";
        probe.observations.clear();
        folia::math::core::AbstractMathItem next(u"z", f.jax.get());
        next.compile(f.document);
        expect(probe.observations.size() == 2_u);
        for (auto const &observation : probe.observations)
            expect(observation.output == nullptr);
        expect(!findNodes(*next.root, NodeKind::mi).empty());
    };
    "TeX compile output borrows survive error formatting and unwind after filter errors"_test = [] {
        JaxFixture f;
        f.configure();
        f.jax->preFilters.add(observeCompilePre);
        f.jax->postFilters.add(observeCompilePost);
        CompileOutput output;
        CompileProbe probe;
        ObserveCompilation observer(probe);
        folia::math::core::AbstractMathItem invalid(u"\\notACommand", f.jax.get());
        invalid.compile(f.document, {&output});
        expect(findNodes(*invalid.root, NodeKind::merror).size() == 1_u);
        expect(probe.observations.size() == 2_u);
        for (auto const &observation : probe.observations)
            expect(observation.output == &output);
        for (bool failBeforeParsing : {true, false}) {
            probe.observations.clear();
            probe.failPre = failBeforeParsing;
            probe.failPost = !failBeforeParsing;
            folia::math::core::AbstractMathItem failed(u"x", f.jax.get());
            expect(throws<std::runtime_error>([&] { failed.compile(f.document, {&output}); }));
            expect(failed.root == nullptr);
            expect(failed.state() == folia::math::core::STATE.UNPROCESSED);
            expect(probe.observations.size() == std::size_t(failBeforeParsing ? 1 : 2));
            for (auto const &observation : probe.observations)
                expect(observation.output == &output);
            expect(f.jax->parseOptions().request().parsers.empty());
            probe.failPre = probe.failPost = false;
            probe.observations.clear();
            folia::math::core::AbstractMathItem recovered(u"y", f.jax.get());
            recovered.compile(f.document);
            expect(probe.observations.size() == 2_u);
            for (auto const &observation : probe.observations)
                expect(observation.output == nullptr);
            expect(recovered.state() == folia::math::core::STATE.COMPILED);
        }
    };
    "TeX nested compilation keeps each invocation output borrow and document"_test = [] {
        JaxFixture outer, inner;
        outer.configure();
        inner.configure();
        for (auto *jax : {outer.jax.get(), inner.jax.get()}) {
            jax->preFilters.add(observeCompilePre);
            jax->postFilters.add(observeCompilePost);
        }
        CompileOutput outerOutput, innerOutput;
        CompileProbe probe;
        probe.nestedJax = inner.jax.get();
        probe.nestedDocument = &inner.document;
        probe.nestedOutput = &innerOutput;
        ObserveCompilation observer(probe);
        folia::math::core::AbstractMathItem math(u"x", outer.jax.get());
        math.compile(outer.document, {&outerOutput});
        expect(probe.observations.size() == 4_u);
        const std::array expectedOutputs{&outerOutput, &innerOutput, &innerOutput, &outerOutput};
        const std::array expectedDocuments{&outer.document, &inner.document, &inner.document,
                                           &outer.document};
        const std::array expectedPhases{false, false, true, true};
        for (std::size_t i = 0; i < probe.observations.size(); ++i) {
            expect(probe.observations[i].output == expectedOutputs[i]);
            expect(probe.observations[i].document == expectedDocuments[i]);
            expect(probe.observations[i].post == expectedPhases[i]);
        }
        expect(!findNodes(*math.root, NodeKind::mi).empty());
        expect(outer.jax->parseOptions().request().parsers.empty());
        expect(inner.jax->parseOptions().request().parsers.empty());
    };
    "TeX input jax compiles base syntax through all original postfilters"_test = [] {
        JaxFixture f;
        f.configure();
        expect(f.jax->name() == "TeX");
        expect(f.jax->processStrings());
        expect(f.jax->options.maxBuffer.value() == 5120_u);
        expect(f.jax->options.maxTemplateSubtitutions.value() == 10000_u);
        expect(f.jax->postFilters.entries().size() == 8_u);
        const std::array<double, 8> priorities{-7, -6, -5, -4, -4, -3, -2, -1};
        const auto filters = f.jax->postFilters.entries();
        for (std::size_t i = 0; i < std::min(filters.size(), priorities.size()); ++i)
            expect(filters[i].priority == priorities[i]);
        auto &root = f.compile(u"\\mathbb{R}+\\frac{1}{\\sqrt[3]{x}}+x^2");
        expect(root.kind() == NodeKind::math);
        expect(attr(root, u"display") == u"block");
        expect(findNodes(root, NodeKind::mfrac).size() == 1_u);
        expect(findNodes(root, NodeKind::mroot).size() == 1_u);
        expect(findNodes(root, NodeKind::msup).size() == 1_u);
        expect(findNodes(root, NodeKind::msubsup).empty());
        auto identifiers = findNodes(root, NodeKind::mi);
        expect(std::ranges::any_of(identifiers, [](auto node) {
            return NodeUtil::getText(*node) == u"R" &&
                   attr(*node, u"mathvariant") == u"double-struck";
        }));
        expect(!f.jax->parseOptions().request().error);
        expect(f.jax->parseOptions().request().parsers.empty());
    };
    "TeX postfilter combines adjacent relation tokens in the inferred row"_test = [] {
        JaxFixture f;
        f.configure();
        auto &root = f.compile(u"a><b");

        auto rows = findNodes(root, NodeKind::inferred_mrow);
        expect(rows.size() == 1_u) << "base configuration compiles into one inferred row";
        if (rows.size() != 1)
            return;

        auto children = rows.front()->children();
        expect(children.size() == 3_u)
            << "the inferred row contains left operand, combined relation, and right operand";
        if (children.size() != 3)
            return;
        const bool hasAllChildren = children[0] && children[1] && children[2];
        expect(hasAllChildren) << "all three inferred-row children are present";
        if (!hasAllChildren)
            return;

        expect(children[0]->kind() == NodeKind::mi);
        expect(NodeUtil::getText(*children[0]) == u"a");
        expect(children[1]->kind() == NodeKind::mo)
            << "the two source relations compile to one operator node";
        expect(NodeUtil::getText(*children[1]) == u"><")
            << "the combined operator preserves both relation characters in order";
        expect(children[2]->kind() == NodeKind::mi);
        expect(NodeUtil::getText(*children[2]) == u"b");
    };
    "TeX input jax preserves supplied config and package filter registration order"_test = [] {
        JaxFixture f;
        ConfigurationDefinition package;
        package.config = ProcessorSpec<ConfigMethod>{configurePackage};
        package.preprocessors.emplace_back(FilterList::Callback{packagePre});
        package.postprocessors.emplace_back(FilterList::Callback{packagePost});
        Configuration::create(f.registry, u"test-filter-order", std::move(package));
        TexOptionValues supplied;
        supplied.packages =
            std::vector<PackageSelection>{{u"base", {}}, {u"test-filter-order", {}}};
        supplied.preFilters = std::vector<FilterList::Item>{{5, userPre}};
        supplied.postFilters = std::vector<FilterList::Item>{{5, userPost}};
        f.configure(supplied);
        auto &root = f.compile(u"replaced", false);
        expect(findNodes(root, NodeKind::msup).size() == 1_u);
        expect(attr(root, u"data-latex") == u"x^2");
        expect(f.jax->options.baseURL.value() ==
               u"config;user-pre;config-pre;package-pre;user-post;package-post;");
    };
    "TeX input jax converts only TexError and preserves configured formatter"_test = [] {
        JaxFixture f;
        f.configure();
        auto &root = f.compile(u"\\notACommand");
        expect(f.jax->parseOptions().request().error) << "TexError marks the expression";
        expect(findNodes(root, NodeKind::merror).size() == 1_u)
            << "Default formatter returns merror";
        expect(tokenText(root).find(u"Undefined control sequence") != std::u16string::npos)
            << "Default formatter retains the diagnostic text";
        expect(f.jax->parseOptions().request().parsers.empty())
            << "Parser scope unwinds after TexError";
        f.jax->options.formatError = customError;
        auto &custom = f.compile(u"\\notACommand");
        expect(findNodes(custom, NodeKind::merror).empty()) << "Custom formatter replaces merror";
        expect(findNodes(custom, NodeKind::mtext).size() == 1_u)
            << "Custom formatter creates mtext";
        f.jax->preFilters.add(failPre);
        expect(throws<std::runtime_error>([&] { f.compile(u"x"); }));
        f.jax->preFilters.remove(failPre);
        expect(!findNodes(f.compile(u"x"), NodeKind::mi).empty());
        expect(!f.jax->parseOptions().request().error);
        expect(tokenText(f.jax->formatError(TexError("Multiline", u"first\nsecond"))) == u"first");
        const std::array<std::pair<std::u16string_view, std::u16string_view>, 6> messages{
            {{u"first\nsecond\nthird", u"first\nthird"},
             {u"first\nsecond\rthird", u"first\rthird"},
             {u"first\nsecond\u2028third", u"first\u2028third"},
             {u"first\nsecond\u2029third", u"first\u2029third"},
             {u"first\rsecond", u"first\rsecond"},
             {u"\n", u""}}};
        for (auto const &[message, expected] : messages)
            expect(tokenText(f.jax->formatError(TexError("Multiline", std::u16string(message)))) ==
                   expected);
    };
    "TeX input jax applies hsize after filtering and stops filters on false"_test = [] {
        JaxFixture f;
        f.configure();
        auto &root = f.compile(u"\\hsize{5em} x");
        expect(attr(root, u"maxwidth") == u"5em");
        expect(attr(root, u"overflow") == u"linebreak");
        f.jax->postFilters.add(stopPost, -10);
        f.jax->postFilters.add(userPost, -9);
        f.compile(u"x");
        expect(f.jax->options.baseURL.value() == u"stop;");
    };
    "TeX input jax rebinds document ownership and clears per-expression lists"_test = [] {
        JaxFixture f;
        f.configure({}, false);
        std::size_t expectedCount{};
        for (std::size_t i = 0; i < 64; ++i) {
            MmlDocument document;
            {
                TeXRequest request(*f.jax, document.factory());
                folia::math::core::AbstractMathItem math(u"x_i+\\frac{1}{2}", f.jax.get());
                auto &root = f.jax->compile(math, document);
                expect(&root.document() == &document);
                expect(&f.jax->parseOptions().request().nodeFactory.mmlFactory() ==
                       &document.factory());
                expect(f.jax->parseOptions().request().root == &root);
                expect(f.jax->parseOptions().request().parsers.empty());
                if (i == 0)
                    expectedCount = document.size();
                expect(document.size() == expectedCount);
                expect(f.jax->parseOptions().getList(u"msub").size() == 1_u);
                expect(f.jax->parseOptions().hasRequest());
            }
            expect(!f.jax->parseOptions().hasRequest());
        }
        f.jax->reset(9);
        expect(!f.jax->parseOptions().hasRequest());
        expect(throws<std::logic_error>([&] { (void)f.jax->parseOptions().request(); }));
        expect(f.jax->session().counters().equation == 9);
    };
    "TeX request rejects missing and foreign bindings and restores nested scopes"_test = [] {
        JaxFixture f;
        f.configure({}, false);
        auto &options = f.jax->parseOptions();
        expect(!options.hasRequest());
        bool missingRejected = false;
        try {
            (void)options.request();
        } catch (std::logic_error const &) {
            missingRejected = true;
        }
        expect(missingRejected);

        MmlDocument outerDocument, innerDocument;
        {
            TeXRequest outer(*f.jax, outerDocument.factory());
            expect(options.hasRequest());
            expect(&options.request().nodeFactory.mmlFactory() == &outerDocument.factory());
            bool foreignRejected = false;
            {
                TeXRequest inner(*f.jax, innerDocument.factory());
                expect(&options.request().nodeFactory.mmlFactory() == &innerDocument.factory());
                folia::math::core::AbstractMathItem foreign(u"x", f.jax.get());
                try {
                    (void)f.jax->compile(foreign, outerDocument);
                } catch (std::exception const &) {
                    foreignRejected = true;
                }
                expect(foreignRejected);
                expect(&options.request().nodeFactory.mmlFactory() == &innerDocument.factory());
            }
            expect(&options.request().nodeFactory.mmlFactory() == &outerDocument.factory());
            try {
                TeXRequest exceptional(*f.jax, innerDocument.factory());
                throw std::runtime_error("request scope unwind");
            } catch (std::runtime_error const &) {
            }
            expect(options.hasRequest());
            expect(&options.request().nodeFactory.mmlFactory() == &outerDocument.factory());
        }
        expect(!options.hasRequest());
    };
    "configuration runtime add installs original creators defaults tags and filters"_test = [] {
        JaxFixture f;
        f.configure();
        ConfigurationDefinition package;
        package.init = ProcessorSpec<InitMethod>{dynamicInit};
        package.config = ProcessorSpec<ConfigMethod>{dynamicConfig};
        package.options.maxMacros = 99;
        package.nodes.node = dynamicNode;
        package.preprocessors.emplace_back(FilterList::Callback{packagePre}, 2);
        Configuration::create(f.registry, u"test-dynamic", std::move(package));
        TexOptionValues supplied;
        supplied.maxMacros = 123;
        f.jax->configuration().add(u"test-dynamic", *f.jax, supplied);
        expect(f.jax->options.maxMacros.value() == 123_u);
        expect(f.jax->configuration().options.maxMacros.value() == 100_u);
        expect(f.jax->options.baseURL.value() == u"dynamic;");
        auto &root = f.compile(u"x");
        expect(attr(root, u"data-created") == u"dynamic");
        expect(f.jax->options.baseURL.value() == u"dynamic;package-pre;");
        expect(
            throws<std::invalid_argument>([&] { f.jax->configuration().add(u"absent", *f.jax); }));
    };
};
