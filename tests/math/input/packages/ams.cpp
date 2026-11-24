#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.ams.configuration;
import folia.math.input.tex.begingroup.configuration;

namespace {
constexpr std::array<MacroHandler, 2> amsCallbacks{&BaseMethods::Macro,
                                                   &AmsMethods::HandleDeclareOp};
thread_local std::string *amsError{};
MmlNode &formatAmsError(TeX &jax, TexError const &error) {
    if (amsError)
        *amsError = error.id();
    return jax.formatError(error);
}
struct AmsFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{amsCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit AmsFixture(bool tags = false, bool groups = false) {
        if (groups)
            BegingroupConfiguration::initializeModule(registry, maps);
        AmsConfiguration::initializeModule(registry, maps);
        auto commands = std::dynamic_pointer_cast<MacroMap>(maps.getMap(u"AMSmath-macros"));
        commands->add(Macro(u"widearrow", &AmsMethods::xArrow, {8594.0, 5.0, 10.0, 3.0}));
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (groups)
            options.packages->push_back({u"begingroup", {}});
        options.packages->push_back({u"ams", {}});
        options.tags = tags ? u"ams" : u"none";
        options.maxMacros = 1000;
        options.formatError = &formatAmsError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source, bool display = true) {
        error.clear();
        struct Restore {
            std::string *previous;
            ~Restore() {
                amsError = previous;
            }
        } restore{amsError};
        amsError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(display);
        auto &result = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return result;
    }
    MmlNode &ok(std::u16string source) {
        auto &result = compile(std::move(source));
        expect(error.empty());
        return result;
    }
    std::u16string text(std::u16string source) {
        auto &result = ok(std::move(source));
        std::u16string text;
        result.walkTree([&](auto &node) {
            if (node.isToken())
                text += NodeUtil::getText(node);
        });
        return text;
    }
};
std::vector<MmlNode *> amsNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
bool numericAttribute(MmlNode const &node, std::u16string_view name, double value) {
    auto current = node.attributes().lookup(name);
    return current && std::holds_alternative<double>(*current) &&
           std::get<double>(*current) == value;
}
bool booleanAttribute(MmlNode const &node, std::u16string_view name, bool value) {
    auto current = node.attributes().lookup(name);
    return current && std::holds_alternative<bool>(*current) && std::get<bool>(*current) == value;
}
} // namespace

suite ams_methods = [] {
    "ams methods retain all ten original Base aliases and real mapping callbacks"_test = [] {
        expect(AmsMethods::HandleNoTag == &BaseMethods::HandleNoTag);
        expect(AmsMethods::HandleRef == &BaseMethods::HandleRef);
        expect(AmsMethods::Macro == &BaseMethods::Macro);
        expect(AmsMethods::Accent == &BaseMethods::Accent);
        expect(AmsMethods::Tilde == &BaseMethods::Tilde);
        expect(AmsMethods::Array == &BaseMethods::Array);
        expect(AmsMethods::Spacer == &BaseMethods::Spacer);
        expect(AmsMethods::NamedOp == &BaseMethods::NamedOp);
        expect(AmsMethods::EqnArray == &BaseMethods::EqnArray);
        expect(AmsMethods::Equation == &BaseMethods::Equation);
        AmsFixture f;
        auto map = f.maps.getMap(u"AMSmath-macros");
        for (auto name : {u"operatorname", u"DeclareMathOperator", u"sideset", u"idotsint",
                          u"xrightarrow", u"shoveleft", u"cfrac", u"genfrac", u"tag"})
            expect(map->contains(name));
    };
    "ams methods five array builders retain optional align counts and empty count behavior"_test =
        [] {
            AmsFixture f;
            for (auto source :
                 {u"\\begin{aligned}[t]a&=b\\\\c&=d\\end{aligned}",
                  u"\\begin{gathered}[b]a\\\\b\\end{gathered}",
                  u"\\begin{aligned}[]a&=b\\end{aligned}",
                  u"\\begin{alignat*}{2}a&=b&c&=d\\end{alignat*}",
                  u"\\begin{alignedat}[t]{1}a&=b\\end{alignedat}",
                  u"\\begin{xalignat*}{1}a&=b\\end{xalignat*}",
                  u"\\begin{xxalignat}{1}a&=b\\end{xxalignat}",
                  u"\\begin{flalign*}a&=b\\end{flalign*}", u"\\begin{alignedat}{}a\\end{alignedat}",
                  u"\\begin{xalignat*}{}a\\end{xalignat*}"}) {
                auto &root = f.ok(source);
                expect(amsNodes(root, NodeKind::mtable).size() == 1_u);
            }
            auto &root =
                f.ok(u"\\begin{multline*}\\shoveleft a+b\\\\\\shoveright c+d\\end{multline*}");
            auto tables = amsNodes(root, NodeKind::mtable);
            expect(tables.size() == 1_u);
            if (tables.size() == 1) {
                expect(attr(*tables[0], u"width") == u"100%");
                expect(attr(*tables[0], u"columnspacing") == u"100%");
            }
        };
    "ams operatorname groups operator letters overrides font and preserves star limits rules"_test =
        [] {
            AmsFixture f;
            struct Example {
                std::u16string_view source;
                NodeKind kind;
            };
            for (auto const &example :
                 {Example{u"\\operatorname{lim}_{x}y", NodeKind::msub},
                  Example{u"\\operatorname*{lim}_{x}y", NodeKind::munder},
                  Example{u"\\operatorname{lim}\\limits_{x}y", NodeKind::msub}}) {
                auto &root = f.ok(std::u16string(example.source));
                expect(amsNodes(root, example.kind).size() == 1_u);
            }
            auto &root = f.ok(u"\\mathbf{\\operatorname{a-b*}}+a-b*");
            bool found{};
            for (auto token : amsNodes(root, NodeKind::mi))
                if (NodeUtil::getText(*token) == u"a-b*") {
                    found = true;
                    expect(attr(*token, u"mathvariant") == u"normal");
                }
            expect(found);
            auto atoms = amsNodes(f.ok(u"\\operatorname{a+b}"), NodeKind::tex_atom);
            expect(std::ranges::any_of(atoms, [](auto node) {
                return node->properties().tex.explicit_tex_class == TexClass::op &&
                       node->texClass() == TexClass::op;
            }));
            auto named = amsNodes(f.ok(u"\\operatorname{lim}"), NodeKind::mi);
            expect(named.size() == 1_u);
            if (named.size() == 1)
                expect(named[0]->texClass() == TexClass::op);
            expect(f.text(u"\\operatorname{lim}\\alpha") == u"lim\u2061α");
        };
    "ams DeclareMathOperator persists scopes and global starred operator definitions"_test = [] {
        AmsFixture f(false, true);
        f.ok(u"\\DeclareMathOperator{\\foo}{Foo}");
        auto definition = f.jax->session().findMacro(u"foo");
        expect(definition != nullptr);
        if (definition)
            expect(definition->replacement_source == u"\\operatorname{Foo}");
        f.ok(u"\\begingroup\\DeclareMathOperator{\\foo}{Local}");
        expect(f.text(u"\\foo") == u"Local");
        expect(f.text(u"\\endgroup\\foo") == u"Foo");
        f.ok(u"\\begingroup\\global\\DeclareMathOperator*{\\foo}{Global}\\endgroup");
        auto &root = f.ok(u"\\foo_x");
        expect(amsNodes(root, NodeKind::munder).size() == 1_u);
    };
    "ams sideset retains missing script slots prescript alignment and extra material"_test = [] {
        AmsFixture f;
        auto multis = amsNodes(f.ok(u"\\sideset{_a^b}{_c^d}{\\sum}"), NodeKind::mmultiscripts);
        expect(multis.size() == 1_u);
        if (multis.size() == 1) {
            expect(multis[0]->children().size() == 6_u);
            expect(multis[0]->properties().tex.script_alignment == HorizontalAlignment::left);
        }
        multis = amsNodes(f.ok(u"\\sideset{_a}{}{\\sum}"), NodeKind::mmultiscripts);
        expect(multis.size() == 1_u);
        if (multis.size() == 1)
            expect(amsNodes(*multis[0], NodeKind::none).size() == 1_u);
        auto &extra = f.ok(u"\\sideset{_a+x}{_b+y}{\\sum}");
        expect(amsNodes(extra, NodeKind::mphantom).size() == 1_u);
        expect(amsNodes(extra, NodeKind::mmultiscripts).empty());
        expect(amsNodes(f.ok(u"\\sideset{}{_a}{\\sum}"), NodeKind::msub).size() == 1_u);
    };
    "ams MultiIntegral limits and xArrow upper lower empty and minsize use actual scripts"_test =
        [] {
            AmsFixture f;
            auto integral = amsNodes(f.ok(u"\\idotsint_a^b"), NodeKind::mo);
            expect(std::ranges::count_if(integral, [](auto node) {
                       return NodeUtil::getText(*node) == u"∫";
                   }) == 2_i);
            expect(amsNodes(f.ok(u"\\idotsint\\limits_a^b"), NodeKind::munderover).size() == 1_u);
            auto &both = f.ok(u"\\xrightarrow[b]{a}");
            expect(amsNodes(both, NodeKind::munderover).size() == 1_u);
            expect(amsNodes(both, NodeKind::mpadded).size() == 2_u);
            auto &empty = f.ok(u"\\xleftarrow[]{a}");
            expect(amsNodes(empty, NodeKind::mover).size() == 1_u);
            expect(amsNodes(empty, NodeKind::mpadded).size() == 1_u);
            auto arrows = amsNodes(f.ok(u"\\widearrow{a}"), NodeKind::mo);
            expect(std::ranges::any_of(arrows, [](auto node) {
                return NodeUtil::getText(*node) == u"→" && attr(*node, u"minsize") == u"3em";
            }));
        };
    "ams CFrac and Genfrac retain align delimiters thickness and numeric style prefixes"_test = [] {
        AmsFixture f;
        for (auto align : {u"", u"l", u"r"}) {
            auto fractions =
                amsNodes(f.ok(u"\\cfrac[" + std::u16string(align) + u"]{a}{b}"), NodeKind::mfrac);
            expect(fractions.size() == 1_u);
            if (fractions.size() == 1)
                expect(attr(*fractions[0], u"numalign") ==
                       (std::u16string_view(align) == u"l"   ? u"left"
                        : std::u16string_view(align) == u"r" ? u"right"
                                                             : u"center"));
        }
        struct Style {
            std::u16string_view input;
            double level;
            bool display;
        };
        for (auto const &style :
             {Style{u"0", 0, true}, Style{u"1", 0, false}, Style{u"2", 1, false},
              Style{u"3", 2, false}, Style{u"2tail", 1, false}}) {
            auto fractions =
                amsNodes(f.ok(u"\\genfrac{(}{)}{0pt}{" + std::u16string(style.input) + u"}{a}{b}"),
                         NodeKind::mfrac);
            expect(fractions.size() == 1_u);
            if (fractions.size() == 1) {
                expect(attr(*fractions[0], u"linethickness") == u"0pt");
                expect(fractions[0]->properties().tex.with_delimiters.value_or(false));
                expect(numericAttribute(*fractions[0], u"scriptlevel", style.level));
                expect(booleanAttribute(*fractions[0], u"displaystyle", style.display));
            }
        }
        expect(f.text(u"\\genfrac{}{}{}{}{a}{b}") == u"ab");
    };
    "ams tag notag label and references use actual equation tag state"_test = [] {
        AmsFixture f(true);
        auto &tagged = f.ok(u"\\begin{align}a&=b\\tag{A}\\label{first}\\end{align}");
        expect(amsNodes(tagged, NodeKind::mlabeledtr).size() == 1_u);
        auto label = f.jax->session().findLabel(u"first");
        expect(label != nullptr);
        if (label)
            expect(label->tag == u"A");
        expect(f.text(u"\\eqref{first}") == u"(A)");
        expect(f.text(u"\\ref{first}") == u"A");
        auto &untagged = f.ok(u"\\begin{align}a&=b\\notag\\end{align}");
        expect(amsNodes(untagged, NodeKind::mlabeledtr).empty());
        auto raw = amsNodes(f.ok(u"x\\tag*{raw}"), NodeKind::mtext);
        expect(
            std::ranges::any_of(raw, [](auto node) { return NodeUtil::getText(*node) == u"raw"; }));
    };
    "ams method errors retain precise checks and release parsers before subsequent formula"_test =
        [] {
            struct Example {
                std::u16string_view source;
                std::string_view id;
            };
            for (auto const &example :
                 {Example{u"\\begin{alignat}{x}a\\end{alignat}", "PositiveIntegerArg"},
                  Example{u"\\begin{xalignat}{x}a\\end{xalignat}", "PositiveIntegerArg"},
                  Example{u"\\shoveleft x", "CommandOnlyAllowedInEnv"},
                  Example{u"\\begin{multline*}x\\shoveright y\\end{multline*}",
                          "CommandAtTheBeginingOfLine"},
                  Example{u"\\cfrac[q]{a}{b}", "IllegalAlign"},
                  Example{u"\\cfrac[q]{\\unknownAmsCommand}{b}", "UndefinedControlSequence"},
                  Example{u"\\genfrac{bad}{}{}{}{a}{b}", "MissingOrUnrecognizedDelim"},
                  Example{u"\\genfrac{}{}{}{4}{a}{b}", "BadMathStyleFor"},
                  Example{u"\\genfrac{}{}{}{4}{\\unknownAmsCommand}{b}",
                          "UndefinedControlSequence"},
                  Example{u"x\\tag{a}\\tag{b}", "MultipleCommand"},
                  Example{u"\\begin{split}a&=b\\tag{a}\\end{split}", "CommandNotAllowedInEnv"},
                  Example{u"\\begin{align}a&=b\\label{x}\\label{y}\\end{align}", "MultipleCommand"},
                  Example{u"\\begin{align}a&=b\\label{x}\\\\c&=d\\label{x}\\end{align}",
                          "MultipleLabel"}}) {
                AmsFixture f(true);
                f.compile(std::u16string(example.source));
                expect(f.error == example.id);
                expect(f.text(u"a+b") == u"a+b");
            }
        };
};
