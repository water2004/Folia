#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.setoptions.set_options_configuration;
import folia.math.input.tex.base_methods;
import folia.math.input.tex.bbm.configuration;
import folia.math.input.tex.dsfont.configuration;
import folia.math.input.tex.bboldx.configuration;
import folia.math.input.tex.units.configuration;
import folia.math.input.tex.ams.configuration;
import folia.math.input.tex.color.configuration;
import folia.math.input.tex.noundefined.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.tagformat.tag_format_configuration;

namespace {
using namespace std::literals;
thread_local std::string *optionError{};
thread_local std::vector<std::u16string> *requireCalls{};
MmlNode &optionFormatError(TeX &jax, TexError const &e) {
    if (optionError)
        *optionError = e.id();
    return jax.formatError(e);
}
DispatchOutcome localRequire(TexParser &p, MacroInvocation const &invocation) {
    auto name = p.GetArgument(invocation.dispatch_token).value();
    if (requireCalls)
        requireCalls->push_back(name);
    p.Push(p.itemFactory().create(StackKind::null_item));
    return NoParseResult{};
}
DispatchOutcome oldSetOptions(TexParser &, MacroInvocation const &) {
    throw std::logic_error("Setoptions map must override the older macro");
}
constexpr std::array<MacroHandler, 2> optionCallbacks{&SetOptions, &localRequire};
struct OptionFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{optionCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    OptionFixture(TexOptionValues options = {}, std::vector<std::u16string> packages = {},
                  bool reversed = false, bool withRequire = false) {
        SetOptionsConfiguration::initializeModule(registry);
        TagFormatConfiguration::initializeModule(registry);
        BbmConfiguration::initializeModule(registry, maps);
        DsfontConfiguration::initializeModule(registry, maps);
        BboldxConfiguration::initializeModule(registry, maps);
        UnitsConfiguration::initializeModule(registry, maps);
        AmsConfiguration::initializeModule(registry, maps);
        ColorConfiguration::initializeModule(registry, maps);
        NoUndefinedConfiguration::initializeModule(registry);
        NewcommandConfiguration::initializeModule(registry, maps);
        options.packages = std::vector<PackageSelection>{};
        if (reversed)
            options.packages->push_back({u"setoptions", {}});
        options.packages->push_back({u"base", {}});
        if (!reversed)
            options.packages->push_back({u"setoptions", {}});
        for (auto const &package : packages)
            options.packages->push_back({package, {}});
        if (withRequire) {
            maps.registerMap(std::make_shared<CommandMap>(CommandMapInit{MacroMapInit{
                {u"setoptions-local-require", {}},
                {Macro(u"require", &localRequire), Macro(u"setOptions", &oldSetOptions)}}}));
            ConfigurationDefinition definition;
            definition.handler = {{HandlerKind::macro, {u"setoptions-local-require"}}};
            Configuration::create(registry, u"setoptions-local-require", std::move(definition));
            options.packages->push_back({u"setoptions-local-require", {}});
        }
        options.formatError = &optionFormatError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{optionError};
            ~Restore() {
                optionError = old;
            }
        } restore;
        optionError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        auto &result = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return result;
    }
    MmlNode &ok(std::u16string source) {
        auto &root = compile(std::move(source));
        expect(error.empty()) << error;
        return root;
    }
    void fails(std::u16string source, std::string_view id) {
        compile(std::move(source));
        expect(error == id);
        ok(u"x");
    }
};
std::vector<MmlNode *> optionNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> nodes;
    root.walkTree([&](auto &n) {
        if (n.kind() == kind)
            nodes.push_back(&n);
    });
    return nodes;
}
struct FilterCounts {
    int packages{}, options{}, values{};
    std::vector<std::u16string> keys;
};
thread_local FilterCounts *filterCounts{};
bool acceptPackage(TexParser &, std::u16string_view package) {
    if (filterCounts)
        ++filterCounts->packages;
    return !std::ranges::equal(package, u"skip"sv);
}
bool acceptOption(TexParser &, std::u16string_view, std::u16string_view key) {
    if (filterCounts)
        ++filterCounts->options;
    return !std::ranges::equal(key, u"skip"sv);
}
bool recordSkippedOption(TexParser &, std::u16string_view, std::u16string_view key) {
    if (filterCounts)
        filterCounts->keys.emplace_back(key);
    return false;
}
OptionInputValue uppercaseValue(TexParser &, std::u16string_view, std::u16string_view,
                                OptionInputValue const &input) {
    if (filterCounts)
        ++filterCounts->values;
    if (auto text = std::get_if<std::u16string>(&input)) {
        auto result = *text;
        for (auto &c : result)
            if (c >= u'a' && c <= u'z')
                c -= u'a' - u'A';
        return result;
    }
    return input;
}
OptionInputValue nullRegexp(TexParser &, std::u16string_view, std::u16string_view,
                            OptionInputValue const &) {
    return folia::math::regexp_host::RegExpRef{};
}
TagFormatValue optionReference(std::u16string_view value) {
    return u"custom:" + std::u16string(value);
}
enum class OptionKind { boolean, string, budget, regexp, complex };
template <auto Head, auto... Tail, class Owner> auto &optionSlot(Owner &owner) {
    auto &value = owner.*Head;
    if constexpr (sizeof...(Tail) == 0)
        return value;
    else {
        if (!value)
            value.emplace();
        return optionSlot<Tail...>(*value);
    }
}
struct OptionCase {
    std::u16string_view package, key;
    OptionKind kind;
    std::u16string_view input;
    void (*seed)(TexOptionValues &);
    bool (*matches)(TexOptionValues &);
};
template <auto... Members>
OptionCase optionCase(std::u16string_view package, std::u16string_view key, OptionKind kind,
                      std::u16string_view input) {
    return {
        package,
        key,
        kind,
        input,
        +[](TexOptionValues &options) {
            auto &value = optionSlot<Members...>(options);
            if (!value)
                value.emplace();
        },
        +[](TexOptionValues &options) {
            auto &value = optionSlot<Members...>(options);
            if (!value)
                return false;
            using T = typename std::remove_reference_t<decltype(value)>::value_type;
            if constexpr (std::is_same_v<T, bool>)
                return !*value;
            else if constexpr (std::is_same_v<T, std::variant<bool, std::u16string>>)
                return std::holds_alternative<bool>(*value) && !std::get<bool>(*value);
            else if constexpr (std::is_same_v<T, std::u16string>)
                return std::ranges::equal(*value, u"set-value"sv);
            else if constexpr (std::is_same_v<T, double>)
                return *value == 2048;
            else if constexpr (std::is_same_v<T, folia::math::regexp_host::RegExpRef>)
                return *value && std::ranges::equal((*value)->source(), u"^Q"sv);
            else if constexpr (std::is_same_v<T, std::variant<std::u16string,
                                                              folia::math::regexp_host::RegExpRef>>)
                return std::holds_alternative<std::u16string>(*value) &&
                       std::ranges::equal(std::get<std::u16string>(*value), u"set-value"sv);
            else
                return false;
        }};
}
#include "math/fixtures/setoptions.inc"
std::u16string assignment(OptionCase const &c, std::u16string_view value) {
    return u"\\setOptions[" + std::u16string(c.package) + u"]{" + std::u16string(c.key) + u"=" +
           std::u16string(value) + u"}";
}
thread_local std::vector<MmlNode *> *optionTokens{};
MmlNode &observeOptionToken(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                            std::optional<std::u16string_view> text) {
    auto &node = NodeFactory::createToken(factory, kind, std::move(definition), text);
    if (optionTokens)
        optionTokens->push_back(&node);
    return node;
}
suite setoptions_suite = [] {
    "setoptions original configuration metadata and default permissions"_test = [] {
        OptionFixture f;
        auto config = f.registry.get(u"setoptions");
        expect(config->priority == 3.0);
        expect(config->configMethod->priority == 3.0);
        expect(config->config() == &setoptionsConfig);
        expect(std::ranges::all_of(config->handler, [](auto const &h) { return h.maps.empty(); }));
        auto const &defaults = *config->options.setoptions;
        expect(defaults.allowPackageDefault.value());
        expect(defaults.allowOptionsDefault.value());
        expect(defaults.filterPackage.value() == &SetOptionsUtil::filterPackage);
        expect(defaults.filterOption.value() == &SetOptionsUtil::filterOption);
        expect(defaults.filterValue.value() == &SetOptionsUtil::filterValue);
        expect(defaults.allowOptions->size() == 6_u);
        expect(f.jax->options.findTeX.has_value());
        expect(!*f.jax->options.findTeX);
        auto const &tex =
            std::get<std::unordered_map<std::u16string, bool>>(defaults.allowOptions->at(u"tex"));
        expect(tex.size() == 9_u);
        for (auto key : {u"FindTeX", u"formatError", u"package", u"baseURL", u"tags", u"maxBuffer",
                         u"maxMaxros", u"macros", u"environments"})
            expect(!tex.at(key));
        for (auto key : {u"setoptions", u"autoload", u"require", u"configmacros", u"tagformat"})
            expect(!std::get<bool>(defaults.allowOptions->at(key)));
        auto found = f.jax->parseOptions().handlers.get(HandlerKind::macro)->lookup(u"setOptions");
        expect(std::get<MacroHandler>(std::get<Macro const *>(found)->function) == &SetOptions);
    };
    "setoptions complete Source schema presence and scalar writes"_test = [] {
        OptionFixture f;
        std::size_t scalar{}, complex{};
        for (auto const &c : optionCases) {
            auto saved = f.jax->options;
            if (!std::ranges::equal(c.key, u"FindTeX"sv))
                c.seed(f.jax->options);
            auto &permission = *f.jax->options.setoptions;
            permission.allowOptions->clear();
            permission.filterPackage = &acceptPackage;
            // Unported packages supply real option values here; only the field contract is tested.
            TexParser parser(u"", {}, f.jax->parseOptions());
            parser.mml();
            expect(SetOptionsUtil::filterOption(parser, c.package, c.key));
            if (c.kind == OptionKind::complex) {
                f.fails(assignment(c, u"bad"sv), "InvalidValue");
                ++complex;
            } else {
                f.ok(assignment(c, c.input));
                expect(c.matches(f.jax->options));
                ++scalar;
            }
            f.jax->options = std::move(saved);
        }
        expect(optionCases.size() == 96_u);
        expect(scalar == 59_u);
        expect(complex == 37_u);
    };
    "setoptions real fonts units grouping and independent options"_test = [] {
        OptionFixture f({}, {u"bbm", u"dsfont", u"bboldx", u"units"});
        struct Font {
            std::u16string_view source, variant;
        };
        for (auto const &row :
             std::array{Font{u"\\setOptions[bbm]{bold}\\mathbbm{A}", u"-bbm-bold"},
                        Font{u"\\setOptions[dsfont]{sans}\\mathds{A}", u"-ds-ss"},
                        Font{u"\\setOptions[bboldx]{light}\\mathbb{A}", u"-bboldx-light"}}) {
            auto nodes = optionNodes(f.ok(std::u16string(row.source)), NodeKind::mi);
            expect(nodes.size() == 1_u);
            if (!nodes.empty())
                expect(std::ranges::equal(attr(*nodes[0], u"mathvariant"sv), row.variant));
        }
        auto &root = f.ok(u"\\setOptions[units]{loose,ugly}\\units[2]{kg}+\\nicefrac{m}{s}");
        expect(std::ranges::equal(NodeUtil::getText(*optionNodes(root, NodeKind::mtext)[0]),
                                  u"\u00a0"sv));
        expect(!std::get<bool>(
            *optionNodes(root, NodeKind::mfrac)[0]->attributes().lookup(u"bevelled")));
        f.ok(u"{\\setOptions[units]{loose=false}} ");
        expect(!*f.jax->options.units->loose);
        f.jax->reset();
        expect(optionNodes(f.ok(u"\\units[2]{kg}"), NodeKind::mspace).size() == 1_u);
        f.ok(u"\\setOptions[bboldx]{bfbb,light=false}");
        expect(std::ranges::equal(
            attr(*optionNodes(f.ok(u"\\mathbb{A}"), NodeKind::mi)[0], u"mathvariant"sv),
            u"-bboldx-bold"sv));
        OptionFixture other({}, {u"units"});
        expect(!*other.jax->options.units->loose);
    };
    "setoptions real AMS color undefined and captured parsing options"_test = [] {
        OptionFixture f({}, {u"ams", u"color", u"noundefined"});
        auto &table = f.ok(u"\\setOptions[ams]{multlineWidth=73%,multlineIndent=2em}\\begin{"
                           u"multline}a\\\\b\\end{multline}");
        expect(std::ranges::equal(attr(*optionNodes(table, NodeKind::mtable)[0], u"width"sv),
                                  u"73%"sv));
        auto &color = f.ok(u"\\setOptions[color]{padding=3px,borderWidth=4px}\\colorbox{red}{x}");
        expect(std::ranges::equal(attr(*optionNodes(color, NodeKind::mpadded)[0], u"height"sv),
                                  u"+3px"sv));
        auto &undefined =
            f.ok(u"\\setOptions[noundefined]{color=blue,background=false,size=false}\\unknown");
        auto token = optionNodes(undefined, NodeKind::mtext)[0];
        expect(std::ranges::equal(attr(*token, u"mathcolor"sv), u"blue"sv));
        expect(!token->attributes().isSet(u"mathbackground"));
        f.ok(u"\\setOptions[]{tagIndent=2em,useLabelIds=false,mathStyle=literal,numberPattern={/"
             u"^[0-9]+/},initialDigit={/^[A]/},digits={/a/}}12A");
        expect(std::ranges::equal(*f.jax->options.tagIndent, u"2em"sv));
        expect(!*f.jax->options.useLabelIds);
        auto &numbers = f.ok(u"12A");
        expect(optionNodes(numbers, NodeKind::mn).size() == 1_u);
        expect(optionNodes(numbers, NodeKind::mi).size() == 1_u);
        expect(std::ranges::equal((*f.jax->options.initialDigit)->source(), u"^[A]"sv));
        expect(std::ranges::equal(
            std::get<folia::math::regexp_host::RegExpRef>(*f.jax->options.digits)->source(),
            u"a"sv));
    };
    "setoptions permissions errors partial mutation and presence"_test = [] {
        OptionFixture f;
        struct Error {
            std::u16string_view source;
            std::string_view id;
        };
        for (auto const &row :
             std::array{Error{u"\\setOptions[absent]", "MissingArgFor"},
                        Error{u"\\setOptions[absent]{x}", "NotAPackage"},
                        Error{u"\\setOptions[tagformat]{x}", "PackageNotSettable"},
                        Error{u"\\setOptions{tags=all}", "TeXOptionNotSettable"},
                        Error{u"\\setOptions{unknown}", "InvalidTexOption"},
                        Error{u"\\setOptions[units]{loose}", "InvalidOptionKey"}})
            f.fails(std::u16string(row.source), row.id);
        f.fails(u"\\setOptions{tagIndent=5em,unknown=x}", "InvalidTexOption");
        expect(std::ranges::equal(*f.jax->options.tagIndent, u"5em"sv));
        f.ok(u"\\setOptions{tagIndent=1em,tagIndent=4em}");
        expect(std::ranges::equal(*f.jax->options.tagIndent, u"4em"sv));
        auto &config = *f.jax->options.setoptions;
        config.allowOptionsDefault = false;
        f.fails(u"\\setOptions{unknown}", "TeXOptionNotSettable");
        std::get<std::unordered_map<std::u16string, bool>>(
            config.allowOptions->at(u"tex"))[u"tagIndent"] = true;
        f.ok(u"\\setOptions{tagIndent=7em}");
        config.allowPackageDefault = false;
        f.fails(u"\\setOptions[units]{loose}", "PackageNotSettable");
        (*config.allowOptions)[u"units"] = true;
        f.fails(u"\\setOptions[units]{loose}", "OptionNotSettable");
        config.allowOptionsDefault = true;
        f.jax->options.units = UnitsOptions{.loose = false};
        f.ok(u"\\setOptions[units]{loose}");
        expect(*f.jax->options.units->loose);
        f.jax->options.units->loose.reset();
        f.fails(u"\\setOptions[units]{loose}", "InvalidOptionKey");
    };
    "setoptions custom filters are lazy ordered and preserve skipped values"_test = [] {
        OptionFixture f;
        FilterCounts counts;
        filterCounts = &counts;
        struct Clear {
            ~Clear() {
                filterCounts = nullptr;
            }
        } clear;
        auto &config = *f.jax->options.setoptions;
        config.filterPackage = &acceptPackage;
        config.filterOption.reset();
        config.filterValue.reset();
        f.ok(u"\\setOptions[skip]{tagIndent=bad}");
        expect(counts.packages == 1);
        f.ok(u"\\setOptions{}");
        expect(counts.packages == 2);
        config.filterOption = &acceptOption;
        f.ok(u"\\setOptions{skip=no}");
        expect(counts.options == 1);
        expect(counts.values == 0);
        config.filterValue = &uppercaseValue;
        f.ok(u"\\setOptions{tagIndent=abc,skip=no,useLabelIds=false}");
        expect(counts.packages == 4);
        expect(counts.options == 4);
        expect(counts.values == 2);
        expect(std::ranges::equal(*f.jax->options.tagIndent, u"ABC"sv));
        expect(!*f.jax->options.useLabelIds);
        config.filterOption = &recordSkippedOption;
        config.filterValue.reset();
        f.ok(u"\\setOptions{20=x,tagIndent=abc,2=y,01=z,0=w}");
        const std::array<std::u16string_view, 5> order{u"0", u"2", u"20", u"tagIndent", u"01"};
        expect(counts.keys.size() == order.size());
        for (std::size_t i = 0; i < order.size() && i < counts.keys.size(); ++i)
            expect(std::ranges::equal(counts.keys[i], order[i]));
    };
    "setoptions scalar typed boundaries regex errors and numeric thresholds"_test = [] {
        OptionFixture f({}, {u"units"});
        f.ok(u"\\setOptions[units]{loose=false}");
        expect(!*f.jax->options.units->loose);
        f.ok(u"\\setOptions[units]{loose=nonempty,ugly={/a/}}");
        expect(*f.jax->options.units->loose);
        expect(*f.jax->options.units->ugly);
        for (auto source : {u"\\setOptions{tagIndent}", u"\\setOptions{numberPattern=plain}",
                            u"\\setOptions{digits=false}"})
            f.fails(source, "InvalidValue");
        auto &config = *f.jax->options.setoptions;
        config.filterValue = &nullRegexp;
        f.fails(u"\\setOptions{numberPattern=x}", "InvalidValue");
        f.fails(u"\\setOptions{digits=x}", "InvalidValue");
        f.fails(u"\\setOptions[units]{loose=x}", "InvalidValue");
        config.filterValue = &SetOptionsUtil::filterValue;
        struct Limit {
            std::u16string_view input;
            double value;
        };
        for (auto row : std::array{Limit{u"-Infinity", -std::numeric_limits<double>::infinity()},
                                   Limit{u"-2", -2}, Limit{u"0", 0}, Limit{u"3.9", 3.9},
                                   Limit{u"Infinity", std::numeric_limits<double>::infinity()},
                                   Limit{u"NaN", std::numeric_limits<double>::quiet_NaN()},
                                   Limit{u"184467440737095516160", 184467440737095516160.0},
                                   Limit{u"false", 0}, Limit{u"true", 1}}) {
            f.ok(u"\\setOptions{maxTemplateSubtitutions=" + std::u16string(row.input) + u"}");
            if (std::isnan(row.value))
                expect(std::isnan(*f.jax->options.maxTemplateSubtitutions));
            else
                expect(*f.jax->options.maxTemplateSubtitutions == row.value);
        }
        try {
            f.compile(u"\\setOptions{numberPattern={/[/}}");
            expect(false);
        } catch (folia::math::regexp_host::RegexError const &) {
        }
        expect(f.jax->parseOptions().request().parsers.empty());
        f.ok(u"x");
        TexParser parser(u"", {}, f.jax->parseOptions());
        parser.mml();
        for (std::u16string_view input : {u"/a/bad", u"/a\nb/"})
            expect(std::ranges::equal(std::get<std::u16string>(SetOptionsUtil::filterValue(
                                          parser, u"tex", u"x", std::u16string(input))),
                                      input));
        try {
            SetOptionsUtil::filterValue(parser, u"tex", u"x", std::u16string(u"/a/ii"));
            expect(false);
        } catch (folia::math::regexp_host::RegexError const &) {
        }
    };
    "setoptions clears the nullable reference formatter without changing its native type"_test =
        [] {
            TexOptionValues options;
            options.tagformat.emplace();
            options.tagformat->ref = &optionReference;
            options.setoptions.emplace();
            options.setoptions->allowOptions =
                std::unordered_map<std::u16string, SetOptionsOptions::Permission>{
                    {u"tagformat", true}};
            OptionFixture f(options, {u"tagformat"});
            for (auto source :
                 {u"\\setOptions[tagformat]{ref=false}", u"\\setOptions[tagformat]{ref={}}"}) {
                f.jax->options.tagformat->ref = &optionReference;
                expect(std::get<std::u16string>(f.jax->parseOptions().tags->formatRef(u"X")) ==
                       u"custom:X");
                f.ok(source);
                expect(std::ranges::equal(std::get<std::vector<std::u16string>>(
                                              f.jax->parseOptions().tags->formatRef(u"X")),
                                          std::array{u"(", u"X", u")"}));
            }
        };
    "setoptions budgets and captured math style follow actual parser consumers"_test = [] {
        struct Limit {
            std::u16string_view input;
            bool two, three;
        };
        for (auto const &limit :
             std::array{Limit{u"2.9", true, false}, Limit{u"0", false, false},
                        Limit{u"-Infinity", false, false}, Limit{u"NaN", false, false},
                        Limit{u"Infinity", true, true}}) {
            OptionFixture f({}, {u"newcommand"});
            f.ok(u"\\def\\saved{x}");
            auto first =
                u"\\setOptions{maxMacros=" + std::u16string(limit.input) + u"}\\saved\\saved";
            if (limit.two)
                expect(optionNodes(f.ok(std::move(first)), NodeKind::mi).size() == 2_u);
            else
                f.fails(std::move(first), "MaxMacroSub1");
            if (limit.three)
                expect(optionNodes(f.ok(u"\\saved\\saved\\saved"), NodeKind::mi).size() == 3_u);
            else
                f.fails(u"\\saved\\saved\\saved", "MaxMacroSub1");
            expect(optionNodes(f.ok(u"\\setOptions{maxMacros=Infinity}\\saved"), NodeKind::mi)
                       .size() == 1_u);
        }
        for (auto limit : {u"-1", u"0", u"NaN", u"Infinity"}) {
            TexOptionValues options;
            options.setoptions.emplace();
            options.setoptions->allowOptions =
                std::unordered_map<std::u16string, SetOptionsOptions::Permission>{
                    {u"tex", std::unordered_map<std::u16string, bool>{{u"maxBuffer", true}}}};
            OptionFixture buffer(options, {u"newcommand"});
            buffer.ok(u"\\def\\empty{}");
            auto source = u"\\setOptions{maxBuffer=" + std::u16string(limit) + u"}\\empty";
            if (std::u16string_view(limit) == u"-1") {
                buffer.compile(std::move(source));
                expect(buffer.error == "MaxBufferSize");
            } else
                buffer.ok(std::move(source));
        }
        OptionFixture f;
        for (auto source : {u"\\Gamma", u"\\setOptions{mathStyle=ISO}\\Gamma"}) {
            auto tokens = optionNodes(f.ok(source), NodeKind::mi);
            expect(tokens.size() == 1_u);
            if (tokens.size() == 1)
                expect(attr(*tokens[0], u"mathvariant") == u"normal");
        }
    };
    "setoptions require wrapper fixed let priority and configured tokens"_test = [] {
        OptionFixture f({}, {u"units", u"newcommand"}, false, true);
        std::vector<std::u16string> calls;
        requireCalls = &calls;
        struct Clear {
            ~Clear() {
                requireCalls = nullptr;
                optionTokens = nullptr;
            }
        } clear;
        f.ok(u"\\require[loose]{units}");
        expect(calls.size() == 1_u);
        expect(std::ranges::equal(calls[0], u"units"sv));
        expect(*f.jax->options.units->loose);
        f.ok(u"\\require{units}");
        expect(calls.size() == 2_u);
        auto found = f.jax->parseOptions().handlers.get(HandlerKind::macro)->lookup(u"Require");
        expect(std::get<Macro const *>(found)->arguments.empty());
        expect(std::get<MacroHandler>(std::get<Macro const *>(found)->function) == &localRequire);
        f.ok(u"\\let\\savedOptions\\setOptions");
        f.ok(u"\\savedOptions{tagIndent=9em}");
        expect(std::ranges::equal(*f.jax->options.tagIndent, u"9em"sv));
        std::vector<MmlNode *> tokens;
        optionTokens = &tokens;
        f.jax->parseOptions().request().nodeFactory.setCreators(
            NodeCreators{.token = &observeOptionToken});
        auto &root = f.ok(u"a\\setOptions{}b");
        auto actual = optionNodes(root, NodeKind::mi);
        expect(tokens.size() == 2_u);
        expect(actual.size() == 2_u);
        if (actual.size() == 2 && tokens.size() == 2) {
            expect(actual[0] == tokens[0]);
            expect(actual[1] == tokens[1]);
        }
        OptionFixture reversed({}, {}, true, true);
        reversed.ok(u"\\setOptions{tagIndent=3em}");
        expect(std::ranges::equal(*reversed.jax->options.tagIndent, u"3em"sv));
    };
};
} // namespace
