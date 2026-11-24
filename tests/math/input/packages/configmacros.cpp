#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.configmacros.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.parse_methods;

namespace {
using MacroDefinitions = std::unordered_map<std::u16string, MacroDefinition>;
using EnvironmentDefinitions = std::unordered_map<std::u16string, EnvironmentDefinition>;
constexpr std::array<MacroHandler, 3> configMacroCallbacks{
    &BaseMethods::Macro, &NewcommandMethods::MacroWithTemplate, &NewcommandMethods::MacroDef};
constexpr std::array<TokenInputHandler, 1> configTokenCallbacks{&ParseMethods::lcGreek};
thread_local std::string *configMacroError{};
MmlNode &configMacroFormatError(TeX &jax, TexError const &error) {
    if (configMacroError)
        *configMacroError = error.id();
    return jax.formatError(error);
}
MacroDefinition replacement(std::u16string source, double count = 0,
                            std::optional<std::u16string> first = {}) {
    MacroDefinition definition;
    definition.replacement_source = std::move(source);
    definition.arity = count;
    definition.default_first_argument = std::move(first);
    return definition;
}
MacroDefinition pairTemplate() {
    auto definition = replacement(u"#2+#1", 2);
    definition.parameter_template = true;
    definition.parameter_prefix = u"(";
    definition.parameter_delimiters = {u",", u")"};
    return definition;
}
struct ConfigMacrosFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{configMacroCallbacks, configTokenCallbacks};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit ConfigMacrosFixture(TexOptionValues options = {}, bool newcommand = false) {
        ConfigMacrosConfiguration::initializeModule(registry, maps);
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"configmacros", {}}};
        if (newcommand) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        options.formatError = &configMacroFormatError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, std::move(options));
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *previous;
            ~Restore() { configMacroError = previous; }
        } restore{configMacroError};
        configMacroError = &error;
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
    Macro const *macro(std::u16string_view map, std::u16string_view name) {
        auto value = jax->parseOptions().handlers.retrieve(map)->lookup(name);
        auto entry = std::get_if<Macro const *>(&value);
        return entry ? *entry : nullptr;
    }
};
}

suite configmacros = [] {
    "configmacros init owns three configured maps and BeginEnv class without newcommand package"_test = [] {
        ConfigMacrosFixture f;
        auto package = f.registry.get(u"configmacros");
        expect(package->init() != nullptr && package->config() != nullptr);
        expect(package->items.size() == 1_u);
        expect(package->options.active.has_value() && package->options.active->empty());
        expect(package->options.macros.has_value() && package->options.macros->empty());
        expect(package->options.environments.has_value() && package->options.environments->empty());
        expect(f.registry.get(u"newcommand") == nullptr);
        expect(f.maps.getMap(u"new-Command") == nullptr);
        auto &handlers = f.jax->parseOptions().handlers;
        expect(dynamic_cast<MacroMap *>(handlers.retrieve(u"configmacros-active-map").get()) != nullptr);
        expect(dynamic_cast<CommandMap *>(handlers.retrieve(u"configmacros-map").get()) != nullptr);
        expect(dynamic_cast<EnvironmentMap *>(handlers.retrieve(u"configmacros-env-map").get()) != nullptr);
        auto item = f.jax->parseOptions().itemFactory->create(StackKind::begin_env);
        expect(dynamic_cast<BeginEnvItem *>(item.get()) != nullptr);
        expect(item->isOpen());
        expect(f.jax->session().definitions().global.commands.empty());
        expect(f.jax->session().definitions().global.environments.empty());
    };
    "configmacros ordinary optional zero and empty defaults use original Macro callback"_test = [] {
        TexOptionValues options;
        options.macros = MacroDefinitions{{u"plain", replacement(u"x")},
                          {u"swap", replacement(u"#2+#1", 2)},
                          {u"pair", replacement(u"#1+#2", 2, u"d")},
                          {u"empty", replacement(u"#1+x", 1, u"")},
                          {u"numericZero", replacement(u"x", 0, u"d")},
                          {u"stringZero", replacement(u"#1", 0, u"d")}};
        options.macros->at(u"stringZero").substitute_parameters = true;
        ConfigMacrosFixture f(options);
        expect(f.text(u"\\plain+\\swap ab+\\pair{x}+\\pair[q]{r}+\\empty") == u"x+b+a+d+x+q+r++x");
        expect(f.text(u"\\numericZero[q]") == u"x[q]");
        expect(f.text(u"\\stringZero[q]") == u"q");
        auto macro = f.macro(u"configmacros-map", u"plain");
        expect(macro != nullptr);
        if (macro)
            expect(std::get<MacroHandler>(macro->function) == &BaseMethods::Macro);
        expect(f.error.empty());
    };
    "configmacros templates and active characters reuse compiled expansion with priority three"_test = [] {
        TexOptionValues options;
        options.macros = MacroDefinitions{{u"pair", pairTemplate()}, {u"sin", replacement(u"s")}};
        options.active = MacroDefinitions{{u"@", pairTemplate()}, {u"a", replacement(u"q")},
                          {u"2", replacement(u"r")}};
        ConfigMacrosFixture f(options);
        expect(f.text(u"\\pair(x,y)+@(x,y)+a+2+\\sin") == u"y+x+y+x+q+r+s");
        auto macro = f.macro(u"configmacros-map", u"pair");
        expect(macro != nullptr);
        if (macro) {
            expect(std::get<MacroHandler>(macro->function) == &NewcommandMethods::MacroWithTemplate);
            expect(macro->arguments.size() == 5_u);
            expect(std::get<std::u16string>(macro->arguments[2]) == u"(");
        }
        expect(f.jax->session().findMacro(u"pair") == nullptr);
    };
    "configmacros configured environments parse nested optional and array bodies without newcommand"_test = [] {
        TexOptionValues options;
        options.environments = EnvironmentDefinitions{
            {u"e", EnvironmentDefinition{u"#1+#2+", u"+z", 2, u"d"}},
            {u"n", EnvironmentDefinition{u"a", u"b"}},
            {u"m", EnvironmentDefinition{u"\\begin{array}{cc}", u"\\end{array}"}},
            {u"zero", EnvironmentDefinition{u"#1", u"x", 0, u"d", true}}};
        ConfigMacrosFixture f(options);
        expect(f.text(u"\\begin{e}{x}y\\end{e}+\\begin{e}[q]{r}s\\end{e}") == u"d+x+y+z+q+r+s+z");
        expect(f.text(u"\\begin{n}x\\begin{n}y\\end{n}z\\end{n}") == u"axaybzb");
        expect(f.text(u"\\begin{m}a&b\\end{m}") == u"ab");
        expect(f.text(u"\\begin{zero}[q]\\end{zero}") == u"qx");
        expect(f.error.empty());
        expect(f.jax->session().findEnvironment(u"e") == nullptr);
    };
    "configmacros maps own definition snapshots across external option changes formulas and reset"_test = [] {
        TexOptionValues options;
        options.macros = MacroDefinitions{{u"kept", replacement(u"x")}};
        ConfigMacrosFixture f(options);
        options.macros->at(u"kept").replacement_source = u"y";
        f.jax->options.macros->at(u"kept").replacement_source = u"z";
        expect(f.text(u"\\kept") == u"x");
        f.jax->reset(11);
        expect(f.text(u"\\kept+\\kept") == u"x+x");
        expect(f.jax->session().findCommand(u"kept") == nullptr);
        ConfigMacrosFixture other;
        other.compile(u"\\kept");
        expect(other.error == "UndefinedControlSequence");
    };
    "configmacros newcommand selection permits let snapshots redefinition and environment overrides"_test = [] {
        TexOptionValues options;
        options.macros = MacroDefinitions{{u"kept", replacement(u"x")}, {u"pair", pairTemplate()}};
        options.environments = EnvironmentDefinitions{{u"e", EnvironmentDefinition{u"a", u"b"}}};
        ConfigMacrosFixture f(options, true);
        expect(f.text(u"\\let\\saved\\kept\\renewcommand{\\kept}{y}\\saved\\kept") == u"xy");
        expect(f.text(u"\\let\\savedPair\\pair\\def\\pair{x}\\savedPair(a,b)") == u"b+a");
        expect(f.text(u"\\renewenvironment{e}{c}{d}\\begin{e}x\\end{e}") == u"cxd");
        expect(f.jax->session().findMacro(u"kept") != nullptr);
        expect(f.macro(u"configmacros-map", u"kept") != nullptr);
    };
    "configmacros errors preserve subsequent formulas and original recursion limits"_test = [] {
        TexOptionValues options;
        options.macros = MacroDefinitions{{u"pair", pairTemplate()}, {u"loop", replacement(u"\\loop")},
                          {u"bad", replacement(u"#1", 1)}};
        options.environments = EnvironmentDefinitions{{u"e", EnvironmentDefinition{u"a", u"b"}},
                                {u"recursive", EnvironmentDefinition{u"\\begin{recursive}", u""}}};
        options.maxMacros = 8;
        ConfigMacrosFixture f(options);
        f.compile(u"\\pair[a,b)");
        expect(f.error == "MismatchUseDef");
        expect(f.text(u"\\pair(a,b)") == u"b+a");
        f.compile(u"\\bad");
        expect(f.error == "MissingArgFor");
        f.compile(u"\\begin{e}");
        expect(f.error == "EnvMissingEnd");
        f.compile(u"\\begin{e}\\end{array}");
        expect(f.error == "EnvBadEnd");
        f.compile(u"\\loop");
        expect(f.error == "MaxMacroSub1");
        f.compile(u"\\begin{recursive}");
        expect(f.error == "MaxMacroSub2");
        expect(f.text(u"\\begin{e}x\\end{e}") == u"axb");
        expect(f.jax->parseOptions().request().parsers.empty());
    };
};
