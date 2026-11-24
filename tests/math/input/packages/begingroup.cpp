#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.begingroup.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.parse_methods;

namespace {
constexpr std::array<MacroHandler, 10> groupCallbacks{
    &BaseMethods::Macro, &BaseMethods::Frac, &NewcommandMethods::MacroDef,
    &NewcommandMethods::Let, &NewcommandMethods::NewCommand,
    &BegingroupMethods::begingroup, &BegingroupMethods::endgroup,
    &BegingroupMethods::global, &BegingroupMethods::reset, &BegingroupMethods::sandbox};
constexpr std::array<TokenInputHandler, 6> groupTokenCallbacks{
    &ParseMethods::lcGreek, &ParseMethods::ucGreek, &ParseMethods::mathchar0mi,
    &ParseMethods::mathchar0mo, &ParseMethods::mathchar7, &ParseMethods::delimiter};
thread_local std::string *groupError{};
thread_local std::u16string *groupMessage{};
thread_local bool *sandboxObservation{};
MmlNode &groupFormatError(TeX &jax, TexError const &error) {
    if (groupError)
        *groupError = error.id();
    if (groupMessage)
        *groupMessage = error.message();
    return jax.formatError(error);
}
DispatchOutcome observeSandbox(TexParser &parser, MacroInvocation const &) {
    if (sandboxObservation)
        *sandboxObservation = parser.stack.global().is_sandbox.value_or(false);
    return NoParseResult{};
}
DispatchOutcome interruptedFormula(TexParser &, MacroInvocation const &) {
    throw std::runtime_error("Interrupted formula before finish");
}
struct BegingroupFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{groupCallbacks, groupTokenCallbacks};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    std::u16string message;
    bool observedSandbox{};
    explicit BegingroupFixture(std::optional<std::vector<std::u16string>> allow = {}) {
        BegingroupConfiguration::initializeModule(registry, maps);
        NewcommandConfiguration::initializeModule(registry, maps);
        // These probes observe the real parser and interrupt it; group operations
        // themselves always come from the actual begingroup package.
        auto map = std::dynamic_pointer_cast<MacroMap>(maps.getMap(u"begingroup"));
        map->add(Macro(u"observeSandbox", &observeSandbox));
        map->add(Macro(u"interruptedFormula", &interruptedFormula));
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{
            {u"base", {}}, {u"begingroup", {}}, {u"newcommand", {}}};
        options.formatError = &groupFormatError;
        if (allow)
            options.begingroup = BegingroupOptions{std::move(allow)};
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, std::move(options));
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    BegingroupStack &stack() { return *jax->parseOptions().begingroup; }
    MmlNode &compile(std::u16string source) {
        error.clear();
        message.clear();
        struct ObservationScope {
            std::string *previousError{groupError};
            std::u16string *previousMessage{groupMessage};
            bool *previousSandbox{sandboxObservation};
            ~ObservationScope() {
                groupError = previousError;
                groupMessage = previousMessage;
                sandboxObservation = previousSandbox;
            }
        } scope;
        groupError = &error;
        groupMessage = &message;
        sandboxObservation = &observedSandbox;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        return jax->compile(math, document);
    }
    std::u16string text(std::u16string source) {
        auto &root = compile(std::move(source));
        std::u16string result;
        root.walkTree([&](auto &node) {
            if (node.isToken())
                result += NodeUtil::getText(node);
        });
        return result;
    }
};
}

suite begingroup = [] {
    "begingroup package registers original six commands defaults filters and owned service"_test = [] {
        BegingroupFixture f;
        auto package = f.registry.get(u"begingroup");
        expect(package->handler.size() == 4_u);
        for (auto const &handler : package->handler)
            expect(std::ranges::equal(handler.maps, handler.kind == HandlerKind::macro
                ? std::vector<std::u16string>{u"begingroup"}
                : std::vector<std::u16string>{}));
        expect(package->preprocessors.size() == 1_u);
        expect(package->postprocessors.size() == 1_u);
        expect(package->config() != nullptr);
        expect(package->options.begingroup->allowGlobal.value() ==
               std::vector<std::u16string>{u"let", u"def", u"newcommand",
                                            u"DeclareMathOperator", u"Newextarrow"});
        expect(f.jax->parseOptions().begingroup != nullptr);
        expect(f.stack().index() == -100_i);
        expect(f.stack().top() == -100_i);
        expect(f.stack().base() == -100_i);
        expect(BegingroupMethods::macro == &BaseMethods::Macro);
        auto map = f.maps.getMap(u"begingroup");
        for (auto name : {u"begingroup", u"endgroup", u"global", u"gdef",
                          u"begingroupReset", u"begingroupSandbox"})
            expect(map->contains(name));
        auto lookup = map->lookup(u"gdef");
        auto macro = std::get<Macro const *>(lookup);
        expect(std::get<MacroHandler>(macro->function) == &BaseMethods::Macro);
        expect(std::get<std::u16string>(macro->arguments.front()) == u"\\global\\def");
    };
    "begingroup package nested cross formula groups restore all three definition kinds"_test = [] {
        BegingroupFixture f;
        f.compile(u"\\def\\foo{g}\\let\\delim(\\newenvironment{e}{a}{b}");
        expect(f.text(u"\\begingroup\\def\\foo{l}\\let\\delim["
                      u"\\renewenvironment{e}{c}{d}\\foo\\delim\\begin{e}x\\end{e}") == u"l[cxd");
        expect(f.stack().index() == -101_i && f.stack().top() == -101_i);
        expect(f.text(u"\\begingroup\\def\\foo{q}\\foo") == u"q");
        expect(f.stack().index() == -102_i && f.stack().top() == -102_i);
        expect(f.text(u"\\endgroup\\foo") == u"l");
        expect(f.text(u"\\endgroup\\foo\\delim\\begin{e}x\\end{e}") == u"g(axb");
        expect(f.stack().index() == -100_i && f.stack().top() == -100_i);
        expect(f.error.empty());
    };
    "begingroup package global def newcommand let and gdef clear nested shadows"_test = [] {
        BegingroupFixture f;
        expect(f.text(u"\\def\\a{x}\\begingroup\\def\\a{y}\\begingroup\\gdef\\a{z}"
                      u"\\a\\endgroup\\a\\endgroup\\a") == u"zzz");
        expect(f.text(u"\\begingroup\\global \\newcommand{\\b}{q}"
                      u"\\global\\let\\d(\\endgroup\\b\\d") == u"q(");
        expect(f.text(u"\\begingroup\\def\\a{l}\\global\\def\\a{r}\\endgroup\\a") == u"r");
        expect(f.error.empty());
    };
    "begingroup package global allow list controls actual command spelling and environments"_test = [] {
        BegingroupFixture f;
        f.compile(u"\\global\\frac12");
        expect(f.error == "IllegalGlobal");
        expect(f.message == u"Invalid use of \\global");
        f.compile(u"\\global x");
        expect(f.error == "IllegalGlobal");
        f.compile(u"\\let\\glob\\global\\glob\\frac12");
        expect(f.error == "IllegalGlobal");
        expect(f.message == u"Invalid use of \\glob");
        f.compile(u"\\global\\newenvironment{e}{a}{b}");
        expect(f.error == "IllegalGlobal");
        BegingroupFixture none(std::vector<std::u16string>{});
        none.compile(u"\\global\\def\\a{x}");
        expect(none.error == "IllegalGlobal");
        BegingroupFixture environments(std::vector<std::u16string>{u"newenvironment"});
        expect(environments.text(u"\\begingroup\\global\\newenvironment{e}{a}{b}"
                                 u"\\endgroup\\begin{e}x\\end{e}") == u"axb");
        expect(environments.error.empty());
    };
    "begingroup package reset removes open local definitions and preserves base values"_test = [] {
        BegingroupFixture f;
        f.compile(u"\\def\\a{g}\\let\\d(\\newenvironment{e}{a}{b}");
        f.compile(u"\\begingroup\\def\\a{l}\\let\\d[\\renewenvironment{e}{c}{d}"
                  u"\\begingroup\\def\\a{q}");
        expect(f.text(u"\\begingroupReset\\a\\d\\begin{e}x\\end{e}") == u"g(axb");
        expect(f.stack().index() == -100_i && f.stack().top() == -100_i);
        f.compile(u"\\endgroup");
        expect(f.error == "MissingBegingroup");
        expect(f.text(u"\\a") == u"g");
    };
    "begingroup package sandbox isolates global writes and marks only current parser"_test = [] {
        BegingroupFixture f;
        f.compile(u"\\def\\a{g}\\let\\d(\\newenvironment{e}{a}{b}");
        expect(f.text(u"\\begingroupSandbox\\observeSandbox\\global\\def\\a{s}"
                      u"\\global\\let\\d[\\renewenvironment{e}{c}{d}\\a\\d"
                      u"\\begin{e}x\\end{e}") == u"s[cxd");
        expect(f.observedSandbox);
        expect(f.stack().base() == -101_i && f.stack().index() == -101_i);
        expect(f.text(u"\\observeSandbox\\a") == u"s");
        expect(!f.observedSandbox);
        expect(f.text(u"\\begingroupReset\\a") == u"s");
        expect(f.text(u"\\begingroupSandbox\\observeSandbox\\a\\d\\begin{e}x\\end{e}") == u"g(axb");
        expect(f.observedSandbox);
        f.compile(u"\\endgroup");
        expect(f.error == "MissingBegingroup");
    };
    "begingroup package TexError finishes the group and next formula can close it"_test = [] {
        BegingroupFixture f;
        f.compile(u"\\def\\a{g}");
        f.compile(u"\\begingroup\\def\\a{l}\\unknownGroupCommand");
        expect(f.error == "UndefinedControlSequence");
        expect(f.stack().index() == -101_i && f.stack().top() == -101_i);
        expect(f.jax->parseOptions().request().parsers.empty());
        expect(f.text(u"\\a\\endgroup\\a") == u"lg");
    };
    "begingroup package remove discards interrupted groups above previous finished top"_test = [] {
        BegingroupFixture f;
        f.compile(u"\\def\\a{g}\\begingroup\\def\\a{o}");
        expect(throws<std::runtime_error>([&] {
            f.compile(u"\\begingroup\\def\\a{i}\\interruptedFormula");
        }));
        expect(f.stack().index() == -102_i && f.stack().top() == -101_i);
        expect(f.jax->parseOptions().request().parsers.empty());
        expect(f.text(u"\\a") == u"o");
        expect(f.stack().index() == -101_i && f.stack().top() == -101_i);
        expect(f.text(u"\\endgroup\\a") == u"g");
    };
    "begingroup package let aliases use fixed compiled callbacks and protect sandbox command"_test = [] {
        BegingroupFixture f;
        expect(f.text(u"\\def\\a{g}\\let\\start\\begingroup\\let\\finish\\endgroup"
                      u"\\start\\def\\a{l}\\a\\finish\\a") == u"lg");
        f.compile(u"\\newcommand{\\begingroupSandbox}{x}");
        expect(f.error == "ProtectedMacro");
        f.jax->reset(7);
        expect(f.text(u"\\a") == u"g");
    };
};
