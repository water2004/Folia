#include "math/support/tex_fixture.hpp"
#include "math/support/bussproofs_fixture_data.hpp"
import folia.math.input.tex.bussproofs.configuration;
import folia.math.input.tex.bussproofs.mappings;
import folia.math.input.tex.bussproofs.util;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.parse_methods;
import folia.math.output.common;
import folia.math.fonts.newcm_font_module_provider;
namespace {
namespace common = folia::math::output::common;
namespace core = folia::math::core;
namespace scene = folia::math::output::scene;
using namespace std::literals;
constexpr std::array<MacroHandler, 10> proofCallbacks{&BaseMethods::Macro,
                                                      &BussproofsMethods::Axiom,
                                                      &BussproofsMethods::Inference,
                                                      &BussproofsMethods::Label,
                                                      &BussproofsMethods::SetLine,
                                                      &BussproofsMethods::RootAtTop,
                                                      &BussproofsMethods::AxiomF,
                                                      &BussproofsMethods::FCenter,
                                                      &BussproofsMethods::InferenceF,
                                                      &NewcommandMethods::MacroWithTemplate};
thread_local std::string *proofError{};
MmlNode &proofFormatError(TeX &jax, TexError const &error) {
    if (proofError)
        *proofError = error.id();
    return jax.formatError(error);
}
class ProofLinebreaks final : public common::Linebreaks {
  public:
    using common::Linebreaks::Linebreaks;
    void breakToWidth(common::CommonWrapper &, double) override {
        throw std::logic_error("Proof subtree measurement must not run line breaking");
    }
};
std::unique_ptr<common::Linebreaks> proofLinebreaks(common::CommonWrapperFactory &factory) {
    return std::make_unique<ProofLinebreaks>(factory);
}
template <class Wrapper> common::CommonWrapperRegistration proofRegistration(NodeKind kind) {
    return {kind,
            {[](common::WrapperBuildArguments const &a) -> std::unique_ptr<common::CommonWrapper> {
                 return std::make_unique<Wrapper>(a.factory, a.node, a.parent);
             },
             [](common::CommonWrapper const &w) {
                 return dynamic_cast<Wrapper const *>(&w) != nullptr;
             }},
            common::CommonWrapperClassData::commonDefaults()};
}
common::CommonWrapperCatalog proofCatalog() {
    return {proofRegistration<common::CommonMath>(NodeKind::math),
            proofRegistration<common::CommonMrow>(NodeKind::mrow),
            proofRegistration<common::CommonInferredMrow>(NodeKind::inferred_mrow),
            proofRegistration<common::CommonWrapper>(NodeKind::mstyle),
            proofRegistration<common::CommonTeXAtom>(NodeKind::tex_atom),
            proofRegistration<common::CommonTextNode>(NodeKind::text),
            proofRegistration<common::CommonMi>(NodeKind::mi),
            proofRegistration<common::CommonMn>(NodeKind::mn),
            proofRegistration<common::CommonMo>(NodeKind::mo),
            proofRegistration<common::CommonMtext>(NodeKind::mtext),
            proofRegistration<common::CommonMspace>(NodeKind::mspace),
            proofRegistration<common::CommonMpadded>(NodeKind::mpadded),
            proofRegistration<common::CommonMtable>(NodeKind::mtable),
            proofRegistration<common::CommonMtr>(NodeKind::mtr),
            proofRegistration<common::CommonMtd>(NodeKind::mtd),
            proofRegistration<common::CommonMfrac>(NodeKind::mfrac),
            proofRegistration<common::CommonMsqrt>(NodeKind::msqrt),
            proofRegistration<common::CommonMsup>(NodeKind::msup),
            proofRegistration<common::CommonMsub>(NodeKind::msub),
            proofRegistration<common::CommonMsubsup>(NodeKind::msubsup)};
}
common::FontRepository const &proofFonts() {
    static const auto data = folia::math::fonts::makeNewcmFontData();
    return data.fonts;
}
class ProofOutput final : public core::MathOutputJaxService, public common::MeasurementTarget {
  public:
    std::size_t measurements{};
    bool fail{};
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("Proof output uses its supplied profile metrics");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("Proof fixture requires known NewCM glyphs");
    }
    scene::Scene typeset(core::MathItem &, MmlDocument &) override {
        throw std::logic_error("Proof fixture measures; renderer is not supplied");
    }
    scene::Scene escaped(core::MathItem &, MmlDocument &) override {
        throw std::logic_error("Proof fixture does not render escaped math");
    }
    folia::math::BBox getBBox(core::MathItem &math, MmlDocument &document) override {
        ++measurements;
        if (fail)
            throw std::runtime_error("Proof measurement failure");
        if (!math.root)
            throw std::logic_error("Proof measurement requires subtree");
        auto *root = document.root(), *parent = math.root->parent();
        common::CommonOutputJaxConfig config{
            {16, 8, 640, 1, "MathJaxNewcm", math.isDisplay()}, {}, proofFonts(), "MathJaxNewcm"};
        auto result = common::CommonOutputJax::getBBox(document, *math.root, std::move(config),
                                                       *this, proofCatalog(), &proofLinebreaks);
        expect(document.root() == root);
        expect(math.root->parent() == parent);
        return result;
    }
};
struct ProofFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{proofCallbacks, {}};
    Warnings warnings;
    std::unique_ptr<MmlDocument> document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    ProofOutput output;
    std::string error;
    explicit ProofFixture(bool newcommand = true, bool proof = true) {
        BussproofsConfiguration::initializeModule(registry, maps);
        NewcommandConfiguration::initializeModule(registry, maps);
        TexOptionValues values;
        values.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (newcommand)
            values.packages->push_back({u"newcommand", {}});
        if (proof)
            values.packages->push_back({u"bussproofs", {}});
        values.maxMacros = 1000;
        values.formatError = &proofFormatError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, values);
    }
    MmlNode &compile(std::u16string source, bool withOutput = true) {
        error.clear();
        struct Restore {
            std::string *old{proofError};
            ~Restore() {
                proofError = old;
            }
        } restore;
        proofError = &error;
        request.reset();
        document = std::make_unique<MmlDocument>();
        request = std::make_unique<TeXRequest>(*jax, document->factory());
        core::AbstractMathItem math(std::move(source), jax.get());
        math.setMetrics(16, 8, 640, 1);
        return jax->compile(math, *document, {withOutput ? &output : nullptr});
    }
    MmlNode &ok(std::u16string source) {
        auto &root = compile(std::move(source));
        expect(error.empty()) << error;
        expect(jax->parseOptions().request().parsers.empty());
        return root;
    }
};
std::vector<MmlNode *> proofNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
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
std::u16string proofSnapshot(MmlNode const &root) {
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
void proofMatchesSource(MmlNode const &root, std::u16string_view expected) {
    auto actual = proofSnapshot(root);
    auto mismatch = std::ranges::mismatch(actual, expected);
    const auto index = static_cast<std::size_t>(mismatch.in1 - actual.begin());
    expect(std::ranges::equal(actual, expected))
        << "Bussproofs Source structure at UTF-16 offset " << index << " lengths " << actual.size()
        << " " << expected.size();
}

void proofParents(MmlNode &root) {
    root.walkTree([&](auto &node) {
        for (auto *child : node.children())
            if (child)
                expect(child->parent() == &node);
    });
}
using BussproofsData = math_test::fixtures::BussproofsFixtureData<TexArgument>;
using BussproofsTree = BussproofsData::BussproofsTree;
using BussproofsError = BussproofsData::BussproofsError;
using BussproofsRuntime = BussproofsData::BussproofsRuntime;
using BussproofsStep = BussproofsData::BussproofsStep;
using BussproofsDirect = BussproofsData::BussproofsDirect;
using BussproofsBinding = BussproofsData::BussproofsBinding;

struct ProofRequest {
    NodeKind kind;
    NodeDefinition definition;
    std::vector<MmlNode *> children;
    MmlNode *node;
};
thread_local std::vector<ProofRequest> *proofRequests{};
MmlNode &observeProofNode(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                          NodeDefinition definition, MmlNode *text) {
    auto saved = definition;
    std::vector<MmlNode *> borrowed(children.begin(), children.end());
    auto &result = NodeFactory::createNode(factory, kind, children, std::move(definition), text);
    if (proofRequests)
        proofRequests->push_back({kind, std::move(saved), std::move(borrowed), &result});
    return result;
}
} // namespace
suite bussproofs_package = [] {
    "bussproofs original configuration maps callbacks and filter order"_test = [] {
        ProofFixture f;
        auto config = f.registry.get(u"bussproofs");
        expect(config->priority == 5.0);
        expect(!config->config());
        expect(!config->init());
        expect(config->handler.size() == 4_u);
        for (auto const &handler : config->handler) {
            switch (handler.kind) {
            case HandlerKind::character:
            case HandlerKind::delimiter:
                expect(handler.maps.empty());
                break;
            case HandlerKind::macro:
                expect(handler.maps == std::vector<std::u16string>{u"Bussproofs-macros"});
                break;
            case HandlerKind::environment:
                expect(handler.maps == std::vector<std::u16string>{u"Bussproofs-environments"});
                break;
            }
        }
        expect(config->items.size() == 1_u);
        expect(config->preprocessors.size() == 1_u);
        expect(config->postprocessors.size() == 3_u);
        expect(std::get<FilterList::VoidCallback>(config->preprocessors[0].method) ==
               &bussproofs_util::saveDocument);
        expect(config->preprocessors[0].priority == 1.0);
        const std::array<std::pair<FilterList::VoidCallback, double>, 3> expected{
            {{&bussproofs_util::clearDocument, 3},
             {&bussproofs_util::makeBsprAttributes, 2},
             {&bussproofs_util::balanceRules, 1}}};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            expect(std::get<FilterList::VoidCallback>(config->postprocessors[i].method) ==
                   expected[i].first);
            expect(config->postprocessors[i].priority == expected[i].second);
        }
        const std::array<MacroHandler, 8> methods{
            &BussproofsMethods::Axiom,   &BussproofsMethods::Inference, &BussproofsMethods::Label,
            &BussproofsMethods::SetLine, &BussproofsMethods::RootAtTop, &BussproofsMethods::AxiomF,
            &BussproofsMethods::FCenter, &BussproofsMethods::InferenceF};
        auto map = f.maps.getMap(u"Bussproofs-macros");
        expect(BussproofsData::bussproofsBindings().size() == 33_u);
        for (auto const &binding : BussproofsData::bussproofsBindings()) {
            auto macro = std::get<Macro const *>(map->lookup(binding.name));
            expect(macro != nullptr);
            if (!macro)
                continue;
            expect(std::get<MacroHandler>(macro->function) == methods.at(binding.method - 1));
            bool same = macro->arguments == binding.arguments;
            expect(same);
        }
        auto env = f.maps.getMap(u"Bussproofs-environments");
        expect(std::get<EnvironmentHandler>(*env->parser()) == &ParseMethods::environment);
        auto macro = std::get<Macro const *>(env->lookup(u"prooftree"));
        expect(std::get<EnvironmentBodyHandler>(macro->function) == &BussproofsMethods::Prooftree);
        expect(macro->arguments.size() == 2_u);
        expect(std::holds_alternative<NullArgument>(macro->arguments[0]));
        expect(!std::get<bool>(macro->arguments[1]));
        auto frame = f.jax->parseOptions().itemFactory->create(StackKind::proof_tree);
        expect(dynamic_cast<ProofTreeItem *>(frame.get()) != nullptr);
        ProofFixture alone(false);
        alone.ok(u"\\begin{prooftree}\\AxiomC{A}\\UnaryInfC{B}\\end{prooftree}");
    };
    "bussproofs mathematical trees match real Source including semantic insertion order"_test = [] {
        for (auto const &entry : BussproofsData::bussproofsTrees()) {
            ProofFixture f;
            auto &root = f.ok(std::u16string(entry.source));
            proofMatchesSource(root, entry.snapshot);
            proofParents(root);
            expect(&root.document() == f.document.get());
        }
    };
    "bussproofs original errors and Source runtime boundaries recover the next request"_test = [] {
        for (auto const &entry : BussproofsData::bussproofsErrors()) {
            ProofFixture f;
            f.compile(std::u16string(entry.source));
            expect(f.error == entry.id) << entry.id;
            expect(f.jax->parseOptions().request().parsers.empty());
            f.ok(u"x");
            f.ok(u"\\begin{prooftree}\\AxiomC{A}\\UnaryInfC{B}\\end{prooftree}");
        }
        for (auto const &entry : BussproofsData::bussproofsRuntime()) {
            ProofFixture f;
            expect(entry.kind == "TypeError");
            expect(throws<std::logic_error>([&] { f.compile(std::u16string(entry.source)); }));
            expect(f.jax->parseOptions().request().parsers.empty());
            f.ok(u"x");
            f.ok(u"\\begin{prooftree}\\AxiomC{A}\\UnaryInfC{B}\\end{prooftree}");
        }
    };
    "bussproofs definitions captured callbacks and proof defaults survive expression sequences"_test =
        [] {
            int group = -1;
            std::unique_ptr<ProofFixture> f;
            for (auto const &entry : BussproofsData::bussproofsSteps()) {
                if (entry.group != group) {
                    f = std::make_unique<ProofFixture>();
                    group = entry.group;
                }
                if (entry.reset)
                    f->jax->reset();
                if (!entry.runtime.empty())
                    expect(throws<std::logic_error>(
                        [&] { f->compile(std::u16string(entry.source)); }));
                else {
                    auto &root = f->compile(std::u16string(entry.source));
                    expect(f->error == entry.error);
                    proofMatchesSource(root, entry.snapshot);
                }
                expect(f->jax->parseOptions().request().parsers.empty());
            }
            expect(f->output.measurements > 0_u);
        };
    "bussproofs requires the per-call real measurement service and releases failed borrows"_test =
        [] {
            ProofFixture f;
            expect(throws<std::runtime_error>([&] { f.compile(u"x", false); }));
            expect(f.jax->parseOptions().request().parsers.empty());
            const auto source = u"\\begin{prooftree}\\AxiomC{A}\\AxiomC{B}\\BinaryInfC{C}"
                                u"\\UnaryInfC{D}\\end{prooftree}";
            f.output.fail = true;
            expect(throws<std::runtime_error>([&] { f.compile(source); }));
            expect(f.jax->parseOptions().request().parsers.empty());
            f.output.fail = false;
            f.ok(source);
            expect(f.output.measurements > 0_u);
            ProofFixture noPackage(true, false);
            auto &root = noPackage.compile(u"x", false);
            expect(proofNodes(root, NodeKind::mi).size() == 1_u);
        };
    "bussproofs semantic postprocessor preserves finite insertion deletion and margin exclusions"_test =
        [] {
            for (auto const &entry : BussproofsData::bussproofsDirect()) {
                ProofFixture f;
                f.ok(u"x");
                auto &options = f.jax->parseOptions();
                auto &node = options.request().nodeFactory.makeNode(NodeKind::mrow);
                auto &props = node.properties().proof;
                if (entry.name == u"plain-inference"sv) {
                    props.setRuleDirection(ProofRuleDirection::down);
                    props.setPremiseCount(0);
                    props.setProof(true);
                    props.left_space = .25;
                    props.right_space = .5;
                } else if (entry.name == u"all-finite-reinsert"sv) {
                    props.setSequent(true);
                    props.setAxiom(false);
                    props.setProof(true);
                    props.setPremiseCount(0);
                    props.setRuleDirection(ProofRuleDirection::up);
                    props.setLabelledRule(ProofLabels::both);
                    props.setProofLabel(ProofSide::left);
                    props.setSequentAdjustRight(0);
                    props.setSequentAdjustLeft(.5);
                    props.setSequentProcessed(true);
                    props.setAxiom(true);
                    props.erase(ProofAttribute::sequent_processed);
                    props.setSequentProcessed(false);
                } else if (entry.name == u"delete-reinsert"sv) {
                    props.setProof(true);
                    props.setPremiseCount(1);
                    props.erase(ProofAttribute::proof);
                    props.setProof(false);
                } else if (entry.name == u"preserve-existing"sv)
                    node.attributes().set(u"semantics", u"existing");
                else if (entry.name == u"margins-only"sv) {
                    props.left_space = .25;
                    props.right_space = .5;
                } else
                    throw std::logic_error("Unknown proof semantic Source case");
                options.request().root = &node;
                core::AbstractMathItem math(u"", f.jax.get());
                math.root = &node;
                TexFilterData data{math, *f.document, options, &f.output};
                bussproofs_util::makeBsprAttributes(data);
                expect((node.attributes().own(u"semantics") != nullptr) == entry.present);
                expect(std::ranges::equal(attr(node, u"semantics"), entry.semantics));
            }
        };
    "bussproofs configured creators receive original definitions and document child borrows"_test =
        [] {
            ProofFixture f;
            std::vector<ProofRequest> requests;
            struct Restore {
                std::vector<ProofRequest> *old{proofRequests};
                ~Restore() {
                    proofRequests = old;
                }
            } restore;
            proofRequests = &requests;
            f.jax->parseOptions().setNodeCreators(NodeCreators{.node = &observeProofNode});
            auto &root = f.ok(u"\\begin{prooftree}\\AxiomC{A}\\LeftLabel{l}\\RightLabel{r}"
                              u"\\UnaryInfC{B}\\end{prooftree}");
            std::size_t spaces{}, labels{}, rules{};
            for (auto const &request : requests) {
                expect(&request.node->document() == f.document.get());
                if (request.kind == NodeKind::mspace &&
                    attr(request.definition.attributes, u"width") == u".5ex")
                    ++spaces;
                if (request.kind == NodeKind::mpadded) {
                    ++labels;
                    expect(request.children.size() == 1_u);
                    expect(request.node->children().front()->children().front() ==
                           request.children.front());
                    expect(std::ranges::equal(attr(request.definition.attributes, u"width"),
                                              u"+.5ex"sv));
                }
                if (request.kind == NodeKind::mtable &&
                    attr(request.definition.attributes, u"rowlines") == u"solid") {
                    ++rules;
                    expect(request.children.size() == 2_u);
                    expect(std::ranges::equal(attr(request.definition.attributes, u"align"),
                                              u"top 2"sv));
                    expect(request.node->children()[0] == request.children[0]);
                    expect(request.node->children()[1] == request.children[1]);
                }
            }
            expect(spaces == 4_u);
            expect(labels == 2_u);
            expect(rules == 1_u);
            proofParents(root);
        };
};
