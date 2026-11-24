#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.color.configuration;

namespace {
thread_local std::string *colorError{};
MmlNode &formatColorError(TeX &jax, TexError const &error) {
    if (colorError)
        *colorError = error.id();
    return jax.formatError(error);
}
struct ColorFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit ColorFixture(TexOptionValues options = {}) {
        ColorConfiguration::initializeModule(registry, maps);
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"color", {}}};
        options.formatError = formatColorError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old;
            ~Restore() {
                colorError = old;
            }
        } restore{colorError};
        colorError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        return jax->compile(math, document);
    }
};
std::vector<MmlNode *> coloredTokens(MmlNode &root) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.isToken())
            result.push_back(&node);
    });
    return result;
}
std::vector<MmlNode *> padded(MmlNode &root) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == NodeKind::mpadded)
            result.push_back(&node);
    });
    return result;
}
} // namespace

suite color_package = [] {
    "color package registers original five methods and owns a configured model"_test = [] {
        ColorFixture f;
        expect(f.jax->options.color->padding.value() == u"5px");
        expect(f.jax->options.color->borderWidth.value() == u"2px");
        expect(f.jax->session().colorModel() != nullptr);
        for (auto command : {u"color", u"textcolor", u"definecolor", u"colorbox", u"fcolorbox"})
            expect(f.jax->configuration().handlers.get(HandlerKind::macro)->contains(command));
    };
    "color package actual TeX scopes textcolor and restores the outer declaration"_test = [] {
        ColorFixture f;
        auto tokens =
            coloredTokens(f.compile(u"\\color{red}a\\textcolor{blue}{b+\\color{green}c}d"));
        expect(tokens.size() == 5_u);
        const std::array<std::u16string_view, 5> expected{u"red", u"blue", u"blue", u"green",
                                                          u"red"};
        for (std::size_t i = 0; i < std::min(tokens.size(), expected.size()); ++i)
            expect(attr(*tokens[i], u"mathcolor") == expected[i]);
        expect(!f.jax->parseOptions().request().error);
        expect(f.jax->parseOptions().request().parsers.empty());
    };
    "color package definitions persist across formulas and tag reset with isolated models"_test =
        [] {
            ColorFixture f;
            f.compile(u"\\definecolor{Stored}{rgb}{0,0.5,1}");
            auto model = f.jax->session().colorModel();
            auto first = coloredTokens(f.compile(u"\\color{Stored}x"));
            expect(attr(*first[0], u"mathcolor") == u"#007fff");
            f.jax->reset(9);
            auto again = coloredTokens(f.compile(u"\\textcolor{Stored}{y}"));
            expect(attr(*again[0], u"mathcolor") == u"#007fff");
            expect(f.jax->session().colorModel() == model);
            ColorFixture other;
            expect(other.jax->session().colorModel()->getColor(u"named", u"Stored") == u"Stored");
            expect(model->getColor(u"named", u"Stored") == u"#007fff");
        };
    "color package boxes retain text and inline math with original padding attributes"_test = [] {
        ColorFixture f;
        auto boxes = padded(f.compile(u"\\colorbox{red}{hi}\\colorbox{blue}{$x^2$}"));
        expect(boxes.size() == 2_u);
        expect(attr(*boxes[0], u"mathbackground") == u"red");
        expect(attr(*boxes[1], u"mathbackground") == u"blue");
        for (auto box : boxes) {
            expect(attr(*box, u"width") == u"+10px");
            expect(attr(*box, u"height") == u"+5px");
            expect(attr(*box, u"depth") == u"+5px");
            expect(attr(*box, u"lspace") == u"5px");
        }
        auto text = coloredTokens(*boxes[0]);
        expect(text.size() == 1_u);
        expect(text[0]->kind() == NodeKind::mtext);
        expect(NodeUtil::getText(*text[0]) == u"hi");
        bool script{};
        boxes[1]->walkTree([&](auto &node) { script |= node.kind() == NodeKind::msup; });
        expect(script);
    };
    "color package fcolorbox inherits its model and applies supplied padding and border"_test = [] {
        TexOptionValues options;
        options.color = ColorOptions{u"0.25em", u"0.1em"};
        ColorFixture f(options);
        auto boxes = padded(f.compile(u"\\fcolorbox[rgb]{1,0,0}{0,0,1}{hi}"));
        expect(boxes.size() == 1_u);
        expect(attr(*boxes[0], u"mathbackground") == u"#0000ff");
        expect(attr(*boxes[0], u"style") == u"border: 0.1em solid #ff0000");
        expect(attr(*boxes[0], u"width") == u"+0.5em");
        expect(attr(*boxes[0], u"lspace") == u"0.25em");
    };
    "color package helper preserves original padding numeric prefixes and units"_test = [] {
        const std::array<std::pair<std::u16string_view, std::u16string_view>, 10> cases{
            {{u"5px", u"+10px"},
             {u".25em", u"+0.5em"},
             {u"2e2pt", u"+400pt"},
             {u"0x10em", u"+0em"},
             {u"-2px", u"+NaNpx"},
             {u"2EM", u"+4"},
             {u"1e-400px", u"+0px"},
             {u"1e400px", u"+Infinitypx"},
             {u"5px\n", u"+105px\n"},
             {u"", u"+NaN"}}};
        for (auto const &[input, expected] : cases) {
            auto attributes = padding(input);
            expect(attr(attributes.attributes, u"width") == expected);
            expect(attr(attributes.attributes, u"lspace") == input);
            expect(std::ranges::equal(attr(attributes.attributes, u"height"),
                                      u"+" + std::u16string(input)));
        }
    };
    "color package reports original model errors and recovers on following formulas"_test = [] {
        const std::array<std::pair<std::u16string_view, std::string_view>, 4> cases{
            {{u"\\color[cmyk]{0,0,0,0}x", "UndefinedColorModel"},
             {u"\\textcolor[rgb]{2,0,0}{x}", "ModelArg2"},
             {u"\\definecolor{bad}{RGB}{1,2}", "ModelArg1"},
             {u"\\color{red;blue}x", "BadColorValue"}}};
        for (auto const &[input, expected] : cases) {
            ColorFixture f;
            f.compile(std::u16string(input));
            expect(f.error == expected);
            expect(f.jax->parseOptions().request().parsers.empty());
            auto nodes = coloredTokens(f.compile(u"\\color{green}x"));
            expect(f.error.empty());
            expect(attr(*nodes[0], u"mathcolor") == u"green");
        }
    };
};
