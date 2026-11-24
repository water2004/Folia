#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.bbm.configuration;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 3> bbmCallbacks{&BbmMethods::ChooseFont, &BbmMethods::ChangeBold,
                                                   BbmMethods::MathFont};
thread_local std::string *bbmError{};
MmlNode &formatBbmError(TeX &jax, TexError const &error) {
    if (bbmError)
        *bbmError = error.id();
    return jax.formatError(error);
}
struct BbmFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{bbmCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit BbmFixture(bool bold = false) {
        BbmConfiguration::initializeModule(registry, maps);
        NewcommandConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages =
            std::vector<PackageSelection>{{u"base", {}}, {u"bbm", {}}, {u"newcommand", {}}};
        options.bbm = BbmOptions{.bold = bold};
        options.formatError = &formatBbmError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{bbmError};
            ~Restore() {
                bbmError = old;
            }
        } restore;
        bbmError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        auto &root = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return root;
    }
    void ok(std::u16string source,
            std::initializer_list<std::pair<std::u16string_view, std::u16string_view>> expected) {
        auto &root = compile(std::move(source));
        expect(error.empty()) << error;
        std::vector<MmlNode *> tokens;
        root.walkTree([&](auto &node) {
            if (node.isToken())
                tokens.push_back(&node);
        });
        expect(tokens.size() == expected.size());
        if (tokens.size() != expected.size())
            return;
        std::size_t index = 0;
        for (auto const &[text, font] : expected) {
            expect(std::ranges::equal(NodeUtil::getText(*tokens[index]), text));
            expect(std::ranges::equal(attr(*tokens[index], u"mathvariant"), font));
            ++index;
        }
    }
};
} // namespace

suite bbm = [] {
    "bbm original configuration owns all four mappings and Base MathFont alias"_test = [] {
        BbmFixture f;
        expect(f.registry.get(u"bbm")->options.bbm->bold == false);
        expect(BbmMethods::MathFont == &BaseMethods::MathFont);
        const std::pair<std::u16string_view, MacroHandler> methods[]{
            {u"mathversion", &BbmMethods::ChangeBold},
            {u"mathbbm", &BbmMethods::ChooseFont},
            {u"mathbbmss", &BbmMethods::ChooseFont},
            {u"mathbbmtt", BbmMethods::MathFont}};
        const std::vector<std::vector<std::u16string_view>> args{
            {},
            {u"-bbm-normal", u"-bbm-bold"},
            {u"-bbm-sans-serif", u"-bbm-sans-serif-bold"},
            {u"-bbm-monospace"}};
        std::size_t index = 0;
        for (auto const &[name, handler] : methods) {
            auto macro = std::get<Macro const *>(f.maps.getMap(u"bbm")->lookup(name));
            expect(std::get<MacroHandler>(macro->function) == handler);
            expect(macro->arguments.size() == args[index].size());
            if (macro->arguments.size() == args[index].size())
                for (std::size_t i = 0; i < args[index].size(); ++i)
                    expect(std::ranges::equal(std::get<std::u16string>(macro->arguments[i]),
                                              args[index][i]));
            ++index;
        }
    };
    "bbm configured fonts and nested Base fonts preserve surrounding environment"_test = [] {
        for (bool bold : {false, true}) {
            BbmFixture f(bold);
            const std::pair<std::u16string_view, std::u16string_view> variants[]{
                {u"mathbbm", bold ? u"-bbm-bold" : u"-bbm-normal"},
                {u"mathbbmss", bold ? u"-bbm-sans-serif-bold" : u"-bbm-sans-serif"},
                {u"mathbbmtt", u"-bbm-monospace"}};
            for (auto const &[command, font] : variants) {
                const auto prefix = u"\\" + std::u16string(command);
                f.ok(prefix + u"{x}y", {{u"x", font}, {u"y", u"italic"}});
                f.ok(prefix + u" x", {{u"x", font}});
                f.ok(prefix + u"{\\mathbf{x}y}z",
                     {{u"x", u"bold"}, {u"y", font}, {u"z", u"italic"}});
                f.ok(prefix + u"{}", {});
            }
        }
    };
    "bbm mathversion state belongs to the input jax across groups formulas and errors"_test = [] {
        BbmFixture f;
        f.ok(u"{\\mathversion{bold}}\\mathbbm{x}", {{u"x", u"-bbm-bold"}});
        f.ok(u"\\mathbbmss{x}", {{u"x", u"-bbm-sans-serif-bold"}});
        f.compile(u"\\mathversion{normal}\\unknown");
        expect(f.error == "UndefinedControlSequence");
        f.ok(u"\\mathbbm{x}", {{u"x", u"-bbm-normal"}});
        f.ok(u"\\mathversion{bold}", {});
        f.compile(u"\\mathbbm{\\unknown}");
        expect(f.error == "UndefinedControlSequence");
        f.ok(u"\\mathbbm{x}", {{u"x", u"-bbm-bold"}});
        f.ok(u"\\mathversion{other}\\mathbbm{x}", {{u"x", u"-bbm-normal"}});
        BbmFixture independent;
        independent.ok(u"\\mathbbm{x}", {{u"x", u"-bbm-normal"}});
    };
    "bbm missing arguments release parser borrows and immutable aliases retain callbacks"_test =
        [] {
            BbmFixture f;
            for (auto source : {u"\\mathversion", u"\\mathbbm", u"\\mathbbmss", u"\\mathbbmtt"}) {
                f.compile(source);
                expect(f.error == "MissingArgFor");
                f.ok(u"\\mathbbm{x}", {{u"x", u"-bbm-normal"}});
            }
            f.ok(u"\\let\\savedFont\\mathbbm\\let\\savedVersion\\mathversion\\let\\savedMono\\mathb"
                 u"bmtt",
                 {});
            f.ok(u"\\savedVersion{bold}\\savedFont{x}\\savedMono{y}",
                 {{u"x", u"-bbm-bold"}, {u"y", u"-bbm-monospace"}});
        };
};
