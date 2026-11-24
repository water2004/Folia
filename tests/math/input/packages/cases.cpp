#include "math/support/tex_fixture.hpp"
#include "math/support/fixture_values.hpp"
import folia.math.input.tex.cases.configuration;
import folia.math.input.tex.tagformat.tag_format_configuration;
import folia.math.util.utf16;

namespace {
using namespace std::literals;
constexpr std::array<MacroHandler, 2> casesCallbacks{&BaseMethods::Macro,
                                                     &AmsMethods::HandleDeclareOp};
thread_local std::string *casesError{};
MmlNode &casesFormatError(TeX &jax, TexError const &e) {
    if (casesError)
        *casesError = e.id();
    return jax.formatError(e);
}
std::u16string digits(std::int64_t n) {
    auto s = std::to_string(n);
    return {s.begin(), s.end()};
}
std::u16string casesTagNumber(std::int64_t n) {
    return u"N" + digits(n);
}
struct CasesFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{casesCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit CasesFixture(std::u16string_view mode = u"cases", std::u16string_view side = u"right",
                          bool format = false) {
        AmsConfiguration::initializeModule(registry, maps);
        CasesConfiguration::initializeModule(registry, maps);
        TagFormatConfiguration::initializeModule(registry);
        TexOptionValues options;
        options.packages =
            std::vector<PackageSelection>{{u"base", {}}, {u"ams", {}}, {u"cases", {}}};
        if (format) {
            options.packages->push_back({u"tagformat", {}});
            options.tagformat = TagFormatOptions{.number = &casesTagNumber};
        }
        options.tags = std::u16string(mode);
        options.tagSide = std::u16string(side);
        options.maxMacros = 1000;
        options.formatError = &casesFormatError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{casesError};
            ~Restore() {
                casesError = old;
            }
        } restore;
        casesError = &error;
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
std::vector<MmlNode *> caseNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> nodes;
    root.walkTree([&](auto &n) {
        if (n.kind() == kind)
            nodes.push_back(&n);
    });
    return nodes;
}
std::u16string caseText(MmlNode &root) {
    std::u16string text;
    root.walkTree([&](auto &n) {
        if (n.isToken())
            text += NodeUtil::getText(n);
    });
    return text;
}
std::u16string casesSnapshot(MmlNode const &root) {
    std::u16string out;
    auto add = [&](std::u16string_view s) {
        out += digits(static_cast<std::int64_t>(s.size())) + u":" + std::u16string(s);
    };
    auto number = [](double n) -> std::u16string {
        if (n == 0)
            return u"0";
        std::array<char, 64> buf;
        auto end = std::to_chars(buf.data(), buf.data() + buf.size(), n).ptr;
        return {buf.data(), end};
    };
    auto visit = [&](auto &&self, MmlNode const *node) -> void {
        if (!node) {
            add(u"null");
            return;
        }
        auto kind = nodeKindName(node->kind());
        add(std::u16string(kind.begin(), kind.end()));
        auto attrs = node->attributes().ownEntries();
        std::ranges::sort(attrs, {}, &AttributeSet::Entry::first);
        add(digits(static_cast<std::int64_t>(attrs.size())));
        for (auto const &[key, value] : attrs) {
            add(key);
            std::visit(
                [&](auto const &value) {
                    using T = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, AttributeNull>) {
                        add(u"null");
                        add(u"");
                    } else if constexpr (std::is_same_v<T, bool>) {
                        add(u"boolean");
                        add(value ? u"true" : u"false");
                    } else if constexpr (std::is_same_v<T, double>) {
                        add(u"number");
                        add(number(value));
                    } else {
                        add(u"string");
                        add(value);
                    }
                },
                value);
        }
        add(node->isToken() ? NodeUtil::getText(*node) : u"");
        add(digits(static_cast<std::int64_t>(node->isToken() ? 0 : node->children().size())));
        if (!node->isToken())
            for (auto child : node->children())
                self(self, child);
    };
    visit(visit, &root);
    return out;
}
void checkTree(MmlNode &tree, std::u16string_view expected) {
    auto actual = casesSnapshot(tree);
    auto equal = std::ranges::equal(actual, expected);
    if (!equal) {
        auto mismatch = std::ranges::mismatch(actual, expected);
        auto offset = static_cast<std::size_t>(mismatch.in1 - actual.begin());
        auto start = offset > 40 ? offset - 40 : 0;
        expect(false) << "Cases Source difference at " << offset << " actual "
                      << folia::math::utf16::encode(std::u16string_view(actual).substr(start, 180))
                      << " expected "
                      << folia::math::utf16::encode(expected.substr(start, 180));
    } else
        expect(true);
    tree.walkTree([&](auto &node) {
        for (auto child : node.children())
            if (child)
                expect(child->parent() == &node);
    });
}
struct CasesTree {
    std::u16string source, mode, tagSide, snapshot;
    bool tagformat;
};
struct CasesError {
    std::u16string source;
    std::string id;
    std::u16string recoveryTag;
};
struct CasesFormat {
    std::optional<std::int64_t> sub;
    std::u16string expected;
};
struct CasesSequence {
    std::u16string source, snapshot;
};
const auto& casesTrees() {
    static const auto result = math_test::fixtures::records<CasesTree>(
        "math/fixtures/packages/cases/trees.tsv", 5, 32, [](const auto& r) {
            return CasesTree{folia::test::data::decode_utf16(r[0]), folia::test::data::decode_utf16(r[1]), folia::test::data::decode_utf16(r[2]), folia::test::data::decode_utf16(r[3]), math_test::fixtures::boolean(r[4])};
        });
    return result;
}

const auto& casesErrors() {
    static const auto result = math_test::fixtures::records<CasesError>(
        "math/fixtures/packages/cases/errors.tsv", 3, 12, [](const auto& r) {
            return CasesError{folia::test::data::decode_utf16(r[0]), math_test::fixtures::bytes(r[1]), folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

const auto& casesFormats() {
    static const auto result = math_test::fixtures::records<CasesFormat>(
        "math/fixtures/packages/cases/formats.tsv", 2, 8, [](const auto& r) {
            return CasesFormat{(r[0] == "null" ? std::nullopt : std::optional<std::int64_t>{math_test::fixtures::integer<std::int64_t>(r[0])}), folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

const auto& casesSequence() {
    static const auto result = math_test::fixtures::records<CasesSequence>(
        "math/fixtures/packages/cases/sequence.tsv", 2, 5, [](const auto& r) {
            return CasesSequence{folia::test::data::decode_utf16(r[0]), folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

class CasesNumberProbe : public CasesTags {
  public:
    using CasesTags::CasesTags;
    using CasesTags::formatNumberDefault;
};
struct Forwarded {
    std::u16string name;
    bool end{}, type{};
    std::vector<TexArgument> arguments;
};
thread_local Forwarded *forwardedCases{};
StackEntry casesEnvironmentProbe(TexParser &, FrameHandle const &frame,
                                 std::span<TexArgument const> args) {
    if (forwardedCases) {
        forwardedCases->name = frame->getName().value_or(u"");
        auto item = dynamic_cast<CasesBeginItem *>(frame.get());
        forwardedCases->type = item != nullptr;
        forwardedCases->end = item && item->end;
        forwardedCases->arguments.assign(args.begin(), args.end());
    }
    return {};
}
suite cases_suite = [] {
    "cases single configuration owns real factory tags maps callbacks and full environment arguments"_test =
        [] {
            CasesFixture f;
            auto config = f.registry.get(u"cases");
            expect(config->priority == 5.0);
            expect(config->items.size() == 1_u);
            expect(config->tags.size() == 1_u);
            expect(std::ranges::equal(config->tags[0].name, u"cases"sv));
            expect(dynamic_cast<CasesTags *>(f.jax->parseOptions().tags.get()) != nullptr);
            auto &factory = *f.jax->parseOptions().itemFactory;
            auto frame = factory.create(StackKind::cases_begin);
            expect(dynamic_cast<CasesBeginItem *>(frame.get()) != nullptr);
            expect(frame->kind() == StackKind::cases_begin);
            auto map = f.maps.getMap(u"cases-env");
            expect(std::get<EnvironmentHandler>(*map->parser()) == &CasesMethods::environment);
            for (auto name : {u"numcases", u"subnumcases"}) {
                auto macro = std::get<Macro const *>(map->lookup(name));
                expect(std::get<EnvironmentBodyHandler>(macro->function) ==
                       &CasesMethods::NumCases);
                expect(macro->arguments.size() == 1_u);
                expect(
                    std::ranges::equal(std::get<std::u16string>(macro->arguments[0]), u"cases"sv));
            }
            auto entry = std::get<Macro const *>(f.maps.getMap(u"cases-macros")->lookup(u"&"));
            expect(std::get<MacroHandler>(entry->function) == &CasesMethods::Entry);
            expect(entry->arguments.empty());
            for (auto const &h : config->handler) {
                if (h.kind == HandlerKind::character) {
                    expect(h.maps.size() == 1_u);
                    expect(std::ranges::equal(h.maps[0], u"cases-macros"sv));
                }
                if (h.kind == HandlerKind::environment) {
                    expect(h.maps.size() == 1_u);
                    expect(std::ranges::equal(h.maps[0], u"cases-env"sv));
                }
            }
            TexParser parser(u"x", {}, f.jax->parseOptions());
            Forwarded result;
            forwardedCases = &result;
            struct Clear {
                ~Clear() {
                    forwardedCases = nullptr;
                }
            } clear;
            const std::array<TexArgument, 3> args{std::u16string(u"cases"), 2.0, false};
            CasesMethods::environment(parser, u"probe", &casesEnvironmentProbe, args);
            expect(result.type);
            expect(result.end);
            expect(std::ranges::equal(result.name, u"probe"sv));
            expect(result.arguments.size() == 3_u);
            if (result.arguments.size() == 3) {
                expect(
                    std::ranges::equal(std::get<std::u16string>(result.arguments[0]), u"cases"sv));
                expect(std::get<double>(result.arguments[1]) == 2.0);
                expect(!std::get<bool>(result.arguments[2]));
            }
            parser.mml();
        };
    "cases environments text scanner tag modes and tagformat match actual Source trees"_test = [] {
        expect(casesTrees().size() == 32_u);
        for (auto const &c : casesTrees()) {
            CasesFixture f(c.mode, c.tagSide, c.tagformat);
            checkTree(f.ok(std::u16string(c.source)), c.snapshot);
        }
    };
    "cases left expression labels references and subnumber counters persist across formulas"_test =
        [] {
            CasesFixture f;
            for (auto const &c : casesSequence())
                checkTree(f.ok(std::u16string(c.source)), c.snapshot);
            auto const &labels = f.jax->session().labels();
            struct Label {
                std::u16string_view key, tag;
            };
            for (auto c :
                 std::array{Label{u"left", u"2"}, Label{u"row", u"1"}, Label{u"next", u"3"},
                            Label{u"suba", u"4a"}, Label{u"subb", u"4b"}})
                expect(std::ranges::equal(labels.at(std::u16string(c.key)).tag, c.tag));
        };
    "cases original errors preserve consumed numbering and recover parser borrows"_test = [] {
        expect(casesErrors().size() == 12_u);
        for (auto const &c : casesErrors()) {
            CasesFixture f;
            f.compile(std::u16string(c.source));
            expect(f.error == c.id);
            expect(f.jax->parseOptions().request().parsers.empty());
            f.ok(u"x");
            f.ok(u"\\begin{numcases}{g=}z & recovered\\label{after}\\end{numcases}");
            expect(std::ranges::equal(f.jax->session().labels().at(u"after").tag, c.recoveryTag));
        }
    };
    "cases tags preserve explicit zero UTF16 subnumbers and reset their local counter"_test = [] {
        CasesFixture f;
        CasesNumberProbe tag(f.jax->parseOptions());
        for (auto const &c : casesFormats())
            expect(std::ranges::equal(tag.formatNumberDefault(7, c.sub), c.expected));
        auto &eq = f.jax->parseOptions().request().equation;
        tag.start("subnumcases", true, true);
        tag.autoTag();
        expect(std::ranges::equal(*eq.current_tag.tag, u"1a"sv));
        tag.clearTag();
        tag.autoTag();
        expect(std::ranges::equal(*eq.current_tag.tag, u"1b"sv));
        tag.end();
        tag.start("numcases-left", true, true);
        tag.autoTag();
        expect(std::ranges::equal(*eq.current_tag.tag, u"1"sv));
        tag.end();
        tag.start("subnumcases", true, true);
        tag.autoTag();
        expect(std::ranges::equal(*eq.current_tag.tag, u"2a"sv));
        tag.end();
    };
    "cases first matching end pops one activation and second end finalizes the same frame"_test =
        [] {
            CasesFixture f;
            auto &factory = *f.jax->parseOptions().itemFactory;
            auto frame = factory.create(StackKind::cases_begin);
            auto &begin = dynamic_cast<CasesBeginItem &>(*frame);
            begin.name = u"numcases";
            begin.end = true;
            auto end = factory.create(StackKind::end);
            dynamic_cast<EndItem &>(*end).name = u"numcases";
            auto first = begin.checkItem(end);
            expect(first.action == CheckAction::replay);
            expect(first.replacements.empty());
            expect(!begin.end);
            auto second = begin.checkItem(end);
            expect(second.action == CheckAction::replay);
            expect(second.replacements.size() == 1_u);
            if (!second.replacements.empty())
                expect(std::get<FrameHandle>(second.replacements[0])->kind() == StackKind::mml);
            auto wrong = factory.create(StackKind::end);
            dynamic_cast<EndItem &>(*wrong).name = u"other";
            texError([&] { begin.checkItem(wrong); }, "EnvBadEnd");
            texError([&] { begin.checkItem(factory.create(StackKind::stop)); }, "EnvMissingEnd");
            Stack stack(factory);
            auto shared = factory.create(StackKind::cases_begin);
            auto &item = dynamic_cast<CasesBeginItem &>(*shared);
            item.name = u"numcases";
            item.end = true;
            stack.Push(shared);
            stack.Push(shared);
            expect(stack.height() == 3_u);
            expect(stack.Top() == stack.Top(2));
            stack.Push(end);
            expect(stack.height() == 2_u);
            expect(stack.Top() == shared.get());
            expect(!item.end);
            stack.Push(end);
            expect(stack.height() == 1_u);
            expect(stack.Top()->kind() == StackKind::start);
        };
};
} // namespace
