#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.tagformat.tag_format_configuration;
import folia.math.input.tex.ams.configuration;

namespace {
std::u16string numberText(std::int64_t n) {
    const auto text = std::to_string(n);
    return {text.begin(), text.end()};
}
std::u16string customNumber(std::int64_t n) {
    return u"N" + numberText(n + 10);
}
std::u16string updatedNumber(std::int64_t n) {
    return u"U" + numberText(n);
}
TagFormatValue customTag(std::u16string_view tag) {
    return std::vector<std::u16string>{u"[", std::u16string(tag), u"]"};
}
TagFormatValue stringTag(std::u16string_view tag) {
    return u"<" + std::u16string(tag) + u">";
}
TagFormatValue customReference(std::u16string_view tag) {
    return u"ref:" + std::u16string(tag);
}
std::u16string customId(std::u16string_view id) {
    return u"id:" + std::u16string(id);
}
std::u16string customUrl(std::u16string_view id, std::u16string_view base) {
    return std::u16string(base) + u"/to/" + std::u16string(id);
}
TagFormatOptions customFormats() {
    TagFormatOptions result;
    result.number = &customNumber;
    result.tag = &customTag;
    result.ref = &customReference;
    result.id = &customId;
    result.url = &customUrl;
    return result;
}
std::u16string formattedText(TagFormatValue const &value) {
    if (auto text = std::get_if<std::u16string>(&value))
        return *text;
    std::u16string result;
    for (auto const &part : std::get<std::vector<std::u16string>>(value))
        result += part;
    return result;
}
struct ParentCounters {
    std::size_t created{}, alive{}, destroyed{}, finalized{};
};
class ParentTags final : public AllTags {
    ParentCounters &counts_;

  public:
    ParentTags(ParseOptions &options, ParentCounters &counts) : AllTags(options), counts_(counts) {
        ++counts_.created;
        ++counts_.alive;
    }
    ~ParentTags() override {
        --counts_.alive;
        ++counts_.destroyed;
    }
    MmlNode &finalize(MmlNode &node, TexEnvironment const &env) override {
        ++counts_.finalized;
        auto &result = AllTags::finalize(node, env);
        result.attributes().set(u"data-parent", u"yes");
        return result;
    }

  protected:
    std::u16string formatNumberDefault(std::int64_t n, std::optional<std::int64_t>) override {
        return u"P" + numberText(n);
    }
    TagFormatValue formatTagDefault(std::u16string_view tag) override {
        return u"parent:" + std::u16string(tag);
    }
    std::u16string formatIdDefault(std::u16string_view id) override {
        return u"parent:" + std::u16string(id);
    }
    std::u16string formatUrlDefault(std::u16string_view id, std::u16string_view base) override {
        return std::u16string(base) + u"!" + std::u16string(id);
    }
};
thread_local std::string *tagError{};
MmlNode &formatTagError(TeX &jax, TexError const &error) {
    if (tagError)
        *tagError = error.id();
    return jax.formatError(error);
}
struct TagFixture {
    ParentCounters parent;
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit TagFixture(TexOptionValues options = {}, bool selected = true,
                        bool customParent = false) {
        AmsConfiguration::initializeModule(registry, maps);
        TagFormatConfiguration::initializeModule(registry);
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"ams", {}}};
        if (!options.tags)
            options.tags = u"all";
        if (customParent) {
            ConfigurationDefinition definition;
            definition.tags = {{u"parent", [this](ParseOptions &owner) -> std::unique_ptr<Tags> {
                                    return std::make_unique<ParentTags>(owner, parent);
                                }}};
            Configuration::create(registry, u"tagformat-parent", std::move(definition));
            options.packages->push_back({u"tagformat-parent", {}});
        }
        if (selected)
            options.packages->push_back({u"tagformat", {}});
        options.formatError = &formatTagError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source, bool display = true) {
        error.clear();
        struct Restore {
            std::string *previous{tagError};
            ~Restore() {
                tagError = previous;
            }
        } restore;
        tagError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(display);
        auto &root = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return root;
    }
    MmlNode &ok(std::u16string source, bool display = true) {
        auto &root = compile(std::move(source), display);
        expect(error.empty()) << error;
        return root;
    }
};
std::vector<MmlNode *> tagNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
std::u16string tagText(MmlNode &root) {
    std::u16string result;
    root.walkTree([&](auto &node) {
        if (node.isToken())
            result += NodeUtil::getText(node);
    });
    return result;
}
std::vector<MmlNode *> tagCells(MmlNode &root) {
    std::vector<MmlNode *> result;
    for (auto row : tagNodes(root, NodeKind::mlabeledtr))
        if (!row->children().empty())
            result.push_back(row->children()[0]);
    return result;
}
MmlNode *oneTag(MmlNode &root) {
    auto nodes = tagCells(root);
    expect(nodes.size() == 1_u);
    return nodes.size() == 1 ? nodes[0] : nullptr;
}
std::vector<MmlNode *> tagRefs(MmlNode &root) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (attr(node, u"class") == u"MathJax_ref")
            result.push_back(&node);
    });
    return result;
}
} // namespace

suite tagformat = [] {
    "tagformat all five original defaults retain numeric tag ref id and URL formatting"_test = [] {
        auto defaults = TagsFactory::formatters();
        expect(defaults.number.value_or(nullptr) != nullptr);
        expect((*defaults.number)(7) == u"7");
        expect(formattedText((*defaults.tag)(u"X")) == u"(X)");
        expect(defaults.ref.value_or(nullptr) == nullptr);
        expect((*defaults.id)(u"a b\u00a0c") == u"mjx-eqn:a_b_c");
        expect((*defaults.url)(u"a% /?α😀", u"/doc") == u"/doc#a%25%20%2F%3F%CE%B1%F0%9F%98%80");
        TexOptionValues options;
        options.baseURL = u"/doc";
        TagFixture f(options);
        const auto definition = f.registry.get(u"tagformat");
        expect(definition->config() == &tagformatConfig);
        expect(definition->configMethod->priority == 10.0);
        expect(definition->options.tagformat->number == defaults.number);
        expect(definition->options.tagformat->tag == defaults.tag);
        expect(definition->options.tagformat->ref == defaults.ref);
        expect(definition->options.tagformat->id == defaults.id);
        expect(definition->options.tagformat->url == defaults.url);
        auto node = oneTag(f.ok(u"x\\label{a}"));
        if (node) {
            expect(tagText(*node) == u"(1)");
            expect(attr(*node, u"id") == u"mjx-eqn:a");
        }
        auto label = f.jax->session().findLabel(u"a");
        expect(label != nullptr);
        if (label) {
            expect(label->tag == u"1");
            expect(label->id == u"mjx-eqn:a");
        }
        auto refs = tagRefs(f.ok(u"\\ref{a}+\\eqref{a}", false));
        expect(refs.size() == 2_u);
        if (refs.size() == 2) {
            expect(tagText(*refs[0]) == u"1");
            expect(tagText(*refs[1]) == u"(1)");
            for (auto ref : refs)
                expect(attr(*ref, u"href") == u"/doc#mjx-eqn%3Aa");
        }
        expect(f.jax->session().counters().equation == 1);
        node = oneTag(f.ok(u"y"));
        if (node)
            expect(attr(*node, u"id") == u"mjx-eqn:2");
        f.jax->reset(7);
        node = oneTag(f.ok(u"z"));
        if (node)
            expect(tagText(*node) == u"(8)");
    };
    "tagformat custom callbacks vector or string tags nullable ref and replacement options stay live"_test =
        [] {
            TexOptionValues options;
            options.tagformat = customFormats();
            options.baseURL = u"/doc";
            TagFixture f(options);
            auto node = oneTag(f.ok(u"x\\label{a}"));
            if (node) {
                expect(tagText(*node) == u"[N11]");
                expect(attr(*node, u"id") == u"id:a");
            }
            auto refs = tagRefs(f.ok(u"\\ref{a}+\\eqref{a}", false));
            expect(refs.size() == 2_u);
            if (refs.size() == 2) {
                expect(tagText(*refs[0]) == u"N11");
                expect(tagText(*refs[1]) == u"ref:N11");
                expect(attr(*refs[0], u"href") == u"/doc/to/id:a");
            }
            auto updated = customFormats();
            updated.number = &updatedNumber;
            updated.tag = &stringTag;
            updated.ref = TagTextFormatter{};
            f.jax->options.tagformat = std::move(updated);
            node = oneTag(f.ok(u"y\\label{b}"));
            if (node) {
                expect(tagText(*node) == u"<U2>");
                expect(tagNodes(*node, NodeKind::mtext).size() == 1_u);
            }
            refs = tagRefs(f.ok(u"\\eqref{b}", false));
            if (!refs.empty())
                expect(tagText(*refs[0]) == u"<U2>");
            node = oneTag(f.ok(u"x\\tag{X}"));
            if (node)
                expect(tagText(*node) == u"<X>");
            node = oneTag(f.ok(u"x\\tag*{X}"));
            if (node)
                expect(tagText(*node) == u"X");
            f.jax->options.tagformat.reset();
            expect(throws<std::logic_error>([&] { f.jax->parseOptions().tags->formatTag(u"X"); }));
            f.jax->options.tagformat = TagsFactory::formatters();
            node = oneTag(f.ok(u"z"));
            if (node)
                expect(tagText(*node) == u"(3)");
        };
    "tagformat none all ams and unknown parent choices preserve actual automatic numbering rules"_test =
        [] {
            struct Case {
                std::u16string_view mode, source;
                std::size_t count;
            };
            for (auto const &item : {Case{u"none", u"x", 0},
                                     {u"none", u"\\begin{equation}x\\end{equation}", 0},
                                     {u"none", u"x\\tag{E}", 1},
                                     {u"all", u"x", 1},
                                     {u"all", u"\\begin{equation*}x\\end{equation*}", 0},
                                     {u"ams", u"x", 0},
                                     {u"ams", u"\\begin{equation}x\\end{equation}", 1},
                                     {u"ams", u"\\begin{align}x&=y\\\\a&=b\\end{align}", 2}}) {
                TexOptionValues options;
                options.tags = std::u16string(item.mode);
                TagFixture f(options);
                expect(tagCells(f.ok(std::u16string(item.source))).size() == item.count);
            }
            TexOptionValues options;
            options.tags = u"unregistered";
            TagFixture unknown(options);
            expect(tagCells(unknown.ok(u"x")).empty());
            auto node = oneTag(unknown.ok(u"x\\tag{E}"));
            if (node)
                expect(tagText(*node) == u"(E)");
        };
    "tagformat labels and reference URLs retain Unicode whitespace percent and reserved characters"_test =
        [] {
            struct Case {
                std::u16string_view label, id, url;
            };
            for (auto const &item : {Case{u"a b", u"mjx-eqn:a_b", u"/doc#mjx-eqn%3Aa_b"},
                                     {u"α😀", u"mjx-eqn:α😀", u"/doc#mjx-eqn%3A%CE%B1%F0%9F%98%80"},
                                     {u"a%/:?#[]()", u"mjx-eqn:a%/:?#[]()",
                                      u"/doc#mjx-eqn%3Aa%25%2F%3A%3F%23%5B%5D()"},
                                     {u"a\u00a0b", u"mjx-eqn:a_b", u"/doc#mjx-eqn%3Aa_b"}}) {
                TexOptionValues options;
                options.baseURL = u"/doc";
                TagFixture f(options);
                auto node = oneTag(f.ok(u"x\\label{" + std::u16string(item.label) + u"}"));
                if (node)
                    expect(std::ranges::equal(attr(*node, u"id"), item.id));
                auto refs = tagRefs(f.ok(u"\\eqref{" + std::u16string(item.label) + u"}", false));
                if (!refs.empty())
                    expect(std::ranges::equal(attr(*refs[0], u"href"), item.url));
            }
        };
    "tagformat separate input jaxes retain independent callbacks counters and absent package defaults"_test =
        [] {
            TexOptionValues options;
            options.tagformat = customFormats();
            TagFixture a(options), b, absent(options, false);
            auto node = oneTag(a.ok(u"x"));
            if (node)
                expect(tagText(*node) == u"[N11]");
            node = oneTag(b.ok(u"x"));
            if (node)
                expect(tagText(*node) == u"(1)");
            node = oneTag(a.ok(u"y"));
            if (node)
                expect(tagText(*node) == u"[N12]");
            node = oneTag(absent.ok(u"x"));
            if (node)
                expect(tagText(*node) == u"(1)");
        };
    "tagformat preserves custom parent behavior while formatting defaults replace parent formatters"_test =
        [] {
            TexOptionValues options;
            options.tags = u"parent";
            TagFixture selected(options, true, true), absent(options, false, true);
            auto node = oneTag(selected.ok(u"x"));
            if (node)
                expect(tagText(*node) == u"(1)");
            expect(selected.parent.finalized > 0_u);
            auto tables = tagNodes(selected.ok(u"y"), NodeKind::mtable);
            if (!tables.empty())
                expect(attr(*tables[0], u"data-parent") == u"yes");
            node = oneTag(absent.ok(u"x"));
            if (node)
                expect(tagText(*node) == u"parent:P1");
            expect(absent.jax->parseOptions().tags->formatUrl(u"x", u"base") == u"base!x");
        };
    "tagformat parser errors and duplicate labels recover before subsequent numbering"_test = [] {
        for (auto const &[source, error] :
             {std::pair<std::u16string_view, std::string_view>{u"\\tag", "MissingArgFor"},
              {u"x\\tag{X}\\tag{Y}", "MultipleCommand"},
              {u"x\\label{a}\\label{b}", "MultipleCommand"},
              {u"\\unknown", "UndefinedControlSequence"}}) {
            TagFixture f;
            f.compile(std::u16string(source));
            expect(f.error == error);
            auto node = oneTag(f.ok(u"x\\label{after}"));
            if (node)
                expect(tagText(*node) == u"(1)");
        }
        TagFixture f;
        f.ok(u"x\\label{a}");
        f.compile(u"y\\label{a}");
        expect(f.error == "MultipleLabel");
        f.ok(u"z\\label{after}");
    };
    "tagformat repeated construction and reconfiguration retain bounded catalogs and owned parent destruction"_test =
        [] {
            TexOptionValues options;
            options.tags = u"parent";
            TagFixture f(options, true, true);
            const auto registrySize = f.registry.keys().size();
            const auto catalogSize = f.jax->configuration().tags.size();
            auto supplied = options;
            supplied.packages = std::vector<PackageSelection>{
                {u"base", {}}, {u"ams", {}}, {u"tagformat-parent", {}}, {u"tagformat", {}}};
            supplied.formatError = &formatTagError;
            for (std::size_t i = 0; i < 8; ++i) {
                expect(f.parent.alive == 1_u);
                const auto before = f.jax->configuration().tags.size();
                tagformatConfig(f.jax->configuration(), *f.jax);
                expect(f.jax->configuration().tags.size() == before);
                f.ok(u"x");
                f.request.reset();
                f.jax.reset();
                expect(f.parent.alive == 0_u);
                expect(f.parent.created == f.parent.destroyed);
                f.jax =
                    std::make_unique<TeX>(f.registry, f.maps, f.callbacks, f.warnings, supplied);
                f.request = std::make_unique<TeXRequest>(*f.jax, f.document.factory());
                expect(f.registry.keys().size() == registrySize);
                expect(f.jax->configuration().tags.size() == catalogSize);
            }
            f.request.reset();
            f.jax.reset();
            expect(f.parent.alive == 0_u);
            expect(f.parent.created == f.parent.destroyed);
        };
};
