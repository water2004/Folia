#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.dsfont.configuration;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 1> dsfontCallbacks{&DsfontConfiguration::ChooseFont};
thread_local std::string *dsfontError{};
MmlNode &formatDsfontError(TeX &jax, TexError const &error) {
    if (dsfontError)
        *dsfontError = error.id();
    return jax.formatError(error);
}
struct DsfontFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{dsfontCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit DsfontFixture(bool sans = false) {
        DsfontConfiguration::initializeModule(registry, maps);
        NewcommandConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages =
            std::vector<PackageSelection>{{u"base", {}}, {u"dsfont", {}}, {u"newcommand", {}}};
        options.dsfont = DsfontOptions{.sans = sans};
        options.formatError = &formatDsfontError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{dsfontError};
            ~Restore() {
                dsfontError = old;
            }
        } restore;
        dsfontError = &error;
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

suite dsfont = [] {
    "dsfont original single configuration owns the command and default sans option"_test = [] {
        DsfontFixture f;
        expect(f.registry.get(u"dsfont")->options.dsfont->sans == false);
        auto macro = std::get<Macro const *>(f.maps.getMap(u"dsfont")->lookup(u"mathds"));
        expect(std::get<MacroHandler>(macro->function) == &DsfontConfiguration::ChooseFont);
        expect(macro->arguments.empty());
    };
    "dsfont both configured fonts delegate nested argument and environment handling"_test = [] {
        for (bool sans : {false, true}) {
            DsfontFixture f(sans);
            const auto font = sans ? u"-ds-ss" : u"-ds-rm";
            for (auto source : {u"\\mathds{x}y", u"\\mathds x y"})
                f.ok(source, {{u"x", font}, {u"y", u"italic"}});
            f.ok(u"\\mathds{\\mathbf{x}y}z", {{u"x", u"bold"}, {u"y", font}, {u"z", u"italic"}});
            f.ok(u"\\mathds{}", {});
            f.jax->parseOptions().options.dsfont->sans = !sans;
            f.ok(u"\\mathds{x}", {{u"x", sans ? u"-ds-rm" : u"-ds-ss"}});
        }
    };
    "dsfont errors release borrowed parsers and immutable aliases preserve the method"_test = [] {
        for (bool sans : {false, true}) {
            DsfontFixture f(sans);
            const auto font = sans ? u"-ds-ss" : u"-ds-rm";
            f.compile(u"\\mathds");
            expect(f.error == "MissingArgFor");
            f.compile(u"\\mathds{\\unknown}");
            expect(f.error == "UndefinedControlSequence");
            f.ok(u"\\mathds{x}", {{u"x", font}});
            f.ok(u"\\let\\savedDs\\mathds", {});
            f.ok(u"\\savedDs{x}", {{u"x", font}});
        }
    };
};
