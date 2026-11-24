#include "math/support/tex_fixture.hpp"
import folia.math.adaptors.lite_adaptor;
import folia.math.core.mml_tree.mml_nodes.html_node;
import folia.math.core.mml_tree.xml_dom_adaptor;
import folia.math.handlers.html.html_dom_strings;
import folia.math.input.tex.base_configuration;
import folia.math.input.tex.find_tex;
import folia.math.input.tex.setoptions.set_options_configuration;
import folia.math.input.tex.texhtml.tex_html_configuration;
import folia.math.core.math_item;
import folia.math.core.mml_tree.node_kind;
import folia.math.util.js_number;
import folia.math.util.utf16;
#include "math/fixtures/texhtml.inc"

namespace {
using namespace folia::math::adaptors::lite;
using namespace folia::math::handlers::html;
using namespace folia::math::core;

struct TexHtmlFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    MmlDocument document;
    LiteAdaptor adaptor;
    std::shared_ptr<HTMLDomStringsOptions> htmlOptions = std::make_shared<HTMLDomStringsOptions>();
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;

    explicit TexHtmlFixture(std::optional<bool> allow = true) {
        TexHtmlConfiguration::initializeModule(registry, maps, htmlOptions);
        SetOptionsConfiguration::initializeModule(registry);
        TexOptionValues options;
        options.packages =
            std::vector<PackageSelection>{{u"base", {}}, {u"texhtml", {}}, {u"setoptions", {}}};
        if (allow)
            options.allowTexHTML = *allow;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, std::move(options));
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
};

MmlNode *firstKind(MmlNode &root, NodeKind kind) {
    if (root.kind() == kind)
        return &root;
    for (auto *child : root.children())
        if (auto *found = firstKind(*child, kind))
            return found;
    return nullptr;
}

std::u16string textTokens(MmlNode const &root) {
    std::u16string text;
    const auto visit = [&](auto &&self, MmlNode const *node) -> void {
        if (node->isToken())
            text += NodeUtil::getText(*node);
        for (auto const *child : node->children())
            self(self, child);
    };
    visit(visit, &root);
    return text;
}

std::u16string decimal(std::size_t value) {
    const auto text = std::to_string(value);
    return {text.begin(), text.end()};
}
std::u16string decimal(int value) {
    const auto text = std::to_string(value);
    return {text.begin(), text.end()};
}
void add(std::u16string &out, std::u16string_view value) {
    out += decimal(value.size());
    out += u":";
    out += value;
}
void addProperty(std::u16string &out, std::u16string_view name, std::u16string_view type,
                 std::u16string_view value) {
    add(out, name);
    add(out, type);
    add(out, value);
}

std::u16string domSnapshot(folia::math::mml::XmlDomAdaptor const &observer,
                           folia::math::mml::XmlRef const &node, LiteAdaptor &lite) {
    std::u16string out;
    const auto kind = observer.kind(node);
    add(out, std::u16string(kind.begin(), kind.end()));
    if (kind == "#text") {
        add(out, observer.value(node));
        return out;
    }
    if (kind == "#comment") {
        const auto raw = observer.value(node);
        add(out, raw);
        add(out, lite.value(LiteComment::create(raw)));
        return out;
    }
    if (kind == "#document") {
        const auto children = observer.childNodes(node);
        add(out, decimal(children.size()));
        for (auto const &child : children)
            out += domSnapshot(observer, child, lite);
        add(out, observer.textContent(node));
        return out;
    }
    const auto attributes = observer.allAttributes(node);
    add(out, decimal(attributes.size()));
    for (auto const &attribute : attributes) {
        add(out, std::u16string(attribute.name.begin(), attribute.name.end()));
        add(out, attribute.value);
    }
    const auto children = observer.childNodes(node);
    add(out, decimal(children.size()));
    for (auto const &child : children)
        out += domSnapshot(observer, child, lite);
    const auto outer = observer.outerHTML(node);
    add(out, folia::math::utf16::decode(outer).text);
    add(out, observer.textContent(node));
    return out;
}

std::u16string sourceSnapshot(MmlNode const &root, LiteAdaptor &lite) {
    std::u16string out;
    const auto visit = [&](auto &&self, MmlNode const *node) -> void {
        if (!node) {
            add(out, u"null");
            return;
        }
        const auto kind = nodeKindName(node->kind());
        add(out, std::u16string(kind.begin(), kind.end()));
        auto attributes = node->attributes().ownEntries();
        std::ranges::sort(attributes, {}, &AttributeSet::Entry::first);
        add(out, decimal(attributes.size()));
        for (auto const &[key, value] : attributes) {
            add(out, key);
            std::visit(
                [&](auto const &scalar) {
                    using T = std::remove_cvref_t<decltype(scalar)>;
                    if constexpr (std::is_same_v<T, AttributeNull>) {
                        add(out, u"null");
                        add(out, u"");
                    } else if constexpr (std::is_same_v<T, bool>) {
                        add(out, u"boolean");
                        add(out, scalar ? u"true" : u"false");
                    } else if constexpr (std::is_same_v<T, double>) {
                        const auto number = folia::math::js_number::toString(scalar);
                        add(out, u"number");
                        add(out, std::u16string(number.begin(), number.end()));
                    } else {
                        add(out, u"string");
                        add(out, scalar);
                    }
                },
                value);
        }
        auto const &tex = node->properties().tex;
        std::size_t count{};
        count += tex.explicit_tex_class.has_value();
        count += tex.unicode.has_value();
        count += tex.auto_operator.has_value();
        count += tex.function_operator.has_value();
        count += tex.fix_stretchy.has_value();
        count += tex.ignore_variant.has_value();
        count += tex.no_dictionary_definition.has_value();
        count += tex.prime_style.has_value();
        count += tex.in_lists.has_value();
        count += tex.script_position.has_value();
        count += tex.open.has_value();
        count += tex.close.has_value();
        count += tex.math_accent.has_value();
        count += tex.move_sup_sub.has_value();
        count += tex.sub_sup_allowed.has_value();
        count += tex.variant_form.has_value();
        add(out, decimal(count));
        if (tex.explicit_tex_class)
            addProperty(out, u"texClass", u"number",
                        decimal(static_cast<int>(*tex.explicit_tex_class)));
        const auto addBool = [&](std::u16string_view name, std::optional<bool> value) {
            if (value)
                addProperty(out, name, u"boolean", *value ? u"true" : u"false");
        };
        addBool(u"unicode", tex.unicode);
        addBool(u"autoOP", tex.auto_operator);
        addBool(u"fnOP", tex.function_operator);
        addBool(u"fixStretchy", tex.fix_stretchy);
        addBool(u"ignore-variant", tex.ignore_variant);
        addBool(u"noDictDef", tex.no_dictionary_definition);
        addBool(u"texprimestyle", tex.prime_style);
        if (tex.in_lists) {
            std::u16string joined;
            for (auto const &item : *tex.in_lists) {
                if (!joined.empty())
                    joined += u",";
                joined += item;
            }
            addProperty(out, u"in-lists", u"string", joined);
        }
        if (tex.script_position)
            addProperty(out, u"sub-sup", u"string",
                        *tex.script_position == folia::math::mml::ScriptPosition::subscript ? u"_"
                                                                                            : u"^");
        if (tex.open)
            addProperty(out, u"open", u"string", *tex.open);
        if (tex.close)
            addProperty(out, u"close", u"string", *tex.close);
        addBool(u"mathaccent", tex.math_accent);
        addBool(u"movesupsub", tex.move_sup_sub);
        addBool(u"subsupOK", tex.sub_sup_allowed);
        addBool(u"variantForm", tex.variant_form);
        add(out,
            node->isToken() || node->kind() == NodeKind::text ? NodeUtil::getText(*node) : u"");
        if (auto const *html = dynamic_cast<folia::math::mml::HtmlNode const *>(node)) {
            const auto xml = html->getHTML();
            auto observer = folia::math::mml::XmlDomAdaptor::create();
            add(out, u"html");
            out += domSnapshot(*observer, xml, lite);
            const auto serialized = html->getSerializedHTML();
            add(out, folia::math::utf16::decode(serialized).text);
            add(out, html->textContent());
        } else {
            add(out, u"no-html");
        }
        const auto children = node->children();
        add(out, decimal(children.size()));
        for (auto const *child : children)
            self(self, child);
    };
    visit(visit, &root);
    return out;
}

void reportSnapshotDifference(std::u16string_view name, std::u16string_view actual,
                              std::u16string_view expected) {
    std::size_t offset{};
    while (offset < actual.size() && offset < expected.size() &&
           actual[offset] == expected[offset])
        ++offset;
    if (offset == actual.size() && offset == expected.size())
        return;
    const auto begin = offset > 60 ? offset - 60 : 0;
    const auto actualEnd = std::min(actual.size(), offset + 100);
    const auto expectedEnd = std::min(expected.size(), offset + 100);
    std::cerr << "Texhtml MML snapshot mismatch " << folia::math::utf16::encode(name)
              << " at UTF-16 offset " << offset << " (actual=" << actual.size()
              << ", expected=" << expected.size() << ")\nactual:   "
              << folia::math::utf16::encode(actual.substr(begin, actualEnd - begin))
              << "\nexpected: "
              << folia::math::utf16::encode(expected.substr(begin, expectedEnd - begin))
              << '\n';
}

void reportFindObservation(std::u16string_view name,
                           std::vector<std::u16string> const &input,
                           std::vector<ProtoItem> const &items) {
    std::cerr << "Texhtml FindTeX observation " << folia::math::utf16::encode(name)
              << "; inputStrings=" << input.size() << ", matches=" << items.size() << '\n';
    for (std::size_t i = 0; i < input.size(); ++i)
        std::cerr << "  input[" << i << "]=" << folia::math::utf16::encode(input[i]) << '\n';
    for (std::size_t i = 0; i < items.size(); ++i)
        std::cerr << "  match[" << i << "] n=" << items[i].n << " start="
                  << items[i].start.n << " end=" << items[i].end.n << " open="
                  << folia::math::utf16::encode(items[i].open) << " close="
                  << folia::math::utf16::encode(items[i].close) << " math="
                  << folia::math::utf16::encode(items[i].math) << '\n';
}

struct BorrowTrace {
    std::vector<bool> pre, post;
};
thread_local BorrowTrace *borrowTrace{};
struct BorrowTraceScope {
    BorrowTrace *previous{borrowTrace};
    explicit BorrowTraceScope(BorrowTrace &trace) {
        borrowTrace = &trace;
    }
    ~BorrowTraceScope() {
        borrowTrace = previous;
    }
};
void observeBorrowPre(TexFilterData &data) {
    if (borrowTrace)
        borrowTrace->pre.push_back(data.data.request().texhtmlAdaptor == data.adaptor);
}
void observeBorrowPost(TexFilterData &data) {
    if (borrowTrace)
        borrowTrace->post.push_back(data.data.request().texhtmlAdaptor == nullptr);
}
void throwBeforeTexHtmlCompile(TexFilterData &) {
    throw std::runtime_error("texhtml compile filter failure");
}

struct CreatorRequest {
    NodeKind kind{};
    std::vector<NodeKind> children;
    bool definitionHasAttributes{};
    bool hasText{};
};
std::u16string customIncludeHandler(folia::math::DOMNodeRef const &,
                                    folia::math::AbstractDOMAdaptor &) {
    return u"CUSTOM";
}
thread_local std::vector<CreatorRequest> *creatorRequests{};
MmlNode &observeCreator(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                        NodeDefinition definition, MmlNode *text) {
    if (creatorRequests) {
        CreatorRequest request{
            kind, {}, !definition.attributes.ownEntries().empty(), text != nullptr};
        request.children.reserve(children.size());
        for (auto const *child : children)
            request.children.push_back(child->kind());
        creatorRequests->push_back(std::move(request));
    }
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}

} // namespace

suite texhtml = [] {
    "texhtml pinned direct cases exercise enabled and disabled parser paths"_test = [] {
        expect(std::size(texhtmlDirectCases) == 22_u);
        for (auto const &test : texhtmlDirectCases) {
            const auto caseName = folia::math::utf16::encode(test.name);
            TexHtmlFixture f(test.initialAllowTexHTML);
            auto &options = f.jax->parseOptions();
            options.options.allowTexHTML = test.initialAllowTexHTML;
            options.request().texhtmlAdaptor = &f.adaptor;
            try {
                TexParser parser(std::u16string(test.input), ParserEnvironmentSeed{}, options);
                auto *root = parser.mml();
                expect(test.errorId.empty()) << caseName;
                expect(root != nullptr);
                if (root) {
                    const auto actual = sourceSnapshot(*root, f.adaptor);
                    if (actual != test.sourceSnapshot)
                        reportSnapshotDifference(test.name, actual, test.sourceSnapshot);
                    expect(actual == test.sourceSnapshot) << caseName;
                }
            } catch (TexError const &error) {
                if (test.errorId.empty() ||
                    error.id() != folia::math::utf16::encode(test.errorId) ||
                    error.message() != test.errorMessage)
                    std::cerr << "Texhtml direct error mismatch " << caseName
                              << ": id=" << error.id() << " message="
                              << folia::math::utf16::encode(error.message()) << '\n';
                expect(!test.errorId.empty()) << caseName;
                expect(error.id() == folia::math::utf16::encode(test.errorId)) << caseName;
                expect(error.message() == test.errorMessage) << caseName;
            }
            options.request().texhtmlAdaptor = nullptr;
        }
    };

    "texhtml original render2mml cases traverse Lite, HTMLDomStrings, FindTeX and TeX compile"_test =
        [] {
            expect(std::size(texhtmlPipelineCases) == 5_u);
            for (auto const &test : texhtmlPipelineCases) {
                TexHtmlFixture f;
                BorrowTrace trace;
                BorrowTraceScope traceScope(trace);
                f.jax->preFilters.add(observeBorrowPre);
                f.jax->postFilters.add(observeBorrowPost);
                const auto html = u"<html><head></head><body>$$" + std::u16string(test.input) +
                                  u"$$</body></html>";
                auto parsed = f.adaptor.parse(html, u"text/html");
                auto body = f.adaptor.body(parsed);
                HTMLDomStrings strings(f.adaptor, *f.htmlOptions);
                auto found = strings.find(body);
                FindTeX finder;
                auto items = finder.findMath(found.strings);
                if (items.size() != 1_u)
                    reportFindObservation(test.name, found.strings, items);
                expect(items.size() == 1_u) << folia::math::utf16::encode(test.name);
                if (items.size() != 1)
                    continue;
                auto const &item = items.front();
                if (item.math != test.actualTex)
                    reportFindObservation(test.name, found.strings, items);
                expect(item.math == test.actualTex) << folia::math::utf16::encode(test.name);
                expect(item.display == MathDisplay::display_math);
                expect(item.open == u"$$");
                expect(item.close == u"$$");
                expect(item.n == 0_u);

                AbstractMathItem expression(item.math, f.jax.get(), item.display);
                auto &root =
                    f.jax->compile(expression, f.document, CompileServices{nullptr, &f.adaptor});
                expect(f.jax->parseOptions().request().texhtmlAdaptor == nullptr)
                    << "compile clears the borrowed Lite adaptor";
                expect(!trace.pre.empty() && trace.pre.front());
                expect(!trace.post.empty() && trace.post.back());
                expect(root.kind() == NodeKind::math);
                const auto actualSnapshot = sourceSnapshot(root, f.adaptor);
                if (actualSnapshot != test.sourceSnapshot) {
                    reportFindObservation(test.name, found.strings, items);
                    reportSnapshotDifference(test.name, actualSnapshot, test.sourceSnapshot);
                }
                expect(actualSnapshot == test.sourceSnapshot)
                    << folia::math::utf16::encode(test.name);
                auto *foreign = firstKind(root, NodeKind::html);
                const auto containsHtml = test.name == u"original-html" ||
                                          test.name == u"original-multiple" ||
                                          test.name == u"original-nested";
                expect((foreign != nullptr) == containsHtml)
                    << folia::math::utf16::encode(test.name);
                if (foreign) {
                    auto *htmlNode = dynamic_cast<folia::math::mml::HtmlNode *>(foreign);
                    expect(htmlNode != nullptr);
                    if (htmlNode) {
                        expect(!htmlNode->getSerializedHTML().empty());
                        expect(htmlNode->textContent().size() > 0_u ||
                               test.name == u"original-empty");
                        expect(htmlNode->getHTML() != nullptr);
                    }
                }
                expect(!textTokens(root).empty());
            }
        };

    "texhtml direct missing marker reports Source error and the same TeX instance recovers"_test =
        [] {
            TexHtmlFixture f;
            auto const &broken = texhtmlRecoveryCase;
            AbstractMathItem bad(std::u16string(broken.input), f.jax.get());
            auto &badRoot =
                f.jax->compile(bad, f.document, CompileServices{nullptr, &f.adaptor});
            auto *error = firstKind(badRoot, NodeKind::merror);
            expect(error != nullptr);
            expect(f.jax->parseOptions().request().texhtmlAdaptor == nullptr);

            AbstractMathItem recovered(std::u16string(broken.recoveredInput), f.jax.get());
            auto &root =
                f.jax->compile(recovered, f.document, CompileServices{nullptr, &f.adaptor});
            expect(firstKind(root, NodeKind::html) != nullptr);
            expect(f.jax->parseOptions().request().texhtmlAdaptor == nullptr);
        };

    "texhtml disabled remains literal and leaves no DOM adaptor borrow"_test = [] {
        TexHtmlFixture f(false);
        AbstractMathItem expression(u"<tex-html><b>literal</b></tex-html>", f.jax.get());
        auto &root = f.jax->compile(expression, f.document, CompileServices{nullptr, &f.adaptor});
        expect(firstKind(root, NodeKind::html) == nullptr) << "disabled input creates no HTML node";
        expect(f.jax->parseOptions().request().texhtmlAdaptor == nullptr)
            << "compile releases DOM borrow";
        // Ordinary TeX still normalizes the minus, as in the disabled Source fixture.
        expect(textTokens(root).find(u"tex\u2212html") != std::u16string::npos)
            << "disabled tag characters pass through ordinary TeX token handling";
    };

    "texhtml omitted option uses the package false default"_test = [] {
        TexHtmlFixture f(std::nullopt);
        expect(f.jax->options.allowTexHTML == false);
        AbstractMathItem expression(u"<tex-html><b>literal</b></tex-html>", f.jax.get());
        auto &root = f.jax->compile(expression, f.document, CompileServices{nullptr, &f.adaptor});
        expect(firstKind(root, NodeKind::html) == nullptr);
        expect(f.jax->parseOptions().request().texhtmlAdaptor == nullptr);
    };

    "texhtml compile exception unwinds the borrowed DOM adaptor"_test = [] {
        TexHtmlFixture f;
        f.jax->preFilters.add(throwBeforeTexHtmlCompile);
        AbstractMathItem expression(u"<tex-html><b>x</b></tex-html>", f.jax.get());
        expect(throws<std::runtime_error>([&] {
            (void)f.jax->compile(expression, f.document, CompileServices{nullptr, &f.adaptor});
        }));
        expect(f.jax->parseOptions().request().texhtmlAdaptor == nullptr);
    };

    "texhtml package replaces absent and falsey include hooks but preserves configured values"_test =
        [] {
            const auto find = [](HTMLDomStringsOptions const &options) {
                return std::find_if(
                    options.includeHtmlTags.begin(), options.includeHtmlTags.end(),
                    [](IncludedHtmlTag const &entry) { return entry.name == u"tex-html"; });
            };
            for (int mode = 0; mode != 4; ++mode) {
                ConfigurationHandler registry;
                MapHandler maps;
                auto options = std::make_shared<HTMLDomStringsOptions>();
                if (mode != 0) {
                    IncludeHtmlTag value = std::u16string{};
                    if (mode == 2)
                        value = std::u16string(u"KEEP");
                    else if (mode == 3)
                        value = IncludeHtmlHandler{&customIncludeHandler};
                    options->includeHtmlTags.push_back({u"tex-html", std::move(value)});
                }
                TexHtmlConfiguration::initializeModule(registry, maps, options);
                TexOptionValues supplied;
                supplied.packages = std::vector<PackageSelection>{{u"base", {}}, {u"texhtml", {}}};
                Warnings warnings;
                MmlDocument document;
                MacroCallbacks callbacks{{}, {}};
                TeX jax(registry, maps, callbacks, warnings, supplied);
                TeXRequest request(jax, document.factory());
                auto entry = find(*options);
                expect(entry != options->includeHtmlTags.end());
                if (entry == options->includeHtmlTags.end())
                    continue;
                if (mode == 2)
                    expect(std::get<std::u16string>(entry->value) == u"KEEP");
                else if (mode == 3)
                    expect(std::get<IncludeHtmlHandler>(entry->value) == &customIncludeHandler);
                else
                    expect(std::holds_alternative<IncludeHtmlHandler>(entry->value));
            }
        };

    "texhtml macro map binds the character handler and requests the original HTML then mtext creators"_test =
        [] {
            TexHtmlFixture f;
            auto map = f.maps.getMap(u"tex-html");
            expect(map != nullptr);
            if (map) {
                auto entry = map->lookup(u"<");
                auto macro = std::get_if<Macro const *>(&entry);
                expect(macro && *macro != nullptr);
                if (macro && *macro)
                    expect(std::get<MacroHandler>((*macro)->function) == &HtmlNodeMethods::TexHTML);
            }

            std::vector<CreatorRequest> requests;
            auto *previous = creatorRequests;
            creatorRequests = &requests;
            struct RestoreCreator {
                std::vector<CreatorRequest> *previous;
                ~RestoreCreator() {
                    creatorRequests = previous;
                }
            } restore{previous};
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.node = &observeCreator});
            f.jax->parseOptions().request().texhtmlAdaptor = &f.adaptor;
            TexParser parser(u"<tex-html><b data-x=\"1\">text</b></tex-html>",
                             ParserEnvironmentSeed{}, f.jax->parseOptions());
            auto *tree = parser.mml();
            expect(tree != nullptr);
            expect(requests.size() == 2_u);
            if (requests.size() == 2) {
                expect(requests[0].kind == NodeKind::html);
                expect(requests[0].children.empty());
                expect(!requests[0].definitionHasAttributes && !requests[0].hasText);
                expect(requests[1].kind == NodeKind::mtext);
                expect(requests[1].children == std::vector<NodeKind>{NodeKind::html});
                expect(!requests[1].definitionHasAttributes && !requests[1].hasText);
            }
            f.jax->parseOptions().request().texhtmlAdaptor = nullptr;
        };

    "texhtml HTMLDomStrings handler preserves callback ownership while it runs"_test = [] {
        TexHtmlFixture f;
        auto entry = std::find_if(
            f.htmlOptions->includeHtmlTags.begin(), f.htmlOptions->includeHtmlTags.end(),
            [](IncludedHtmlTag const &tag) { return tag.name == u"tex-html"; });
        expect(entry != f.htmlOptions->includeHtmlTags.end());
        if (entry == f.htmlOptions->includeHtmlTags.end())
            return;
        auto const *handler = std::get_if<IncludeHtmlHandler>(&entry->value);
        expect(handler && *handler != nullptr);
        if (!handler || !*handler)
            return;
        auto dom = f.adaptor.parse(
            u"<body>before<tex-html><b>outer<tex-html>inner</tex-html></b></tex-html>after</body>",
            u"text/html");
        HTMLDomStrings strings(f.adaptor, *f.htmlOptions);
        auto found = strings.find(f.adaptor.body(dom));
        expect(found.strings.size() == 1_u);
        if (!found.strings.empty())
            expect(found.strings.front() ==
                   u"before<tex-html "
                   u"n=\"2\"><b>outer<tex-html>inner</tex-html></b><!2></tex-html>after");
        expect(found.nodes.size() == 1_u);
        expect(f.jax->parseOptions().request().texhtmlAdaptor == nullptr);
    };
};
