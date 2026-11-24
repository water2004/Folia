#include "math/support/tex_fixture.hpp"
#include "math/support/fixture_values.hpp"
import folia.math.input.tex.mathtools.tags;
import folia.math.input.tex.cases.configuration;
import folia.math.input.tex.tagformat.tag_format_configuration;
import folia.math.util.utf16;

namespace {
using namespace std::literals;
std::u16string mtNumber(std::int64_t number) {
    auto text = std::to_string(number);
    return {text.begin(), text.end()};
}
std::u16string mtFormattedNumber(std::int64_t number) {
    return u"N" + mtNumber(number);
}
TagFormatValue mtFormattedTag(std::u16string_view tag) {
    return std::vector<std::u16string>{u"<", std::u16string(tag), u">"};
}
TagFormatValue mtFormattedRef(std::u16string_view tag) {
    return u"ref:" + std::u16string(tag);
}
MathtoolsTags::Formats mtForms() {
    return {{u"box", {u"[", u"]", u""}}, {u"bold", {u"<", u">", u"\\mathbf"}}};
}
std::u16string mtText(TagFormatValue const &value) {
    if (auto text = std::get_if<std::u16string>(&value))
        return *text;
    std::u16string result;
    for (auto const &part : std::get<std::vector<std::u16string>>(value))
        result += part;
    return result;
}
struct MtParentCounts {
    int made{}, alive{}, destroyed{}, finalized{};
};
class MtParentTags final : public AllTags {
    MtParentCounts &counts_;

  public:
    MtParentTags(ParseOptions &options, MtParentCounts &counts)
        : AllTags(options), counts_(counts) {
        ++counts_.made;
        ++counts_.alive;
    }
    ~MtParentTags() override {
        --counts_.alive;
        ++counts_.destroyed;
    }
    MmlNode &finalize(MmlNode &node, TexEnvironment const &env) override {
        ++counts_.finalized;
        return AllTags::finalize(node, env);
    }

  protected:
    TagFormatValue formatTagDefault(std::u16string_view tag) override {
        return u"parent:" + std::u16string(tag);
    }
};
thread_local std::string *mtError{};
MmlNode &mtFormatError(TeX &jax, TexError const &error) {
    if (mtError)
        *mtError = error.id();
    return jax.formatError(error);
}
constexpr std::array<MacroHandler, 2> mtCallbacks{&BaseMethods::Macro,
                                                  &AmsMethods::HandleDeclareOp};
// This fixture selects the real tag configuration helper only. It does not
// register or claim support for the remaining Mathtools commands/environments.
struct MtTagFixture {
    MtParentCounts parent;
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{mtCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit MtTagFixture(std::u16string_view mode = u"all", bool tagformat = false,
                          bool reverse = false) {
        AmsConfiguration::initializeModule(registry, maps);
        CasesConfiguration::initializeModule(registry, maps);
        TagFormatConfiguration::initializeModule(registry);
        ConfigurationDefinition tagDefinition;
        tagDefinition.config = ProcessorSpec<ConfigMethod>{&MathtoolsTagFormat, reverse ? 15. : 5.};
        Configuration::create(registry, u"mathtools-tags-test", std::move(tagDefinition));
        ConfigurationDefinition parentDefinition;
        parentDefinition.tags = {{u"parent", [this](ParseOptions &owner) -> std::unique_ptr<Tags> {
                                      return std::make_unique<MtParentTags>(owner, parent);
                                  }}};
        Configuration::create(registry, u"mathtools-parent-test", std::move(parentDefinition));
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}},
                                                         {u"ams", {}},
                                                         {u"cases", {}},
                                                         {u"mathtools-parent-test", {}},
                                                         {u"mathtools-tags-test", {}}};
        if (tagformat) {
            options.packages->push_back({u"tagformat", {}});
            options.tagformat =
                TagFormatOptions{.number = &mtFormattedNumber, .tag = &mtFormattedTag};
        }
        options.mathtools.emplace().tagforms = mtForms();
        options.tags = std::u16string(mode);
        options.maxMacros = 1000;
        options.formatError = &mtFormatError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MathtoolsTags &forms() {
        auto result = dynamic_cast<MathtoolsTags *>(jax->parseOptions().tags->tagForm());
        if (!result)
            throw std::logic_error("Missing Mathtools tag forms");
        return *result;
    }
    void select(std::u16string_view id) {
        if (id.empty())
            forms().mtCurrent.reset();
        else
            forms().mtCurrent = forms().mtFormats.at(std::u16string(id));
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{mtError};
            ~Restore() {
                mtError = old;
            }
        } restore;
        mtError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(true);
        auto &tree = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return tree;
    }
    MmlNode &ok(std::u16string source) {
        auto &tree = compile(std::move(source));
        expect(error.empty()) << error;
        return tree;
    }
};
std::u16string mtTree(MmlNode const &root) {
    std::u16string result;
    auto add = [&](std::u16string_view text) {
        result += mtNumber(text.size()) + u":" + std::u16string(text);
    };
    auto number = [](double n) -> std::u16string {
        if (n == 0)
            return u"0";
        std::array<char, 64> buffer;
        auto end = std::to_chars(buffer.data(), buffer.data() + buffer.size(), n).ptr;
        return {buffer.data(), end};
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
        add(mtNumber(attrs.size()));
        for (auto const &[name, value] : attrs) {
            add(name);
            std::visit(
                [&](auto const &v) {
                    using T = std::remove_cvref_t<decltype(v)>;
                    if constexpr (std::same_as<T, AttributeNull>) {
                        add(u"null");
                        add(u"");
                    } else if constexpr (std::same_as<T, bool>) {
                        add(u"boolean");
                        add(v ? u"true" : u"false");
                    } else if constexpr (std::same_as<T, double>) {
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
        add(mtNumber(node->isToken() ? 0 : node->children().size()));
        if (!node->isToken())
            for (auto child : node->children())
                self(self, child);
    };
    visit(visit, &root);
    return result;
}
void mtCheckTree(MmlNode &root, std::u16string_view expected) {
    const auto actual = mtTree(root);
    if (std::ranges::equal(actual, expected))
        expect(true);
    else {
        auto mismatch = std::ranges::mismatch(actual, expected);
        auto offset = static_cast<std::size_t>(mismatch.in1 - actual.begin());
        auto start = offset > 40 ? offset - 40 : 0;
        expect(false) << "Mathtools tags Source difference at " << offset << " actual "
                      << folia::math::utf16::encode(std::u16string_view(actual).substr(start, 180))
                      << " expected " << folia::math::utf16::encode(expected.substr(start, 180));
    }
    root.walkTree([&](auto &node) {
        for (auto child : node.children())
            if (child)
                expect(child->parent() == &node);
    });
}
struct MtTagTree {
    std::u16string mode;
    bool tagformat, reverse;
    std::u16string selected, source, snapshot;
};
struct MtTagStep {
    int action;
    std::u16string source, snapshot;
};
const auto& mtTagTrees() {
    static const auto result = math_test::fixtures::records<MtTagTree>(
        "math/fixtures/packages/mathtools_tags/trees.tsv", 6, 72, [](const auto& r) {
            return MtTagTree{folia::test::data::decode_utf16(r[0]), math_test::fixtures::boolean(r[1]), math_test::fixtures::boolean(r[2]), folia::test::data::decode_utf16(r[3]), folia::test::data::decode_utf16(r[4]), folia::test::data::decode_utf16(r[5])};
        });
    return result;
}

const auto& mtTagSequence() {
    static const auto result = math_test::fixtures::records<MtTagStep>(
        "math/fixtures/packages/mathtools_tags/sequence.tsv", 3, 8, [](const auto& r) {
            return MtTagStep{math_test::fixtures::integer<int>(r[0]), folia::test::data::decode_utf16(r[1]), folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

} // namespace

suite mathtools_tags = [] {
    "mathtools tags preserve named formats selection renewal snapshot and reset"_test = [] {
        MtTagFixture f;
        auto &tags = *f.jax->parseOptions().tags;
        expect(f.forms().mtFormats == mtForms());
        expect(!f.forms().mtCurrent);
        expect(mtText(tags.formatTag(u"7")) == u"(7)");
        f.select(u"box");
        expect(mtText(tags.formatTag(u"7")) == u"[7]");
        f.forms().mtFormats[u"box"] = {u"(", u")", u""};
        expect(mtText(tags.formatTag(u"7")) == u"[7]");
        f.select(u"box");
        expect(mtText(tags.formatTag(u"7")) == u"(7)");
        f.select(u"bold");
        expect(mtText(tags.formatTag(u"7")) == u"<\\mathbf{7}>");
        auto *product = &tags;
        auto *form = tags.tagForm();
        f.jax->reset();
        expect(f.jax->parseOptions().tags.get() == product);
        expect(tags.tagForm() == form);
        expect(mtText(tags.formatTag(u"7")) == u"<\\mathbf{7}>");
        f.select(u"");
        expect(mtText(tags.formatTag(u"7")) == u"(7)");
    };
    "mathtools tags preserve actual selected parent product and custom finalization"_test = [] {
        MtTagFixture none(u"none"), all(u"all"), ams(u"ams"), cases(u"cases");
        expect(dynamic_cast<NoTags *>(none.jax->parseOptions().tags.get()) != nullptr);
        expect(dynamic_cast<AllTags *>(all.jax->parseOptions().tags.get()) != nullptr);
        expect(dynamic_cast<AmsTags *>(ams.jax->parseOptions().tags.get()) != nullptr);
        expect(dynamic_cast<CasesTags *>(cases.jax->parseOptions().tags.get()) != nullptr);
        MtTagFixture parent(u"parent");
        expect(dynamic_cast<MtParentTags *>(parent.jax->parseOptions().tags.get()) != nullptr);
        expect(parent.parent.made == 1_i);
        expect(parent.parent.alive == 1_i);
        expect(mtText(parent.jax->parseOptions().tags->formatTag(u"X")) == u"parent:X");
        parent.select(u"box");
        expect(mtText(parent.jax->parseOptions().tags->formatTag(u"X")) == u"[X]");
        parent.ok(u"x");
        expect(parent.parent.finalized == 1_i);
        parent.request.reset();
        parent.jax.reset();
        expect(parent.parent.alive == 0_i);
        expect(parent.parent.destroyed == 1_i);
    };
    "mathtools tags honor both formatter orders explicit ref callback and later updates"_test = [] {
        for (bool reverse : {false, true}) {
            MtTagFixture f(u"cases", true, reverse);
            auto &tags = *f.jax->parseOptions().tags;
            f.select(u"box");
            expect(mtText(tags.formatTag(u"7")) == (reverse ? u"[7]" : u"<7>"));
            expect(mtText(tags.formatRef(u"7")) == (reverse ? u"[7]" : u"<7>"));
            f.jax->options.tagformat->ref = &mtFormattedRef;
            expect(mtText(tags.formatRef(u"7")) == u"ref:7");
            f.select(u"");
            expect(mtText(tags.formatTag(u"7")) == u"<7>");
            f.jax->options.tagformat->tag = +[](std::u16string_view tag) -> TagFormatValue {
                return u"updated:" + std::u16string(tag);
            };
            expect(mtText(tags.formatTag(u"7")) == u"updated:7");
            auto *original = &tags;
            const auto creators = f.jax->configuration().tags.size();
            MathtoolsTagFormat(f.jax->configuration(), *f.jax);
            expect(f.jax->parseOptions().tags.get() == original);
            expect(f.jax->configuration().tags.size() <= creators + 1);
        }
    };
    "mathtools tags retain parent auto numbering under Tagformat"_test = [] {
        for (std::u16string_view mode : {u"none", u"all", u"ams", u"cases"}) {
            MtTagFixture f(mode, true);
            auto &tags = *f.jax->parseOptions().tags;
            auto &equation = f.jax->parseOptions().request().equation;
            tags.start("subnumcases", true, true);
            tags.autoTag();
            expect(equation.counter.equation == (mode == u"none" ? 0 : 1));
            expect(equation.current_tag.tag ==
                   (mode == u"none" ? std::nullopt : std::optional<std::u16string>(u"N1")));
            tags.clearTag();
            tags.autoTag();
            expect(equation.counter.equation == (mode == u"none" ? 0 : mode == u"cases" ? 1 : 2));
            tags.end();
        }
    };
    "mathtools tags match actual Source MML with parent numbering and refs"_test = [] {
        for (auto const &row : mtTagTrees()) {
            MtTagFixture f(row.mode, row.tagformat, row.reverse);
            f.select(row.selected);
            mtCheckTree(f.ok(std::u16string(row.source)), row.snapshot);
        }
    };
    "mathtools tags match Source state sequences without claiming macro registration"_test = [] {
        MtTagFixture f;
        for (auto const &row : mtTagSequence()) {
            switch (row.action) {
            case 0:
                break;
            case 1:
                f.select(u"box");
                break;
            case 2:
                f.forms().mtFormats[u"box"] = {u"(", u")", u""};
                break;
            case 3:
                f.select(u"box");
                break;
            case 4:
                f.select(u"");
                break;
            case 5:
                f.select(u"bold");
                break;
            default:
                throw std::logic_error("Unknown Mathtools tag fixture action");
            }
            mtCheckTree(f.ok(std::u16string(row.source)), row.snapshot);
        }
    };
    "mathtools tags release selected products and leave shared catalogs bounded"_test = [] {
        MtTagFixture f(u"parent", true);
        auto options = f.jax->options;
        options.tags = u"parent";
        const auto registrySize = f.registry.keys().size();
        f.request.reset();
        f.jax.reset();
        for (int i = 0; i < 48; ++i) {
            {
                TeX jax(f.registry, f.maps, f.callbacks, f.warnings, options);
                TeXRequest request(jax, f.document.factory());
                expect(dynamic_cast<MtParentTags *>(jax.parseOptions().tags.get()) != nullptr);
                expect(dynamic_cast<MathtoolsTags *>(jax.parseOptions().tags->tagForm()) !=
                       nullptr);
                expect(f.parent.alive == 1_i);
                std::vector<std::u16string_view> names;
                for (auto const &entry : jax.configuration().tags)
                    names.push_back(entry.name);
                std::ranges::sort(names);
                constexpr std::array<std::u16string_view, 6> expected{
                    u"ams", u"base", u"cases", u"configTags", u"mathtoolsTags", u"parent"};
                expect(std::ranges::equal(names, expected));
                expect(f.registry.keys().size() == registrySize);
            }
            expect(f.parent.alive == 0_i);
            expect(f.parent.made == f.parent.destroyed);
        }
    };
};
