#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.base_methods;
import folia.math.input.tex.parse_methods;

namespace {
using folia::math::regexp_host::RegExp;
using namespace std::literals;
void plainCharacter(TexParser &parser, std::u16string_view text) {
    parser.Push(&parser.createToken(NodeKind::mo, text));
}
void baseSetup(Fixture &f) {
    f.effective.numberPattern = std::make_shared<RegExp>(u"^[0-9]+");
    f.effective.identifierPattern = std::make_shared<RegExp>(u"^[a-zA-Z]+");
    f.effective.baseURL = u"";
    f.maps.registerMap(std::make_shared<RegExpMap>(
        RegExpMapInit{{u"letters", TokenMapParser{&ParseMethods::variable}},
                      std::make_shared<RegExp>(u"[a-zA-Z]")}));
    f.maps.registerMap(std::make_shared<RegExpMap>(RegExpMapInit{
        {u"numbers", TokenMapParser{&ParseMethods::digit}}, std::make_shared<RegExp>(u"[0-9]")}));
    f.maps.registerMap(std::make_shared<RegExpMap>(
        RegExpMapInit{{u"control", TokenMapParser{&ParseMethods::controlSequence}},
                      std::make_shared<RegExp>(u"\\\\")}));
    std::vector<Macro> characters;
    auto character = [&](std::u16string s, MacroHandler method) {
        characters.emplace_back(std::move(s), method);
    };
    character(u"{", &BaseMethods::Open);
    character(u"}", &BaseMethods::Close);
    character(u"|", &BaseMethods::Bar);
    character(u"~", &BaseMethods::Tilde);
    character(u" ", &BaseMethods::Space);
    character(u"\n", &BaseMethods::Space);
    character(u"^", &BaseMethods::Superscript);
    character(u"_", &BaseMethods::Subscript);
    character(u"'", &BaseMethods::Prime);
    character(u"%", &BaseMethods::Comment);
    character(u"#", &BaseMethods::Hash);
    character(u"&", &BaseMethods::Entry);
    f.maps.registerMap(std::make_shared<MacroMap>(
        MacroMapInit{{u"characters", std::nullopt}, std::move(characters)}));
    std::vector<Macro> commands;
    auto command = [&](std::u16string name, MacroHandler method,
                       std::vector<TexArgument> args = {}) {
        commands.emplace_back(std::move(name), method, std::move(args));
    };
    command(u"mathrm", &BaseMethods::MathFont, {u"normal"s});
    command(u"rm", &BaseMethods::SetFont, {u"normal"s});
    command(u"bf", &BaseMethods::SetFont, {u"bold"s});
    command(u"displaystyle", &BaseMethods::SetStyle, {u"D"s, true, 0.0});
    command(u"scriptstyle", &BaseMethods::SetStyle, {u"S"s, false, 1.0});
    command(u"small", &BaseMethods::SetSize, {0.85});
    command(u",", &BaseMethods::Spacer, {1.0 / 6});
    command(u"timesbreak", &BaseMethods::DiscretionaryTimes);
    command(u"allowbreak", &BaseMethods::AllowBreak);
    command(u"break", &BaseMethods::Break);
    command(u"goodbreak", &BaseMethods::Linebreak, {u"goodbreak"s});
    command(u"left", &BaseMethods::LeftRight);
    command(u"right", &BaseMethods::LeftRight);
    command(u"middle", &BaseMethods::LeftRight);
    command(u"sin", &BaseMethods::NamedFn);
    command(u"lim", &BaseMethods::NamedOp);
    command(u"limits", &BaseMethods::Limits, {true});
    command(u"nolimits", &BaseMethods::Limits, {false});
    command(u"over", &BaseMethods::Over);
    command(u"above", &BaseMethods::Over);
    command(u"atopwithdelims", &BaseMethods::Over);
    command(u"frac", &BaseMethods::Frac);
    command(u"sqrt", &BaseMethods::Sqrt);
    command(u"root", &BaseMethods::Root);
    command(u"leftroot", &BaseMethods::MoveRoot, {u"leftRoot"s});
    command(u"uproot", &BaseMethods::MoveRoot, {u"upRoot"s});
    command(u"hat", &BaseMethods::Accent, {u"005E"s});
    command(u"vec", &BaseMethods::Accent, {u"2192"s, false});
    command(u"overline", &BaseMethods::UnderOver, {u"2015"s});
    command(u"underbrace", &BaseMethods::UnderOver, {u"23DF"s, true});
    command(u"overset", &BaseMethods::Overset);
    command(u"underset", &BaseMethods::Underset);
    command(u"overunderset", &BaseMethods::Overunderset);
    command(u"mathop", &BaseMethods::TeXAtom, {1.0});
    command(u"mathrel", &BaseMethods::TeXAtom, {3.0});
    command(u"vbox", &BaseMethods::VBox, {u"bottom"s});
    command(u"hsize", &BaseMethods::Hsize);
    command(u"parbox", &BaseMethods::ParBox);
    command(u"breakAlign", &BaseMethods::BreakAlign);
    command(u"mmlToken", &BaseMethods::MmlToken);
    command(u"strut", &BaseMethods::Strut);
    command(u"phantom", &BaseMethods::Phantom);
    command(u"vphantom", &BaseMethods::Phantom, {1.0, 0.0});
    command(u"hphantom", &BaseMethods::Phantom, {0.0, 1.0});
    command(u"smash", &BaseMethods::Smash);
    command(u"llap", &BaseMethods::Lap);
    command(u"rlap", &BaseMethods::Lap);
    command(u"raise", &BaseMethods::RaiseLower);
    command(u"lower", &BaseMethods::RaiseLower);
    command(u"moveleft", &BaseMethods::MoveLeftRight);
    command(u"moveright", &BaseMethods::MoveLeftRight);
    command(u"hskip", &BaseMethods::Hskip);
    command(u"kern", &BaseMethods::Hskip, {true});
    command(u"nonscript", &BaseMethods::Nonscript);
    command(u"Rule", &BaseMethods::Rule);
    command(u"Space", &BaseMethods::Rule, {u"blank"s});
    command(u"rule", &BaseMethods::rule);
    command(u"bigl", &BaseMethods::MakeBig, {4.0, 0.85});
    command(u"biggl", &BaseMethods::MakeBig, {4.0, 1.45});
    command(u"bigr", &BaseMethods::MakeBig, {5.0, 0.85});
    command(u"biggr", &BaseMethods::MakeBig, {5.0, 1.45});
    command(u"buildrel", &BaseMethods::BuildRel);
    command(u"hbox", &BaseMethods::HBox, {0.0});
    command(u"text", &BaseMethods::HBox);
    command(u"fbox", &BaseMethods::FBox);
    command(u"framebox", &BaseMethods::FrameBox);
    command(u"makebox", &BaseMethods::MakeBox);
    command(u"not", &BaseMethods::Not);
    command(u"dots", &BaseMethods::Dots);
    command(u"matrix", &BaseMethods::Matrix);
    command(u"pmatrix", &BaseMethods::Matrix, {u"("s, u")"s});
    command(u"cases", &BaseMethods::Matrix,
            {u"{"s, u""s, u"left left"s, NullArgument{}, u".2em"s, NullArgument{}, true});
    command(u"numericmatrix", &BaseMethods::Matrix,
            {NullArgument{}, NullArgument{}, NullArgument{}, NullArgument{}, NullArgument{},
             NullArgument{}, NullArgument{}, 2.0});
    command(u"cr", &BaseMethods::Cr);
    command(u"\\", &BaseMethods::CrLaTeX);
    command(u"newline", &BaseMethods::CrLaTeX, {true});
    command(u"hline", &BaseMethods::HLine);
    command(u"hdashline", &BaseMethods::HLine, {u"dashed"s});
    command(u"hfill", &BaseMethods::HFill);
    command(u"newcolumntype", &BaseMethods::NewColumnType);
    command(u"begin", &BaseMethods::BeginEnd);
    command(u"end", &BaseMethods::BeginEnd);
    command(u"notag", &BaseMethods::HandleNoTag);
    command(u"label", &BaseMethods::HandleLabel);
    command(u"ref", &BaseMethods::HandleRef);
    command(u"eqref", &BaseMethods::HandleRef, {true});
    command(u"swap", &BaseMethods::Macro, {u"#2+#1"s, 2.0});
    command(u"optional", &BaseMethods::Macro, {u"#1+#2"s, 2.0, u"d"s});
    command(u"mathchoice", &BaseMethods::MathChoice);
    auto map = std::make_shared<CommandMap>(
        CommandMapInit{MacroMapInit{{u"commands", std::nullopt}, std::move(commands)}});
    map->add(u"aliasfrac", Macro(u"frac", &BaseMethods::Frac));
    map->add(u"aliaslimits", Macro(u"limits", &BaseMethods::Limits, {true}));
    f.maps.registerMap(map);
    std::vector<Token> delimiters;
    for (auto c : std::u16string(u"().|[]{}"))
        delimiters.emplace_back(std::u16string(1, c),
                                std::u16string(c == u'.' ? u"" : std::u16string(1, c)));
    f.maps.registerMap(std::make_shared<CharacterMap>(
        CharacterMapInit{{u"delimiters", std::nullopt}, std::move(delimiters)}));
    std::vector<Macro> environments;
    environments.emplace_back(u"array", &BaseMethods::AlignedArray,
                              std::vector<TexArgument>{false});
    environments.emplace_back(
        u"plainarray", &BaseMethods::Array,
        std::vector<TexArgument>{false, NullArgument{}, NullArgument{}, u"cc"s});
    environments.emplace_back(u"indentalign", &BaseMethods::IndentAlign,
                              std::vector<TexArgument>{false});
    environments.emplace_back(u"equation", &BaseMethods::Equation,
                              std::vector<TexArgument>{false, true});
    environments.emplace_back(
        u"eqnarray", &BaseMethods::EqnArray,
        std::vector<TexArgument>{false, false, true, u"rcl"s, u"c"s, u"1em"s});
    f.maps.registerMap(std::make_shared<EnvironmentMap>(
        EnvironmentMapInit{MacroMapInit{{u"environments", std::nullopt}, std::move(environments)},
                           &ParseMethods::environment}));
    const std::array handlers{
        HandlerRegistration{HandlerKind::character,
                            {u"characters", u"control", u"letters", u"numbers"},
                            &plainCharacter},
        HandlerRegistration{HandlerKind::macro, {u"commands"}},
        HandlerRegistration{HandlerKind::delimiter, {u"delimiters"}},
        HandlerRegistration{HandlerKind::environment, {u"environments"}}};
    f.options->handlers.add(handlers);
}
std::vector<MmlNode *> nodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](MmlNode &n) {
        if (n.kind() == kind)
            result.push_back(&n);
    });
    return result;
}
std::u16string visible(MmlNode &root) {
    if (root.isToken())
        return NodeUtil::getText(root);
    std::u16string result;
    for (auto *c : root.children())
        if (c)
            result += visible(*c);
    return result;
}
MmlNode &parse(Fixture &f, std::u16string source) {
    TexParser parser(std::move(source), {}, *f.options);
    auto *result = parser.mml();
    if (!result)
        throw std::runtime_error("Expected actual parser result");
    return *result;
}
} // namespace

suite base_methods = [] {
    "base methods script digit recognition preserves source and argument boundaries"_test = [] {
        struct Case {
            std::u16string suffix, rewritten, argument, rendered;
        };
        const std::array cases{
            Case{u"12", u"1 2", u"1", u"x12"},
            Case{u" \t\u00a012", u" \t\u00a01 2", u"1", u"x12"},
            Case{u"{12}", u"{12}", u"12", u"x12"},
            Case{u"a", u"a", u"a", u"xa"},
            Case{u"١٢", u"١٢", u"١", u"x١٢"},
            Case{u"１２", u"１２", u"１", u"x１２"},
            Case{u"𝟙", u"𝟙", u"𝟙", u"x𝟙"},
            Case{u"\\mathrm{12}", u"\\mathrm{12}", u"12", u"x12"}};
        for (auto marker : {u'_', u'^'})
            for (auto const &item : cases) {
                Fixture f;
                baseSetup(f);
                auto prefix = std::u16string(u"x") + marker;
                TexParser parser(prefix + item.suffix, {}, *f.options);
                expect(static_cast<bool>(parser.string() == prefix + item.rewritten));
                auto root = parser.mml();
                expect(root != nullptr);
                if (!root) continue;
                expect(static_cast<bool>(visible(*root) == item.rendered));
                auto scripts = nodes(*root, NodeKind::msubsup);
                expect(scripts.size() == 1_u);
                if (scripts.empty()) continue;
                const auto slot = marker == u'_' ? 1u : 2u;
                auto const &children = scripts.front()->children();
                expect(children.size() > slot && children[slot] != nullptr);
                if (children.size() > slot && children[slot])
                    expect(static_cast<bool>(visible(*children[slot]) == item.argument));
            }
    };
    "base methods actual parser scripts primes braces and comments"_test = [] {
        Fixture f;
        baseSetup(f);
        auto &root = parse(f, u"{x_1^2}+y'''~|%ignored\nz");
        expect(nodes(root, NodeKind::msubsup).size() == 1_u);
        expect(nodes(root, NodeKind::msup).size() == 1_u);
        expect(visible(root).find(u"\u2034") != std::u16string::npos);
        auto operators = nodes(root, NodeKind::mo);
        auto bar = std::find_if(operators.begin(), operators.end(),
                                [](auto *n) { return NodeUtil::getText(*n) == u"|"; });
        expect(bar != operators.end());
        if (bar != operators.end())
            expect((*bar)->properties().tex.keep_attributes == u"stretchy");
        expect(visible(root).find(u"ignored") == std::u16string::npos);
        expect(visible(root).find(u"\u00a0") != std::u16string::npos);
        texError([&] { parse(f, u"#"); }, "CantUseHash1");
        texError([&] { parse(f, u"x^a^b"); }, "DoubleExponent");
        texError([&] { parse(f, u"x_a_b"); }, "DoubleSubscripts");
        texError([&] { parse(f, u"x^a'"); }, "DoubleExponentPrime");
    };
    "base methods actual parser font scopes styles sizes and function limits"_test = [] {
        Fixture f;
        baseSetup(f);
        auto &root =
            parse(f, u"\\mathrm{abc}{\\bf x}\\scriptstyle y\\small z\\sin t+\\lim_a^b\\nolimits");
        auto ids = nodes(root, NodeKind::mi);
        expect(std::any_of(ids.begin(), ids.end(), [](auto *n) {
            return NodeUtil::getText(*n) == u"abc" && attr(*n, u"mathvariant") == u"normal";
        }));
        auto styles = nodes(root, NodeKind::mstyle);
        expect(styles.size() >= 2_u);
        expect(std::any_of(styles.begin(), styles.end(),
                           [](auto *n) { return attr(*n, u"mathsize") == u"0.85em"; }));
        expect(nodes(root, NodeKind::msubsup).size() == 1_u);
        expect(visible(root).find(u"\u2061") != std::u16string::npos);
        auto &limits = parse(f, u"\\lim_a^b\\limits");
        expect(nodes(limits, NodeKind::munderover).size() == 1_u);
        texError([&] { parse(f, u"x\\limits"); }, "MisplacedLimits");
        try {
            parse(f, u"x\\aliaslimits");
            expect(false);
        } catch (TexError const &e) {
            expect(e.id() == "MisplacedLimits");
            expect(e.message() == u"\\aliaslimits is allowed only on operators");
        }
    };
    "base methods actual parser fractions roots and root offsets"_test = [] {
        Fixture f;
        baseSetup(f);
        auto &root = parse(f, u"\\aliasfrac{a}{b}+{a\\over b}+{c\\above 2pt "
                              u"d}+{e\\atopwithdelims()f}+\\sqrt[\\leftroot{3}\\uproot{-2}n]{x}+"
                              u"\\root 3\\of y+\\sqrt\\frac12");
        expect(nodes(root, NodeKind::mfrac).size() == 5_u);
        expect(nodes(root, NodeKind::mroot).size() == 2_u);
        expect(nodes(root, NodeKind::msqrt).size() == 1_u);
        auto padded = nodes(root, NodeKind::mpadded);
        expect(std::any_of(padded.begin(), padded.end(),
                           [](auto *n) { return attr(*n, u"width") == u"+0.2em"; }));
        texError([&] { parse(f, u"\\leftroot{1}"); }, "MisplacedMoveRoot");
        texError([&] { parse(f, u"\\sqrt[\\leftroot{x}]{a}"); }, "IntegerArg");
        texError([&] { parse(f, u"\\sqrt[\\leftroot{1}\\leftroot{2}]{a}"); }, "MultipleMoveRoot");
    };
    "base methods actual parser accents stack slots fences and relation classes"_test = [] {
        Fixture f;
        baseSetup(f);
        auto &root =
            parse(f, u"\\hat{x}+\\vec{y}+\\overline{a}+\\underbrace{b}+\\overset{a}{x}+\\underset{"
                     u"b}{x}+\\overunderset{a}{b}{x}+\\buildrel a\\over "
                     u"b+\\left(x\\middle|y\\right)+\\bigl[+\\mathrel{x}+\\mathop{\\rm abc}_a");
        expect(nodes(root, NodeKind::munderover).size() >= 5_u);
        expect(nodes(root, NodeKind::mover).size() == 1_u);
        expect(nodes(root, NodeKind::munder).size() == 1_u);
        auto scripts = nodes(root, NodeKind::munderover);
        expect(std::any_of(scripts.begin(), scripts.end(), [](auto *n) {
            return n->children().size() == 3 && !n->children()[1] && n->children()[2];
        }));
        auto atoms = nodes(root, NodeKind::tex_atom);
        expect(std::any_of(atoms.begin(), atoms.end(), [](auto *n) {
            return n->properties().tex.explicit_tex_class == TexClass::rel;
        }));
        auto ops = nodes(root, NodeKind::mo);
        expect(std::any_of(ops.begin(), ops.end(),
                           [](auto *n) { return attr(*n, u"minsize") == u"1.2em"; }));
    };
    "base methods actual parser token attributes text and vertical boxes"_test = [] {
        Fixture f;
        baseSetup(f);
        auto &root =
            parse(f, u"\\mmlToken{mi}[mathvariant='bold', "
                     u"id=test]{x}+\\hbox{hello}+\\text{world}+\\fbox{a}+\\framebox[2em][r]{b}+"
                     u"\\makebox[3em][C]{c}+\\vbox{\\hsize=2em x}+\\parbox[t]{4em}{d}");
        auto ids = nodes(root, NodeKind::mi);
        expect(std::any_of(ids.begin(), ids.end(), [](auto *n) {
            return attr(*n, u"id") == u"test" && attr(*n, u"mathvariant") == u"bold" &&
                   n->properties().tex.keep_attributes == u"mathvariant id";
        }));
        expect(nodes(root, NodeKind::menclose).size() == 2_u);
        auto boxes = nodes(root, NodeKind::mpadded);
        expect(std::any_of(boxes.begin(), boxes.end(), [](auto *n) {
            return n->properties().tex.vertical_box == u"bottom" && attr(*n, u"width") == u"2em";
        }));
        expect(std::any_of(boxes.begin(), boxes.end(), [](auto *n) {
            return attr(*n, u"width") == u"3em" && attr(*n, u"data-overflow") == u"linebreak";
        }));
        texError([&] { parse(f, u"\\mmlToken{mrow}{x}"); }, "NotMathMLToken");
        texError([&] { parse(f, u"\\mmlToken{mi}[unknown=x]{a}"); }, "UnknownAttrForElement");
        texError([&] { parse(f, u"\\mmlToken{mi}[!=x]{a}"); }, "InvalidMathMLAttr");
    };
    "base methods actual parser phantom smash lap positions and rules"_test = [] {
        Fixture f;
        baseSetup(f);
        auto &root = parse(
            f, u"\\strut+\\phantom{x}+\\vphantom{y}+\\hphantom{z}+\\smash[t]{x}+\\llap{a}+\\rlap{b}"
               u"+\\raise 2pt{x}+\\lower -3pt{y}+\\moveleft 1em{a}+\\moveright "
               u"-2em{b}+\\Rule{1em}{2pt}{3pt}+\\Space{1em}{2pt}{3pt}+\\rule[-2pt]{1em}{3pt}");
        expect(nodes(root, NodeKind::mphantom).size() == 3_u);
        auto padded = nodes(root, NodeKind::mpadded);
        expect(std::any_of(padded.begin(), padded.end(),
                           [](auto *n) { return attr(*n, u"lspace") == u"-1width"; }));
        expect(std::any_of(padded.begin(), padded.end(),
                           [](auto *n) { return attr(*n, u"voffset") == u"+2pt"; }));
        expect(std::any_of(padded.begin(), padded.end(),
                           [](auto *n) { return attr(*n, u"depth") == u"+2pt"; }));
        auto spaces = nodes(root, NodeKind::mspace);
        expect(std::count_if(spaces.begin(), spaces.end(), [](auto *n) {
                   return attr(*n, u"mathbackground") == u"black";
               }) == 2_i);
    };
    "base methods actual parser spacing breaks negation dots and choices"_test = [] {
        Fixture f;
        baseSetup(f);
        auto &root =
            parse(f, u"x\\,\\hskip 2pt\\kern 3pt\\nonscript\\,y\\timesbreak z\\allowbreak\\break "
                     u"x\\goodbreak y\\not=+\\dots+\\mathchoice{D}{T}{S}{Q}\\\\[2pt]z");
        auto spaces = nodes(root, NodeKind::mspace);
        expect(std::any_of(spaces.begin(), spaces.end(),
                           [](auto *n) { return attr(*n, u"linebreak") == u"nobreak"; }));
        expect(std::any_of(spaces.begin(), spaces.end(),
                           [](auto *n) { return attr(*n, u"data-lineleading") == u"2pt"; }));
        expect(visible(root).find(u"≠") != std::u16string::npos);
        expect(nodes(root, NodeKind::math_choice).size() == 1_u);
    };
    "base methods actual parser matrices entries row spacing and rules"_test = [] {
        Fixture f;
        baseSetup(f);
        auto &root = parse(
            f, u"\\matrix{\\hline a\\hfill&b\\cr "
               u"c&d\\\\[2pt]\\hdashline}+\\pmatrix{x&y}+\\cases{x&some text\\cr y&\\text{other}}");
        auto tables = nodes(root, NodeKind::mtable);
        expect(tables.size() >= 3_u);
        expect(nodes(root, NodeKind::mtr).size() == 5_u);
        expect(nodes(root, NodeKind::mtd).size() == 10_u);
        expect(visible(root).find(u"some text") != std::u16string::npos);
        texError([&] { parse(f, u"\\hfill"); }, "UnsupportedHFill");
        texError([&] { parse(f, u"\\hline"); }, "Misplaced");
        texError([&] { parse(f, u"\\cases{x&a&b}"); }, "ExtraAlignTab");
    };
    "base methods actual parser column definitions array alignment and break alignment"_test = [] {
        Fixture f;
        baseSetup(f);
        auto &root = parse(f, u"\\newcolumntype{C}{c}\\begin{array}[t]{Cl}\\breakAlign{t}{tc}a&"
                              u"b\\\\c&d\\end{array}+\\begin{plainarray}a&b\\end{plainarray}");
        expect(f.session.columnDefinitions().find(u'C') != nullptr);
        expect(nodes(root, NodeKind::mtable).size() == 2_u);
        auto tables = nodes(root, NodeKind::mtable);
        expect(attr(*tables[0], u"align") == u"baseline 1");
        expect(attr(*tables[0], u"data-break-align") == u"top center");
        texError([&] { parse(f, u"\\newcolumntype{AB}{c}"); }, "BadColumnName");
        texError([&] { parse(f, u"\\newcolumntype{C}[x]{c}"); }, "PositiveIntegerArg");
        texError([&] { parse(f, u"\\breakAlign{c}{t}"); }, "BreakNotInArray");
    };
    "base methods actual parser environments indent labels and references"_test = [] {
        Fixture f;
        baseSetup(f);
        folia::math::core::AbstractMathItem item(u"", nullptr);
        f.options->request().mathItem = &item;
        auto &root = parse(f, u"\\begin{indentalign}[1em][2em][3em]{lcr}x\\end{indentalign}");
        auto styles = nodes(root, NodeKind::mstyle);
        expect(std::any_of(styles.begin(), styles.end(), [](auto *n) {
            return attr(*n, u"indentshift") == u"2em" && attr(*n, u"indentalignlast") == u"right";
        }));
        parse(f, u"\\begin{equation}a\\label{a}\\notag\\end{equation}");
        expect(item.isDisplay());
        expect(f.options->request().equation.labels.contains(u"a"));
        auto &array = parse(f, u"\\begin{eqnarray}a&=&b\\end{eqnarray}");
        expect(nodes(array, NodeKind::mtable).size() == 1_u);
        auto &refs = parse(f, u"\\ref{missing}+\\eqref{missing}");
        expect(f.options->request().equation.redo);
        expect(visible(refs).find(u"???") != std::u16string::npos);
        texError([&] { parse(f, u"\\begin{bad\\name}"); }, "InvalidEnv");
    };
    "base methods actual parser macro optional expansion session data and errors"_test = [] {
        Fixture f;
        baseSetup(f);
        expect(visible(parse(f, u"\\swap{a}{b}+\\optional{x}+\\optional[q]{r}")) == u"b+a+d+x+q+r");
        MacroDefinition definition;
        definition.replacement_source = u"#1+#2";
        definition.arity = 2;
        definition.parameter_delimiters.resize(2);
        definition.default_first_argument = u"k";
        f.session.defineMacro(u"sessionmacro", definition);
        f.maps.registerMap(std::make_shared<CommandMap>(SessionMacroMapInit{
            {u"sessioncommands", std::nullopt}, f.session, f.callbacks, &BaseMethods::Macro}));
        const std::array sessionHandler{
            HandlerRegistration{HandlerKind::macro, {u"sessioncommands"}}};
        f.options->handlers.add(sessionHandler);
        expect(visible(parse(f, u"\\sessionmacro{x}+\\sessionmacro[q]{r}")) == u"k+x+q+r");
        f.options->request().equation.pending_label.clear();
        f.options->tags->setLabel(u"");
        parse(f, u"\\label{x}");
        f.options->tags->setLabel(u"");
        texError([&] { parse(f, u"\\label{x}"); }, "MultipleLabel");
        f.effective.ignoreDuplicateLabels = true;
        f.options->tags->setLabel(u"");
        parse(f, u"\\label{x}");
        texError([&] { parse(f, u"\\label{y}"); }, "MultipleCommand");
        expect(splitAlignArray(u" t b m c ") == u"top bottom middle center");
        texError([] { splitAlignArray(u"x"); }, "BadBreakAlign");
        texError([] { splitAlignArray(u"tt", 1); }, "TooManyAligns");
    };
    "base methods actual parser empty rule color and numeric matrix side"_test = [] {
        Fixture f;
        baseSetup(f);
        TexEnvironment environment;
        environment.color = u"";
        TexParser parser(u"\\Rule{1em}{2pt}{3pt}+\\rule{2em}{4pt}",
                         ParserEnvironmentSeed{environment}, *f.options);
        auto *root = parser.mml();
        auto spaces = nodes(*root, NodeKind::mspace);
        expect(std::count_if(spaces.begin(), spaces.end(), [](auto *n) {
                   return attr(*n, u"mathbackground") == u"black";
               }) == 2_i);
        auto &matrix = parse(f, u"\\numericmatrix{a&b&1}");
        auto tables = nodes(matrix, NodeKind::mtable);
        auto *side = tables.at(0)->attributes().own(u"side");
        expect(side != nullptr);
        if (side)
            expect(std::get<double>(*side) == 2.0);
    };
};
