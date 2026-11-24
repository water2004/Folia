#include "math/support/tex_fixture.hpp"
using namespace boost::ut;
using namespace folia::math::tex;
using namespace folia::math::mml;
suite base_items = [] {
    "base item catalog constructs all26 original frame classes with real ParseOptions"_test = [] {
        Fixture f;
        StackGlobal global;
        const std::array order{
            StackKind::start,     StackKind::stop,   StackKind::open,   StackKind::close,
            StackKind::null_item, StackKind::prime,  StackKind::subsup, StackKind::over,
            StackKind::left,      StackKind::middle, StackKind::right,  StackKind::break_item,
            StackKind::begin,     StackKind::end,    StackKind::style,  StackKind::position,
            StackKind::cell,      StackKind::mml,    StackKind::fn,     StackKind::not_item,
            StackKind::nonscript, StackKind::dots,   StackKind::array,  StackKind::eqnarray,
            StackKind::equation,  StackKind::mstyle};
        expect(std::ranges::equal(f.factory().getKinds(), order));
        for (auto kind : order) {
            StackItemInit init = [&]() -> StackItemInit {
                switch (kind) {
                case StackKind::start:
                    return StartItemInit{global};
                case StackKind::prime:
                    return PrimeItemInit{f.token(), f.token(NodeKind::mo, u"′")};
                case StackKind::subsup:
                    return SubsupItemInit{f.node(NodeKind::msubsup)};
                case StackKind::left:
                    return LeftItemInit{u"("};
                case StackKind::middle:
                case StackKind::right:
                    return MiddleRightItemInit{u"|", u"red"};
                case StackKind::break_item:
                    return BreakItemInit{u"newline", false};
                case StackKind::mml:
                    return MmlItemInit{f.token()};
                case StackKind::fn:
                    return FnItemInit{f.token()};
                case StackKind::eqnarray:
                    return EqnArrayItemInit{u"eqnarray", false, true};
                case StackKind::equation:
                    return EquationItemInit{false};
                case StackKind::mstyle:
                    return MstyleItemInit{{}, u"color"};
                default:
                    return NoItemInit{};
                }
            }();
            auto frame = f.item(kind, init);
            expect(frame->kind() == kind);
            expect(f.factory().nodeIsKind(*frame, kind));
            const bool open = kind == StackKind::start || kind == StackKind::open ||
                              kind == StackKind::left || kind == StackKind::begin ||
                              kind == StackKind::array || kind == StackKind::eqnarray ||
                              kind == StackKind::equation || kind == StackKind::mstyle;
            expect(frame->isOpen() == open);
            expect(frame->isFinal() == (kind == StackKind::mml));
            if (kind == StackKind::eqnarray || kind == StackKind::equation)
                f.options->tags->end();
        }
        expect(!f.item(StackKind::array)->copyEnv());
    };
    "base prime items honor two and three slot script family positions"_test = [] {
        Fixture f;
        for (auto [kind, index] :
             std::array<std::pair<NodeKind, std::size_t>, 4>{{{NodeKind::msup, 1},
                                                              {NodeKind::msubsup, 2},
                                                              {NodeKind::mover, 1},
                                                              {NodeKind::munderover, 2}}}) {
            auto &base = f.node(kind);
            const std::array<MmlNode *, 1> initial{&f.token()};
            base.setChildren(initial);
            auto &prime = f.token(NodeKind::mo, u"′");
            auto frame = f.item(StackKind::prime, PrimeItemInit{base, prime});
            auto result = frame->checkItem(f.item(StackKind::stop));
            expect(result.action == CheckAction::replay);
            expect(&replayNode(result) == &base);
            expect(base.children()[index] == &prime);
            expect(prime.parent() == &base);
            expect(std::holds_alternative<ReuseIncoming>(result.replacements.at(1)));
        }
    };
    "base subsup item preserves sparse primes and explicit false limits"_test = [] {
        Fixture f;
        auto &base = f.node(NodeKind::msubsup);
        const std::array<MmlNode *, 1> initial{&f.token()};
        base.setChildren(initial);
        auto frame = f.item(StackKind::subsup, SubsupItemInit{base});
        auto &script = dynamic_cast<SubsupItem &>(*frame);
        script.position = folia::math::tex::ScriptPosition::sub;
        script.primes = &f.token(NodeKind::mo, u"′");
        script.movesupsub = false;
        auto &lower = f.token(NodeKind::mi, u"i");
        auto result = frame->checkItem(f.mml(lower));
        expect(base.children().size() == 3_u);
        expect(base.children()[1] == &lower);
        expect(base.children()[2] == script.primes);
        expect(base.properties().tex.move_sup_sub == false);
        expect(&replayNode(result) == &base);
        texError([&] { frame->checkItem(f.item(StackKind::stop)); }, "MissingScript");
    };
    "base group and environment errors retain original names"_test = [] {
        Fixture f;
        auto open = f.item(StackKind::open);
        const std::array<MmlNode *, 1> contents{&f.token()};
        open->Push(contents);
        auto closed = open->checkItem(f.item(StackKind::close));
        expect(replayNode(closed).kind() == NodeKind::tex_atom);
        texError([&] { f.item(StackKind::open)->checkItem(f.item(StackKind::stop)); },
                 "ExtraOpenMissingClose");
        auto left = f.item(StackKind::left, LeftItemInit{u"("});
        texError([&] { left->checkItem(f.item(StackKind::stop)); }, "ExtraLeftMissingRight");
        auto begin = f.item(StackKind::begin);
        dynamic_cast<BeginItem &>(*begin).name = u"matrix";
        auto end = f.item(StackKind::end);
        dynamic_cast<EndItem &>(*end).name = u"array";
        texError([&] { begin->checkItem(end); }, "EnvBadEnd");
        texError([&] { begin->checkItem(f.item(StackKind::stop)); }, "EnvMissingEnd");
        auto cell = f.item(StackKind::cell);
        dynamic_cast<CellItem &>(*cell).name = u"&";
        try {
            open->checkItem(cell);
            expect(false);
        } catch (TexError const &e) {
            expect(e.message() == u"Misplaced &");
        }
    };
    "base over style and position items retain mathematical node attributes"_test = [] {
        Fixture f;
        auto frame = f.item(StackKind::over);
        auto &over = dynamic_cast<OverItem &>(*frame);
        over.numerator = &f.token(NodeKind::mi, u"a");
        over.thickness = 0.0;
        const std::array<MmlNode *, 1> contents{&f.token(NodeKind::mi, u"b")};
        frame->Push(contents);
        auto result = frame->checkItem(f.item(StackKind::close));
        auto &fraction = replayNode(result);
        expect(fraction.kind() == NodeKind::mfrac);
        expect(std::get<double>(*fraction.attributes().own(u"linethickness")) == 0.0);
        expect(attr(fraction, u"data-latex-item") == u"\\over");
        texError([&] { frame->checkItem(f.item(StackKind::over)); }, "AmbiguousUseOf");
        auto style = f.item(StackKind::style);
        dynamic_cast<StyleItem &>(*style).styles.set(u"mathcolor", u"red");
        const std::array<MmlNode *, 1> styled{&f.token()};
        style->Push(styled);
        auto styledResult = style->checkItem(f.item(StackKind::stop));
        expect(attr(replayNode(styledResult), u"mathcolor") == u"red");
        auto position = f.item(StackKind::position);
        auto &p = dynamic_cast<PositionItem &>(*position);
        p.move = PositionMove::vertical;
        p.dh = u"1em";
        p.dd = u"-1em";
        auto moved = position->checkItem(f.mml(f.token()));
        expect(attr(replayNode(moved), u"height") == u"1em");
        expect(attr(replayNode(moved), u"depth") == u"-1em");
        texError([&] { position->checkItem(f.item(StackKind::close)); }, "MissingBoxFor");
    };
    "base function not nonscript dots and break use original operator rules"_test = [] {
        Fixture f;
        auto fn = f.item(StackKind::fn, FnItemInit{f.token(NodeKind::mi, u"sin")});
        auto applied = fn->checkItem(f.mml(f.token()));
        expect(NodeUtil::getText(replayNode(applied, 1)) == u"\u2061");
        auto &equal = f.token(NodeKind::mo, u"=");
        auto neg = f.item(StackKind::not_item);
        neg->checkItem(f.mml(equal));
        expect(NodeUtil::getText(equal) == u"≠");
        auto &x = f.token(NodeKind::mo, u"x");
        f.item(StackKind::not_item)->checkItem(f.mml(x));
        expect(NodeUtil::getText(x) == u"x\u0338");
        auto space = f.mml(f.token(NodeKind::mspace, u""));
        f.item(StackKind::nonscript)->checkItem(space);
        expect(f.options->request().nodeLists[u"nonscript"].back() == space->First());
        auto dotsFrame = f.item(StackKind::dots);
        auto &dots = dynamic_cast<DotsItem &>(*dotsFrame);
        dots.ldots = &f.token(NodeKind::mo, u"…");
        dots.cdots = &f.token(NodeKind::mo, u"⋯");
        auto dotted = dotsFrame->checkItem(f.mml(f.token(NodeKind::mo, u"+")));
        expect(&replayNode(dotted) == dots.cdots);
        auto plus = f.mml(f.token(NodeKind::mo, u"+"));
        f.item(StackKind::break_item, BreakItemInit{u"newline", false})->checkItem(plus);
        expect(attr(*plus->First(), u"linebreak") == u"newline");
    };
    "base array row spacing frames and repeating equation columns match Source"_test = [] {
        Fixture f;
        ArrayItem array(f.factory());
        array.arraydef.set(u"rowspacing", u"4pt");
        array.arraydef.set(u"rowlines", u"solid dashed");
        for (int i = 0; i < 2; ++i) {
            const std::array<MmlNode *, 1> entry{&f.token()};
            array.Push(entry);
            array.EndEntry();
            array.EndRow();
        }
        array.checkLines();
        array.addRowSpacing(u"2pt");
        expect(attr(array.arraydef, u"rowspacing") == u"4pt 0.6em");
        expect(attr(array.arraydef, u"rowlines") == u"solid");
        expect(array.frame.size() == 1_u);
        expect(array.frame[0].edge == ArrayFrameRule::Edge::bottom);
        expect(array.frame[0].style.value() == u"dashed");
        ArrayItem framed(f.factory());
        framed.frame = {{ArrayFrameRule::Edge::left, u"solid"},
                        {ArrayFrameRule::Edge::right, u"solid"}};
        expect(framed.createMml().kind() == NodeKind::menclose);
        ArrayItem dashed(f.factory());
        dashed.frame = {{ArrayFrameRule::Edge::top, u"dashed"},
                        {ArrayFrameRule::Edge::left, u"solid"}};
        expect(attr(dashed.createMml(), u"data-frame-styles") == u"dashed none none solid");
        EqnArrayItem eqn(f.factory(), u"align", false, true);
        eqn.arraydef.set(u"columnalign", u"right left");
        eqn.arraydef.set(u"columnwidth", u"auto auto");
        eqn.arraydef.set(u"columnspacing", u"0 1em");
        for (int i = 0; i < 3; ++i) {
            const std::array<MmlNode *, 1> entry{&f.token()};
            eqn.Push(entry);
            eqn.EndEntry();
        }
        eqn.EndRow();
        eqn.EndTable();
        expect(attr(eqn.arraydef, u"columnalign") == u"right left right");
        expect(attr(eqn.arraydef, u"columnspacing") == u"0 1em");
        auto *styled = eqn.table[0]->children()[1]->children()[0]->children()[0];
        expect(attr(*styled, u"indentshift") == u"2em");
    };
    "array template scanning uses real TexParser source and preserves nested contents"_test = [] {
        Fixture f;
        TexParser parser(u"", ParserEnvironmentSeed{}, *f.options);
        ArrayProbe array(f.factory());
        array.parser = &parser;
        parser.setString(u"\\hline {a&b}\\begin{array}x&y\\end{array}&tail");
        parser.setPosition(0);
        auto entry = array.getEntry();
        expect(entry.found);
        expect(entry.prefix == u"\\hline");
        expect(entry.entry == u" {a&b}\\begin{array}x&y\\end{array}");
        expect(entry.terminator == u"&");
        expect(parser.string() == u"&tail");
        parser.setString(u"a&tail");
        parser.setPosition(0);
        array.cstart = {u"x"};
        array.cend = {u"y"};
        array.StartEntry();
        expect(parser.string() == u"xay&tail");
        expect(parser.position() == 0_u);
        expect(array.templateSubs == 1_u);
        f.effective.maxTemplateSubtitutions = 1;
        parser.setString(u"a&tail");
        parser.setPosition(0);
        texError([&] { array.StartEntry(); }, "MaxTemplateSubs");
        parser.mml();
    };
};
