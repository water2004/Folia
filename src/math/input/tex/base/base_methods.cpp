/*************************************************************
 * Copyright (c) 2017-2026 The MathJax Consortium.
 * MathJax 4.1.3 input/tex/base/BaseMethods.ts; Apache-2.0.
 *************************************************************/
module folia.math.input.tex.base_methods;
import folia.math.input.tex.parse_util;
import folia.math.input.tex.unit_util;
import folia.math.input.tex.node_util;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.core.mml_tree.mml_nodes.msubsup;
import folia.math.core.mml_tree.mml_nodes.munderover;
import folia.math.util.string;
import folia.math.util.js_number;
import folia.math.util.lengths;
import std;

namespace folia::math::tex {
namespace {
using mml::MmlNode;
using mml::NodeKind;
using mml::TexClass;
using Arguments = std::span<TexArgument const>;
using namespace std::literals;
Arguments arguments(MacroInvocation const &invocation) {
    return std::get<Arguments>(invocation.arguments);
}
TexArgument const *argument(Arguments args, std::size_t i) {
    return i < args.size() ? &args[i] : nullptr;
}
bool present(Arguments args, std::size_t i) {
    auto *v = argument(args, i);
    return v && !std::holds_alternative<NullArgument>(*v);
}
std::u16string numberString(double value) {
    auto s = js_number::toString(value);
    return {s.begin(), s.end()};
}
std::u16string text(Arguments args, std::size_t i) {
    auto *v = argument(args, i);
    if (!v || std::holds_alternative<NullArgument>(*v))
        return {};
    if (auto *s = std::get_if<std::u16string>(v))
        return *s;
    if (auto *n = std::get_if<double>(v))
        return numberString(*n);
    return std::get<bool>(*v) ? u"true" : u"false";
}
bool flag(Arguments args, std::size_t i) {
    auto *v = argument(args, i);
    if (!v || std::holds_alternative<NullArgument>(*v))
        return false;
    if (auto *b = std::get_if<bool>(v))
        return *b;
    if (auto *n = std::get_if<double>(v))
        return *n != 0 && !std::isnan(*n);
    return !std::get<std::u16string>(*v).empty();
}
double number(std::u16string_view value) {
    std::string s(value.begin(), value.end());
    char *end{};
    auto result = std::strtod(s.c_str(), &end);
    return end == s.c_str() ? std::numeric_limits<double>::quiet_NaN() : result;
}
double number(Arguments args, std::size_t i) {
    auto *v = argument(args, i);
    if (!v || std::holds_alternative<NullArgument>(*v))
        return 0;
    if (auto *n = std::get_if<double>(v))
        return *n;
    if (auto *b = std::get_if<bool>(v))
        return *b ? 1 : 0;
    return number(std::get<std::u16string>(*v));
}
NodeDefinition definition(std::initializer_list<mml::AttributeSet::Entry> entries) {
    NodeDefinition result;
    for (auto const &[name, value] : entries)
        result.attributes.set(name, value);
    return result;
}
NodeDefinition texClass(TexClass value) {
    NodeDefinition result;
    result.properties.tex.explicit_tex_class = value;
    return result;
}
MmlNode &node(TexParser &p, NodeKind kind, std::initializer_list<MmlNode *> children = {},
              NodeDefinition def = {}) {
    return p.create(kind, std::span<MmlNode *const>(children.begin(), children.size()),
                    std::move(def));
}
DispatchOutcome push(TexParser &p, MmlNode &value) {
    p.Push(&value);
    return NoParseResult{};
}
DispatchOutcome push(TexParser &p, FrameHandle value) {
    p.Push(std::move(value));
    return NoParseResult{};
}
[[noreturn]] void error(std::string id, std::u16string message,
                        std::initializer_list<std::u16string> args = {}) {
    throw TexError(std::move(id), std::move(message), {args.begin(), args.size()});
}
std::optional<regexp_host::Match> match(std::u16string_view pattern, std::u16string_view value) {
    return regexp_host::RegExp(std::u16string(pattern)).exec(std::u16string(value));
}
std::u16string trim(std::u16string_view value) {
    return UnitUtil::instance().trimSpaces(value);
}
std::u16string lower(std::u16string value) {
    for (auto &c : value)
        if (c >= u'A' && c <= u'Z')
            c += u'a' - u'A';
    return value;
}
std::u16string em(double value) {
    auto s = lengths::em(value);
    return {s.begin(), s.end()};
}
std::u16string required(TexParser &p, std::u16string_view name) {
    return p.GetArgument(name).value();
}
MmlNode *child(MmlNode &value, std::size_t i) {
    auto children = value.children();
    return i < children.size() ? children[i] : nullptr;
}
bool isSubsup(MmlNode &value) {
    return dynamic_cast<mml::MmlMsubsup *>(&value);
}
bool isUnderover(MmlNode &value) {
    return dynamic_cast<mml::MmlMunderover *>(&value);
}
std::size_t scriptPosition(MmlNode &value, bool sup) {
    if (auto *s = dynamic_cast<mml::MmlMsubsup *>(&value))
        return sup ? s->sup() : s->sub();
    if (auto *s = dynamic_cast<mml::MmlMunderover *>(&value))
        return sup ? s->over() : s->under();
    return sup ? 2 : 1;
}
bool attributeTrue(mml::AttributeValue const *value) {
    return value && std::holds_alternative<bool>(*value) && std::get<bool>(*value);
}
bool accent(MmlNode &value) {
    auto *mo = value.coreMO();
    return mo && mo->kind() == NodeKind::mo &&
           attributeTrue(NodeUtil::getMoAttribute(*mo, u"accent"));
}
std::u16string entity(std::u16string_view hex) {
    std::uint32_t value{};
    for (auto c : hex) {
        value *= 16;
        value += c >= u'0' && c <= u'9'   ? c - u'0'
                 : c >= u'A' && c <= u'F' ? c - u'A' + 10
                                          : c - u'a' + 10;
    }
    if (value <= 0xffff)
        return std::u16string(1, static_cast<char16_t>(value));
    value -= 0x10000;
    return {static_cast<char16_t>(0xd800 + (value >> 10)),
            static_cast<char16_t>(0xdc00 + (value & 1023))};
}
std::u16string beginName(FrameHandle const &begin) {
    return std::u16string(begin->getName().value_or(u""));
}
MmlNode *parseRoot(TexParser &p, std::u16string source) {
    auto &env = p.stack.env();
    auto inRoot = env.in_root;
    env.in_root = true;
    TexParser parser(std::move(source), ParserEnvironmentSeed{env}, p.configuration);
    auto *result = parser.mml();
    auto &global = parser.stack.global();
    if (global.left_root || global.up_root) {
        NodeDefinition def;
        if (global.left_root)
            def.attributes.set(u"width", *global.left_root);
        if (global.up_root) {
            def.attributes.set(u"voffset", *global.up_root);
            def.attributes.set(u"height", *global.up_root);
        }
        result = &node(p, NodeKind::mpadded, {result}, std::move(def));
    }
    env.in_root = inRoot;
    return result;
}
DispatchOutcome script(TexParser &p, bool sup) {
    const auto next = p.GetNext();
    // Source /\d/ searches for an ASCII digit. Keep GetNext's whitespace
    // advancement and search the complete returned token without a regex.
    if (std::ranges::any_of(next, [](char16_t c) { return c >= u'0' && c <= u'9'; })) {
        std::u16string source(p.string());
        source.insert(std::min(p.position() + 1, source.size()), 1, u' ');
        p.setString(std::move(source));
    }
    auto *top = p.stack.Top();
    MmlNode *base{}, *primes{};
    if (top->kind() == StackKind::prime) {
        auto values = top->Peek(2);
        base = values[0];
        primes = values[1];
        p.stack.Pop();
    } else {
        base = p.stack.Prev();
        if (!base)
            base = &p.createToken(NodeKind::mi, u"");
    }
    auto moves = base->properties().tex.move_sup_sub;
    auto position = scriptPosition(*base, sup);
    auto existing = child(*base, position);
    if ((isSubsup(*base) && base->kind() != NodeKind::msup && existing) ||
        (isUnderover(*base) && base->kind() != NodeKind::mover && existing &&
         !base->properties().tex.sub_sup_allowed.value_or(false)))
        error(sup ? "DoubleExponent" : "DoubleSubscripts",
              sup ? u"Double exponent: use braces to clarify"
                  : u"Double subscripts: use braces to clarify");
    if (!isSubsup(*base) || base->kind() == NodeKind::msup) {
        if (moves.value_or(false)) {
            if (!isUnderover(*base) || base->kind() == NodeKind::mover || existing) {
                NodeDefinition def;
                def.properties.tex.move_sup_sub = true;
                base = &node(p, NodeKind::munderover, {base}, std::move(def));
            }
        } else
            base = &node(p, NodeKind::msubsup, {base});
        position = scriptPosition(*base, sup);
    }
    auto item = p.itemFactory().create(StackKind::subsup, SubsupItemInit{*base});
    auto &frame = dynamic_cast<SubsupItem &>(*item);
    frame.position = static_cast<ScriptPosition>(position);
    frame.primes = primes;
    frame.movesupsub = moves;
    return push(p, std::move(item));
}
} // namespace

std::u16string splitAlignArray(std::u16string_view align, double count) {
    std::vector<std::u16string> list;
    std::u16string compact;
    for (auto const &part : regexp_host::split(align, u"\\s+", u""))
        if (part)
            compact += *part;
    for (auto c : compact)
        switch (c) {
        case u't':
            list.emplace_back(u"top");
            break;
        case u'b':
            list.emplace_back(u"bottom");
            break;
        case u'm':
            list.emplace_back(u"middle");
            break;
        case u'c':
            list.emplace_back(u"center");
            break;
        default:
            error("BadBreakAlign", u"Invalid alignment character: %1", {std::u16string(1, c)});
        }
    if (list.size() > count)
        error("TooManyAligns", u"Too many alignment characters: %1", {std::u16string(align)});
    std::u16string result;
    for (auto const &part : list) {
        if (!result.empty())
            result += u' ';
        result += part;
    }
    return result;
}

DispatchOutcome BaseMethods::Open(TexParser &p, MacroInvocation const &) {
    return push(p, p.itemFactory().create(StackKind::open));
}
DispatchOutcome BaseMethods::Close(TexParser &p, MacroInvocation const &) {
    return push(p, p.itemFactory().create(StackKind::close));
}
DispatchOutcome BaseMethods::Bar(TexParser &p, MacroInvocation const &i) {
    auto def = texClass(TexClass::ord);
    def.attributes.set(u"stretchy", false);
    auto &mo = p.createToken(NodeKind::mo, i.dispatch_token, std::move(def));
    mo.properties().tex.keep_attributes = u"stretchy";
    return push(p, mo);
}
DispatchOutcome BaseMethods::Tilde(TexParser &p, MacroInvocation const &) {
    return push(p, p.createToken(NodeKind::mtext, u"\u00a0"));
}
DispatchOutcome BaseMethods::Space(TexParser &, MacroInvocation const &) {
    return NoParseResult{};
}
DispatchOutcome BaseMethods::Superscript(TexParser &p, MacroInvocation const &) {
    return script(p, true);
}
DispatchOutcome BaseMethods::Subscript(TexParser &p, MacroInvocation const &) {
    return script(p, false);
}
DispatchOutcome BaseMethods::Prime(TexParser &p, MacroInvocation const &) {
    auto *base = p.stack.Prev();
    if (!base)
        base = &p.createToken(NodeKind::mi, u"");
    if ((isSubsup(*base) && base->kind() != NodeKind::msup &&
         child(*base, scriptPosition(*base, true))) ||
        (isUnderover(*base) && base->kind() != NodeKind::mover &&
         child(*base, scriptPosition(*base, true)) &&
         !base->properties().tex.sub_sup_allowed.value_or(false)))
        error("DoubleExponentPrime", u"Prime causes double exponent: use braces to clarify");
    std::u16string primes, c;
    p.setPosition(p.position() - 1);
    do {
        primes += u'\u2032';
        p.setPosition(p.position() + 1);
        c = p.GetNext();
    } while (c == u"'" || c == u"\u2019");
    constexpr std::array<char16_t, 5> combined{0, u'\u2032', u'\u2033', u'\u2034', u'\u2057'};
    if (primes.size() < combined.size())
        primes.assign(1, combined[primes.size()]);
    NodeDefinition def;
    def.properties.tex.variant_form = true;
    auto &mo = p.createToken(NodeKind::mo, primes, std::move(def));
    return push(p, p.itemFactory().create(StackKind::prime, PrimeItemInit{*base, mo}));
}
DispatchOutcome BaseMethods::Comment(TexParser &p, MacroInvocation const &) {
    auto at = p.position();
    while (at < p.string().size() && p.string()[at] != u'\n')
        ++at;
    p.setPosition(at);
    return NoParseResult{};
}
DispatchOutcome BaseMethods::Hash(TexParser &, MacroInvocation const &) {
    error("CantUseHash1", u"You can't use 'macro parameter character #' in math mode");
}
DispatchOutcome BaseMethods::MathFont(TexParser &p, MacroInvocation const &i) {
    auto args = arguments(i);
    auto source = required(p, i.dispatch_token);
    TexEnvironment env;
    env.multi_letter_identifiers = p.options().identifierPattern;
    overlayEnvironment(env, p.stack.env());
    env.font = text(args, 0);
    env.italic_font = text(args, 1);
    env.no_auto_op = true;
    TexParser parser(std::move(source), ParserEnvironmentSeed{env}, p.configuration);
    return push(p, node(p, NodeKind::tex_atom, {parser.mml()}));
}
DispatchOutcome BaseMethods::SetFont(TexParser &p, MacroInvocation const &i) {
    p.stack.env().font = text(arguments(i), 0);
    return push(p, p.itemFactory().create(StackKind::null_item));
}
DispatchOutcome BaseMethods::SetStyle(TexParser &p, MacroInvocation const &i) {
    auto args = arguments(i);
    p.stack.env().style = text(args, 0);
    p.stack.env().script_level = number(args, 2);
    auto item = p.itemFactory().create(StackKind::style);
    auto &styles = dynamic_cast<StyleItem &>(*item).styles;
    styles.set(u"displaystyle", flag(args, 1));
    if (auto *v = argument(args, 2); v && std::holds_alternative<std::u16string>(*v))
        styles.set(u"scriptlevel", std::get<std::u16string>(*v));
    else
        styles.set(u"scriptlevel", number(args, 2));
    return push(p, std::move(item));
}
DispatchOutcome BaseMethods::SetSize(TexParser &p, MacroInvocation const &i) {
    auto size = number(arguments(i), 0);
    p.stack.env().size = size;
    auto item = p.itemFactory().create(StackKind::style);
    dynamic_cast<StyleItem &>(*item).styles.set(u"mathsize", em(size));
    return push(p, std::move(item));
}
DispatchOutcome BaseMethods::Spacer(TexParser &p, MacroInvocation const &i) {
    auto &space =
        node(p, NodeKind::mspace, {}, definition({{u"width", em(number(arguments(i), 0))}}));
    return push(p, node(p, NodeKind::mstyle, {&space}, definition({{u"scriptlevel", 0.0}})));
}
DispatchOutcome BaseMethods::DiscretionaryTimes(TexParser &p, MacroInvocation const &) {
    return push(p, p.createToken(NodeKind::mo, u"\u2062",
                                 definition({{u"linebreakmultchar", u"\u00d7"s}})));
}
DispatchOutcome BaseMethods::AllowBreak(TexParser &p, MacroInvocation const &) {
    return push(p, p.createToken(NodeKind::mspace, u""));
}
DispatchOutcome BaseMethods::Break(TexParser &p, MacroInvocation const &) {
    return push(p, p.createToken(NodeKind::mspace, u"", definition({{u"linebreak", u"newline"s}})));
}
DispatchOutcome BaseMethods::Linebreak(TexParser &p, MacroInvocation const &i) {
    auto linebreak = text(arguments(i), 0);
    bool insert = true;
    auto *prev = p.stack.Prev(true);
    if (prev && prev->kind() == NodeKind::mo) {
        auto *style = NodeUtil::getMoAttribute(*prev, u"linebreakstyle");
        if (!style || !std::holds_alternative<std::u16string>(*style) ||
            std::get<std::u16string>(*style) != u"before") {
            prev->attributes().set(u"linebreak", linebreak);
            insert = false;
        }
    }
    return push(p, p.itemFactory().create(StackKind::break_item, BreakItemInit{linebreak, insert}));
}
DispatchOutcome BaseMethods::LeftRight(TexParser &p, MacroInvocation const &i) {
    auto delimiter = p.GetDelimiter(i.dispatch_token).value();
    auto name = i.dispatch_token.substr(1);
    if (name == u"left")
        return push(p, p.itemFactory().create(StackKind::left, LeftItemInit{delimiter}));
    return push(p, p.itemFactory().create(name == u"middle" ? StackKind::middle : StackKind::right,
                                          MiddleRightItemInit{delimiter, p.stack.env().color}));
}
DispatchOutcome BaseMethods::NamedFn(TexParser &p, MacroInvocation const &i) {
    auto id = text(arguments(i), 0);
    if (id.empty())
        id = i.dispatch_token.substr(1);
    auto &mml = p.createToken(NodeKind::mi, id, texClass(TexClass::op));
    return push(p, p.itemFactory().create(StackKind::fn, FnItemInit{mml}));
}
DispatchOutcome BaseMethods::NamedOp(TexParser &p, MacroInvocation const &i) {
    auto id = text(arguments(i), 0);
    if (id.empty())
        id = i.dispatch_token.substr(1);
    if (auto at = id.find(u"&thinsp;"); at != id.npos)
        id.replace(at, 8, u"\u2006");
    auto def = texClass(TexClass::op);
    def.attributes.set(u"movablelimits", true);
    def.attributes.set(u"form", u"prefix");
    def.properties.tex.move_sup_sub = true;
    return push(p, p.createToken(NodeKind::mo, id, std::move(def)));
}
DispatchOutcome BaseMethods::Limits(TexParser &p, MacroInvocation const &i) {
    auto limits = flag(arguments(i), 0);
    auto *op = p.stack.Prev(true);
    if (!op || (op->coreMO()->texClass() != TexClass::op && !op->properties().tex.move_sup_sub))
        error("MisplacedLimits", u"%1 is allowed only on operators", {p.currentCS()});
    if ((isUnderover(*op) && !limits) || (isSubsup(*op) && limits)) {
        auto &replacement = node(p, limits ? NodeKind::munderover : NodeKind::msubsup);
        auto children = std::vector<MmlNode *>(op->children().begin(), op->children().end());
        for (std::size_t n = 0; n < children.size(); ++n)
            NodeUtil::setChild(replacement, n, children[n]);
        op = &replacement;
        p.stack.Top()->setFirst(replacement);
    }
    op->properties().tex.move_sup_sub = limits;
    auto *core = op->coreMO();
    core->attributes().set(u"movablelimits", false);
    core->properties().tex.movable_limits = false;
    auto *attribute = op->kind() == NodeKind::mo ? NodeUtil::getMoAttribute(*op, u"movableLimits")
                                                 : op->attributes().lookup(u"movablelimits");
    if (attributeTrue(attribute) || op->properties().tex.movable_limits.value_or(false)) {
        op->properties().tex.movable_limits = false;
        if (op->kind() == NodeKind::mo || op->kind() == NodeKind::mstyle)
            op->attributes().set(u"movablelimits", false);
    }
    return NoParseResult{};
}
DispatchOutcome BaseMethods::Over(TexParser &p, MacroInvocation const &i) {
    auto args = arguments(i);
    auto open = text(args, 0), close = text(args, 1);
    auto item = p.itemFactory().create(StackKind::over);
    auto &over = dynamic_cast<OverItem &>(*item);
    over.name = p.currentCS();
    if (!open.empty() || !close.empty()) {
        over.leftDelimiter = open;
        over.rightDelimiter = close;
    } else if (i.dispatch_token.ends_with(u"withdelims")) {
        over.leftDelimiter = p.GetDelimiter(i.dispatch_token);
        over.rightDelimiter = p.GetDelimiter(i.dispatch_token);
    }
    if (i.dispatch_token.starts_with(u"\\above"))
        over.thickness = p.GetDimen(i.dispatch_token);
    else if (i.dispatch_token.starts_with(u"\\atop") || !open.empty() || !close.empty())
        over.thickness = 0.0;
    return push(p, std::move(item));
}
DispatchOutcome BaseMethods::Frac(TexParser &p, MacroInvocation const &i) {
    auto *num = p.ParseArg(i.dispatch_token);
    auto *den = p.ParseArg(i.dispatch_token);
    return push(p, node(p, NodeKind::mfrac, {num, den}));
}
DispatchOutcome BaseMethods::Sqrt(TexParser &p, MacroInvocation const &i) {
    auto index = p.GetBrackets(i.dispatch_token);
    auto source = required(p, i.dispatch_token);
    if (source == u"\\frac") {
        auto num = required(p, source);
        auto den = required(p, source);
        source += u"{" + num + u"}{" + den + u"}";
    }
    TexParser parser(source, ParserEnvironmentSeed{p.stack.env()}, p.configuration);
    auto *arg = parser.mml();
    return push(p, index && !index->empty() ? node(p, NodeKind::mroot, {arg, parseRoot(p, *index)})
                                            : node(p, NodeKind::msqrt, {arg}));
}
DispatchOutcome BaseMethods::Root(TexParser &p, MacroInvocation const &i) {
    auto index = p.GetUpTo(i.dispatch_token, u"\\of");
    auto *arg = p.ParseArg(i.dispatch_token);
    return push(p, node(p, NodeKind::mroot, {arg, parseRoot(p, index)}));
}
DispatchOutcome BaseMethods::MoveRoot(TexParser &p, MacroInvocation const &i) {
    if (!p.stack.env().in_root.value_or(false))
        error("MisplacedMoveRoot", u"%1 can appear only within a root", {p.currentCS()});
    auto id = text(arguments(i), 0);
    auto &g = p.stack.global();
    auto &value = id == u"leftRoot" ? g.left_root : g.up_root;
    if (value && !value->empty())
        error("MultipleMoveRoot", u"Multiple use of %1", {p.currentCS()});
    auto n = required(p, i.dispatch_token);
    if (!match(u"-?[0-9]+", n))
        error("IntegerArg", u"The argument to %1 must be an integer", {p.currentCS()});
    auto digits = match(u"^\\s*([+-]?[0-9]+)", n);
    auto numeric =
        digits ? number(digits->at(1).value()) : std::numeric_limits<double>::quiet_NaN();
    auto result = numberString(numeric / 15) + u"em";
    if (!result.starts_with(u"-"))
        result = u"+" + result;
    value = std::move(result);
    return NoParseResult{};
}
DispatchOutcome BaseMethods::Accent(TexParser &p, MacroInvocation const &i) {
    auto args = arguments(i);
    auto *base = p.ParseArg(i.dispatch_token);
    auto def = parse_util::getFontDef(p);
    def.attributes.set(u"accent", true);
    def.properties.tex.math_accent = present(args, 1) ? flag(args, 1) : true;
    auto &mo = p.createToken(NodeKind::mo, entity(text(args, 0)), std::move(def));
    mo.attributes().set(u"stretchy", flag(args, 1));
    auto *core = base->isEmbellished() ? base->coreMO() : base;
    if (core->kind() == NodeKind::mo || core->properties().tex.movable_limits.value_or(false)) {
        core->properties().tex.movable_limits = false;
        if (core->kind() == NodeKind::mo || core->kind() == NodeKind::mstyle)
            core->attributes().set(u"movablelimits", false);
    }
    auto &scripts = node(p, NodeKind::munderover);
    NodeUtil::setChild(scripts, 0, base);
    NodeUtil::setChild(scripts, 1, nullptr);
    NodeUtil::setChild(scripts, 2, mo);
    return push(p, node(p, NodeKind::tex_atom, {&scripts}));
}
DispatchOutcome BaseMethods::UnderOver(TexParser &p, MacroInvocation const &i) {
    auto args = arguments(i);
    auto character = entity(text(args, 0));
    auto &mo = p.createToken(NodeKind::mo, character,
                             definition({{u"stretchy", true}, {u"accent", true}}));
    if (dynamic_cast<mml::MmlMo &>(mo).isMathAccentWithWidth(character))
        mo.properties().tex.math_accent = false;
    auto *base = p.ParseArg(i.dispatch_token);
    auto pos = i.dispatch_token.size() > 1 && i.dispatch_token[1] == u'o' ? u"over" : u"under";
    return push(p, parse_util::underOver(p, *base, mo, pos, flag(args, 1)));
}
DispatchOutcome BaseMethods::Overset(TexParser &p, MacroInvocation const &i) {
    auto *top = p.ParseArg(i.dispatch_token);
    auto *base = p.ParseArg(i.dispatch_token);
    auto isAccent = accent(*top);
    parse_util::checkMovableLimits(*base);
    return push(p, node(p, NodeKind::mover, {base, top}, definition({{u"accent", isAccent}})));
}
DispatchOutcome BaseMethods::Underset(TexParser &p, MacroInvocation const &i) {
    auto *bottom = p.ParseArg(i.dispatch_token);
    auto *base = p.ParseArg(i.dispatch_token);
    auto isAccent = accent(*bottom);
    parse_util::checkMovableLimits(*base);
    return push(
        p, node(p, NodeKind::munder, {base, bottom}, definition({{u"accentunder", isAccent}})));
}
DispatchOutcome BaseMethods::Overunderset(TexParser &p, MacroInvocation const &i) {
    auto *top = p.ParseArg(i.dispatch_token);
    auto *bottom = p.ParseArg(i.dispatch_token);
    auto *base = p.ParseArg(i.dispatch_token);
    auto over = accent(*top), under = accent(*bottom);
    parse_util::checkMovableLimits(*base);
    return push(p, node(p, NodeKind::munderover, {base, bottom, top},
                        definition({{u"accent", over}, {u"accentunder", under}})));
}
DispatchOutcome BaseMethods::TeXAtom(TexParser &p, MacroInvocation const &i) {
    auto mclass = static_cast<TexClass>(static_cast<int>(number(arguments(i), 0)));
    auto def = texClass(mclass);
    if (mclass == TexClass::op) {
        def.properties.tex.move_sup_sub = true;
        def.properties.tex.movable_limits = true;
        auto source = required(p, i.dispatch_token);
        auto rm = match(u"^\\s*\\\\rm\\s+([a-zA-Z0-9 ]+)$", source);
        MmlNode *op{};
        if (rm) {
            def.attributes.set(u"mathvariant", u"normal");
            op = &p.createToken(NodeKind::mi, rm->at(1).value(), std::move(def));
        } else {
            TexParser parser(source, ParserEnvironmentSeed{p.stack.env()}, p.configuration);
            op = &node(p, NodeKind::tex_atom, {parser.mml()}, std::move(def));
        }
        return push(p, p.itemFactory().create(StackKind::fn, FnItemInit{*op}));
    }
    return push(p, node(p, NodeKind::tex_atom, {p.ParseArg(i.dispatch_token)}, std::move(def)));
}
DispatchOutcome BaseMethods::VBox(TexParser &p, MacroInvocation const &i) {
    auto align = text(arguments(i), 0);
    TexParser parser(required(p, i.dispatch_token), ParserEnvironmentSeed{p.stack.env()},
                     p.configuration);
    auto def = texClass(TexClass::ord);
    def.attributes.set(u"data-vertical-align", align);
    if (auto &width = parser.stack.env().horizontal_size; width && !width->empty()) {
        def.attributes.set(u"width", *width);
        def.attributes.set(u"data-overflow", u"linebreak");
    }
    auto &box = node(p, NodeKind::mpadded, {parser.mml()}, std::move(def));
    box.properties().tex.vertical_box = align;
    return push(p, box);
}
DispatchOutcome BaseMethods::Hsize(TexParser &p, MacroInvocation const &i) {
    if (p.GetNext() == u"=")
        p.setPosition(p.position() + 1);
    p.stack.env().horizontal_size = p.GetDimen(i.dispatch_token);
    return push(p, p.itemFactory().create(StackKind::null_item));
}
DispatchOutcome BaseMethods::ParBox(TexParser &p, MacroInvocation const &i) {
    auto c = p.GetBrackets(i.dispatch_token, u"c").value();
    auto width = p.GetDimen(i.dispatch_token);
    auto children = parse_util::internalMath(p, required(p, i.dispatch_token));
    auto align = splitAlignArray(c, 1);
    auto &box = p.create(NodeKind::mpadded, children,
                         definition({{u"width", width},
                                     {u"data-overflow", u"linebreak"s},
                                     {u"data-vertical-align", align}}));
    box.properties().tex.vertical_box = align;
    return push(p, box);
}
DispatchOutcome BaseMethods::BreakAlign(TexParser &p, MacroInvocation const &i) {
    auto *top = dynamic_cast<ArrayItem *>(p.stack.Top());
    if (!top)
        error("BreakNotInArray", u"%1 must be used in an alignment environment", {p.currentCS()});
    auto type = trim(required(p, i.dispatch_token));
    if (type == u"c") {
        if (top->First())
            error("BreakFirstInEntry", u"%1 must be at the beginning of an alignment entry",
                  {p.currentCS() + u"{c}"});
        top->breakAlign.cell = splitAlignArray(required(p, i.dispatch_token), 1);
    } else if (type == u"r") {
        if (!top->row.empty() || top->First())
            error("BreakFirstInRow", u"%1 must be at the beginning of an alignment row",
                  {p.currentCS() + u"{r}"});
        top->breakAlign.row = splitAlignArray(required(p, i.dispatch_token));
    } else if (type == u"t") {
        if (!top->table.empty() || !top->row.empty() || top->First())
            error("BreakFirstInTable", u"%1 must be at the beginning of an alignment",
                  {p.currentCS() + u"{t}"});
        top->breakAlign.table = splitAlignArray(required(p, i.dispatch_token));
    } else
        error("BreakType", u"First argument to %1 must be one of c, r, or t", {p.currentCS()});
    return NoParseResult{};
}
DispatchOutcome BaseMethods::MmlToken(TexParser &p, MacroInvocation const &i) {
    auto kindText = required(p, i.dispatch_token);
    auto attr = p.GetBrackets(i.dispatch_token, u"").value();
    if (auto whitespace = match(u"^\\s+", attr))
        attr.erase(0, whitespace->at(0)->size());
    auto contents = required(p, i.dispatch_token);
    auto kind = mml::nodeKindFromName(std::string(kindText.begin(), kindText.end()));
    MmlNode *token{};
    try {
        if (kind)
            token = &node(p, *kind);
    } catch (std::exception const &) {
    }
    if (!token || !token->isToken())
        error("NotMathMLToken", u"%1 is not a token element", {kindText});
    constexpr std::array allowed{u"fontfamily"sv, u"fontsize"sv,   u"fontweight"sv, u"fontstyle"sv,
                                 u"color"sv,      u"background"sv, u"id"sv,         u"class"sv,
                                 u"href"sv,       u"style"sv};
    NodeDefinition def;
    std::u16string keep;
    while (!attr.empty()) {
        auto m =
            regexp_host::RegExp(
                u"^([a-z]+)\\s*=\\s*('[^'\\n]*'|\"[^\"\\n]*\"|[^ ,\\n]*)[\\s\\n]*,?[\\s\\n]*", u"i")
                .exec(attr);
        if (!m) {
            auto end = attr.find_first_of(u" \t\r\n\v\f=");
            error("InvalidMathMLAttr", u"Invalid MathML attribute: %1", {attr.substr(0, end)});
        }
        auto name = m->at(1).value();
        auto value = m->at(2).value();
        if (!token->attributes().hasDefault(name) &&
            std::find(allowed.begin(), allowed.end(), name) == allowed.end())
            error("UnknownAttrForElement", u"%1 is not a recognized attribute for %2",
                  {name, kindText});
        if (value.size() >= 2 && (value.front() == u'\'' || value.front() == u'"') &&
            value.back() == value.front())
            value = value.substr(1, value.size() - 2);
        value = parse_util::mmlFilterAttribute(p, name, value);
        if (!value.empty()) {
            auto boolean = lower(value);
            if (boolean == u"true" || boolean == u"false")
                def.attributes.set(name, boolean == u"true");
            else
                def.attributes.set(name, value);
            if (!keep.empty())
                keep += u' ';
            keep += name;
        }
        attr.erase(0, m->at(0)->size());
    }
    if (!keep.empty())
        token->properties().tex.keep_attributes = keep;
    token->appendChild(p.createText(string_util::replaceUnicode(contents)));
    NodeUtil::setProperties(*token, std::move(def));
    return push(p, *token);
}
DispatchOutcome BaseMethods::Strut(TexParser &p, MacroInvocation const &) {
    auto &row = node(p, NodeKind::mrow);
    return push(p,
                node(p, NodeKind::mpadded, {&row},
                     definition({{u"height", u"8.6pt"s}, {u"depth", u"3pt"s}, {u"width", 0.0}})));
}
DispatchOutcome BaseMethods::Phantom(TexParser &p, MacroInvocation const &i) {
    auto args = arguments(i);
    auto *box = &node(p, NodeKind::mphantom, {p.ParseArg(i.dispatch_token)});
    if (flag(args, 0) || flag(args, 1)) {
        box = &node(p, NodeKind::mpadded, {box});
        if (flag(args, 1)) {
            box->attributes().set(u"height", 0.0);
            box->attributes().set(u"depth", 0.0);
        }
        if (flag(args, 0))
            box->attributes().set(u"width", 0.0);
    }
    return push(p, node(p, NodeKind::tex_atom, {box}));
}
DispatchOutcome BaseMethods::Smash(TexParser &p, MacroInvocation const &i) {
    auto bt = trim(p.GetBrackets(i.dispatch_token, u"").value());
    auto &box = node(p, NodeKind::mpadded, {p.ParseArg(i.dispatch_token)});
    if (bt != u"b")
        box.attributes().set(u"height", 0.0);
    if (bt != u"t")
        box.attributes().set(u"depth", 0.0);
    return push(p, node(p, NodeKind::tex_atom, {&box}));
}
DispatchOutcome BaseMethods::Lap(TexParser &p, MacroInvocation const &i) {
    auto &box =
        node(p, NodeKind::mpadded, {p.ParseArg(i.dispatch_token)}, definition({{u"width", 0.0}}));
    if (i.dispatch_token == u"\\llap")
        box.attributes().set(u"lspace", u"-1width");
    return push(p, node(p, NodeKind::tex_atom, {&box}));
}
DispatchOutcome BaseMethods::RaiseLower(TexParser &p, MacroInvocation const &i) {
    auto h = p.GetDimen(i.dispatch_token);
    auto item = p.itemFactory().create(StackKind::position);
    auto &position = dynamic_cast<PositionItem &>(*item);
    position.name = p.currentCS();
    position.move = PositionMove::vertical;
    std::u16string name(i.dispatch_token);
    if (h.starts_with(u"-")) {
        h.erase(0, 1);
        name = name.substr(1) == u"raise" ? u"\\lower" : u"\\raise";
    }
    position.dh = (name == u"\\lower" ? u"-" : u"+") + h;
    position.dd = (name == u"\\lower" ? u"+" : u"-") + h;
    return push(p, std::move(item));
}
DispatchOutcome BaseMethods::MoveLeftRight(TexParser &p, MacroInvocation const &i) {
    auto h = p.GetDimen(i.dispatch_token);
    auto nh = h.starts_with(u"-") ? h.substr(1) : u"-" + h;
    if (i.dispatch_token == u"\\moveleft")
        std::swap(h, nh);
    auto item = p.itemFactory().create(StackKind::position);
    auto &position = dynamic_cast<PositionItem &>(*item);
    position.name = p.currentCS();
    position.move = PositionMove::horizontal;
    position.left = &node(p, NodeKind::mspace, {}, definition({{u"width", h}}));
    position.right = &node(p, NodeKind::mspace, {}, definition({{u"width", nh}}));
    return push(p, std::move(item));
}
DispatchOutcome BaseMethods::Hskip(TexParser &p, MacroInvocation const &i) {
    auto &space =
        node(p, NodeKind::mspace, {}, definition({{u"width", p.GetDimen(i.dispatch_token)}}));
    if (flag(arguments(i), 0))
        space.attributes().set(u"linebreak", u"nobreak");
    return push(p, space);
}
DispatchOutcome BaseMethods::Nonscript(TexParser &p, MacroInvocation const &) {
    return push(p, p.itemFactory().create(StackKind::nonscript));
}
DispatchOutcome BaseMethods::Rule(TexParser &p, MacroInvocation const &i) {
    auto w = p.GetDimen(i.dispatch_token);
    auto h = p.GetDimen(i.dispatch_token);
    auto d = p.GetDimen(i.dispatch_token);
    auto def = definition({{u"width", w}, {u"height", h}, {u"depth", d}});
    auto color = p.stack.env().color.value_or(u"");
    if (color.empty())
        color = u"black";
    if (text(arguments(i), 0) != u"blank")
        def.attributes.set(u"mathbackground", color);
    return push(p, node(p, NodeKind::mspace, {}, std::move(def)));
}
DispatchOutcome BaseMethods::rule(TexParser &p, MacroInvocation const &i) {
    auto v = p.GetBrackets(i.dispatch_token);
    auto w = p.GetDimen(i.dispatch_token);
    auto h = p.GetDimen(i.dispatch_token);
    auto color = p.stack.env().color.value_or(u"");
    if (color.empty())
        color = u"black";
    auto *box = &node(p, NodeKind::mspace, {},
                      definition({{u"width", w}, {u"height", h}, {u"mathbackground", color}}));
    if (v && !v->empty()) {
        box = &node(p, NodeKind::mpadded, {box}, definition({{u"voffset", *v}}));
        if (v->starts_with(u"-")) {
            box->attributes().set(u"height", *v);
            box->attributes().set(u"depth", u"+" + v->substr(1));
        } else
            box->attributes().set(u"height", u"+" + *v);
    }
    return push(p, *box);
}
DispatchOutcome BaseMethods::MakeBig(TexParser &p, MacroInvocation const &i) {
    auto args = arguments(i);
    auto size = numberString(number(args, 1) * (1.2 / 0.85));
    if (auto dot = size.find(u'.'); dot != size.npos && size.size() > dot + 4)
        size.resize(dot + 4);
    size += u"em";
    auto delim = p.GetDelimiter(i.dispatch_token, true).value();
    auto &mo = p.createToken(NodeKind::mo, delim,
                             definition({{u"minsize", size},
                                         {u"maxsize", size},
                                         {u"fence", true},
                                         {u"stretchy", true},
                                         {u"symmetric", true}}));
    return push(p, node(p, NodeKind::tex_atom, {&mo},
                        texClass(static_cast<TexClass>(static_cast<int>(number(args, 0))))));
}
DispatchOutcome BaseMethods::BuildRel(TexParser &p, MacroInvocation const &i) {
    auto *top = p.ParseUpTo(i.dispatch_token, u"\\over");
    auto *base = p.ParseArg(i.dispatch_token);
    auto &scripts = node(p, NodeKind::munderover);
    NodeUtil::setChild(scripts, 0, base);
    NodeUtil::setChild(scripts, 1, nullptr);
    NodeUtil::setChild(scripts, 2, top);
    return push(p, node(p, NodeKind::tex_atom, {&scripts}, texClass(TexClass::rel)));
}
DispatchOutcome BaseMethods::HBox(TexParser &p, MacroInvocation const &i) {
    auto args = arguments(i);
    std::optional<ScriptLevel> level;
    if (present(args, 0)) {
        if (auto *n = std::get_if<double>(argument(args, 0)))
            level = *n;
        else
            level = text(args, 0);
    }
    auto font = text(args, 1);
    auto result = parse_util::internalMath(
        p, required(p, i.dispatch_token), level,
        present(args, 1) ? std::optional<std::u16string_view>(font) : std::nullopt);
    p.PushAll(result);
    return NoParseResult{};
}
DispatchOutcome BaseMethods::FBox(TexParser &p, MacroInvocation const &i) {
    auto children = parse_util::internalMath(p, required(p, i.dispatch_token));
    return push(p, p.create(NodeKind::menclose, children, definition({{u"notation", u"box"s}})));
}
DispatchOutcome BaseMethods::FrameBox(TexParser &p, MacroInvocation const &i) {
    auto width = p.GetBrackets(i.dispatch_token);
    auto pos = p.GetBrackets(i.dispatch_token).value_or(u"c");
    auto children = parse_util::internalMath(p, required(p, i.dispatch_token));
    if (width && !width->empty()) {
        auto &box = p.create(NodeKind::mpadded, children,
                             definition({{u"width", *width},
                                         {u"data-align", pos == u"l"   ? u"left"s
                                                         : pos == u"r" ? u"right"s
                                                                       : u"center"s}}));
        children = {&box};
    }
    auto &enclose = p.create(NodeKind::menclose, children, definition({{u"notation", u"box"s}}));
    return push(p, node(p, NodeKind::tex_atom, {&enclose}, texClass(TexClass::ord)));
}
DispatchOutcome BaseMethods::MakeBox(TexParser &p, MacroInvocation const &i) {
    auto width = p.GetBrackets(i.dispatch_token);
    auto pos = p.GetBrackets(i.dispatch_token, u"c").value();
    auto children = parse_util::internalMath(p, required(p, i.dispatch_token));
    auto &box = p.create(NodeKind::mpadded, children);
    if (width && !width->empty())
        box.attributes().set(u"width", *width);
    auto align = lower(pos);
    if (align == u"c" || align == u"r")
        box.attributes().set(u"data-align", align == u"c" ? u"center" : u"right");
    if (align != pos)
        box.attributes().set(u"data-overflow", u"linebreak");
    return push(p, box);
}
DispatchOutcome BaseMethods::Not(TexParser &p, MacroInvocation const &) {
    return push(p, p.itemFactory().create(StackKind::not_item));
}
DispatchOutcome BaseMethods::Dots(TexParser &p, MacroInvocation const &) {
    auto &ldots = p.createToken(NodeKind::mo, u"\u2026", definition({{u"stretchy", false}}));
    auto &cdots = p.createToken(NodeKind::mo, u"\u22ef", definition({{u"stretchy", false}}));
    auto item = p.itemFactory().create(StackKind::dots);
    auto &dots = dynamic_cast<DotsItem &>(*item);
    dots.ldots = &ldots;
    dots.cdots = &cdots;
    return push(p, std::move(item));
}
DispatchOutcome BaseMethods::Matrix(TexParser &p, MacroInvocation const &i) {
    auto args = arguments(i);
    auto open = text(args, 0), close = text(args, 1), align = text(args, 2);
    auto c = p.GetNext();
    if (c.empty())
        error("MissingArgFor", u"Missing argument for %1", {p.currentCS()});
    if (c == u"{")
        p.setPosition(p.position() + 1);
    else {
        p.setString(
            c + u"}" +
            std::u16string(p.string().substr(std::min(p.position() + 1, p.string().size()))));
        p.setPosition(0);
    }
    auto item = p.itemFactory().create(StackKind::array);
    auto &array = dynamic_cast<ArrayItem &>(*item);
    array.requireClose = true;
    if (!open.empty() || align.empty())
        array.arrayPadding = u".2em .125em";
    auto spacing = text(args, 3), vspacing = text(args, 4);
    array.arraydef.set(u"rowspacing", vspacing.empty() ? u"4pt" : vspacing);
    array.arraydef.set(u"columnspacing", spacing.empty() ? u"1em" : spacing);
    if (flag(args, 6))
        array.isCases = true;
    if (flag(args, 7)) {
        array.isNumbered = true;
        if (auto *value = argument(args, 7);
            value && std::holds_alternative<std::u16string>(*value))
            array.arraydef.set(u"side", std::get<std::u16string>(*value));
        else if (auto *value = argument(args, 7); value && std::holds_alternative<double>(*value))
            array.arraydef.set(u"side", std::get<double>(*value));
        else
            array.arraydef.set(u"side", flag(args, 7));
    }
    if (!open.empty() || !close.empty()) {
        array.open = open;
        array.close = close;
    }
    if (text(args, 5) == u"D")
        array.arraydef.set(u"displaystyle", true);
    if (present(args, 2))
        array.arraydef.set(u"columnalign", align);
    return push(p, std::move(item));
}
DispatchOutcome BaseMethods::Entry(TexParser &p, MacroInvocation const &i) {
    auto item = p.itemFactory().create(StackKind::cell);
    auto &cell = dynamic_cast<CellItem &>(*item);
    cell.isEntry = true;
    cell.name = i.dispatch_token;
    p.Push(std::move(item));
    auto *top = dynamic_cast<ArrayItem *>(p.stack.Top());
    if (!top || (!top->isCases && !top->casesEnv))
        return NoParseResult{};
    auto source = std::u16string(p.string());
    std::size_t braces{}, at = p.position(), limit = source.size();
    std::optional<std::size_t> close;
    std::optional<std::u16string> end;
    if (top->casesEnv) {
        auto env = *top->casesEnv;
        if (auto star = env.find(u'*'); star != env.npos)
            env.insert(star, 1, u'\\');
        end = u"^\\\\end\\s*\\{" + env + u"\\}";
    }
    while (at < limit) {
        auto c = source[at];
        if (c == u'{') {
            ++braces;
            ++at;
        } else if (c == u'}') {
            if (braces == 0)
                limit = 0;
            else {
                --braces;
                if (braces == 0 && !close)
                    close = at - p.position();
                ++at;
            }
        } else if (c == u'&' && braces == 0)
            error("ExtraAlignTab", u"Extra alignment tab in \\cases text");
        else if (c == u'\\') {
            auto rest = std::u16string_view(source).substr(at);
            if (match(u"^((\\\\cr)[^a-zA-Z]|\\\\\\\\)", rest) || (end && match(*end, rest)))
                limit = 0;
            else
                at += 2;
        } else
            ++at;
    }
    auto contents = source.substr(p.position(), at - p.position());
    auto rightTrimmed = contents;
    if (auto whitespace = match(u"\\s+$", rightTrimmed))
        rightTrimmed.resize(whitespace->index);
    if (!match(u"^\\s*\\\\text[^a-zA-Z]", contents) || !close ||
        *close != rightTrimmed.size() - 1) {
        auto internal = parse_util::internalMath(p, trim(contents), ScriptLevel{0.0});
        p.PushAll(internal);
        p.setPosition(at);
    }
    return NoParseResult{};
}
DispatchOutcome BaseMethods::Cr(TexParser &p, MacroInvocation const &i) {
    auto item = p.itemFactory().create(StackKind::cell);
    auto &cell = dynamic_cast<CellItem &>(*item);
    cell.isCR = true;
    cell.name = i.dispatch_token;
    return push(p, std::move(item));
}
DispatchOutcome BaseMethods::CrLaTeX(TexParser &p, MacroInvocation const &i) {
    std::u16string n;
    if (!flag(arguments(i), 0)) {
        if (p.position() < p.string().size() && p.string()[p.position()] == u'*')
            p.setPosition(p.position() + 1);
        if (p.position() < p.string().size() && p.string()[p.position()] == u'[') {
            auto dim = p.GetBrackets(i.dispatch_token, u"").value();
            auto matched = UnitUtil::instance().matchDimen(dim);
            if (!dim.empty() && !matched)
                error("BracketMustBeDimension", u"Bracket argument to %1 must be a dimension",
                      {p.currentCS()});
            if (matched)
                n = matched->value + matched->unit;
        }
    }
    auto item = p.itemFactory().create(StackKind::cell);
    auto &cell = dynamic_cast<CellItem &>(*item);
    cell.isCR = true;
    cell.name = i.dispatch_token;
    cell.linebreak = true;
    p.Push(std::move(item));
    if (auto *top = dynamic_cast<ArrayItem *>(p.stack.Top())) {
        if (!n.empty())
            top->addRowSpacing(n);
    } else {
        auto &space = node(p, NodeKind::mspace, {}, definition({{u"linebreak", u"newline"s}}));
        if (!n.empty())
            space.attributes().set(u"data-lineleading", n);
        p.Push(&space);
    }
    return NoParseResult{};
}
DispatchOutcome BaseMethods::HLine(TexParser &p, MacroInvocation const &i) {
    auto args = arguments(i);
    auto style = present(args, 0) ? text(args, 0) : u"solid"s;
    auto *top = dynamic_cast<ArrayItem *>(p.stack.Top());
    if (!top || top->Size())
        error("Misplaced", u"Misplaced %1", {p.currentCS()});
    if (top->table.empty())
        top->frame.push_back({ArrayFrameRule::Edge::top, style});
    else {
        std::vector<std::u16string> lines;
        if (auto *value = top->arraydef.own(u"rowlines");
            value && std::holds_alternative<std::u16string>(*value)) {
            auto const &text = std::get<std::u16string>(*value);
            if (!text.empty())
                for (auto const &part : regexp_host::split(text, u" ", u""))
                    lines.push_back(part.value_or(u""));
        }
        while (lines.size() < top->table.size())
            lines.emplace_back(u"none");
        lines[top->table.size() - 1] = style;
        std::u16string joined;
        for (std::size_t n = 0; n < lines.size(); ++n) {
            if (n)
                joined += u' ';
            joined += lines[n];
        }
        top->arraydef.set(u"rowlines", joined);
    }
    return NoParseResult{};
}
DispatchOutcome BaseMethods::HFill(TexParser &p, MacroInvocation const &) {
    auto *top = dynamic_cast<ArrayItem *>(p.stack.Top());
    if (!top)
        error("UnsupportedHFill", u"Unsupported use of %1", {p.currentCS()});
    top->hfill.push_back(top->Size());
    return NoParseResult{};
}
DispatchOutcome BaseMethods::NewColumnType(TexParser &p, MacroInvocation const &i) {
    auto c = required(p, i.dispatch_token);
    auto n = p.GetBrackets(i.dispatch_token, u"0").value();
    auto macro = required(p, i.dispatch_token);
    if (c.size() != 1)
        error("BadColumnName", u"Column specifier must be exactly one character: %1", {c});
    if (!match(u"^\\d+$", n))
        error("PositiveIntegerArg", u"Argument to %1 must be a positive integer", {n});
    p.configuration.packageData.columnDefinitions().set(c[0],
                                                        MacroColumnDefinition{macro, number(n)});
    return push(p, p.itemFactory().create(StackKind::null_item));
}
DispatchOutcome BaseMethods::BeginEnd(TexParser &p, MacroInvocation const &i) {
    auto env = required(p, i.dispatch_token);
    if (env.find(u'\\') != env.npos)
        error("InvalidEnv", u"Invalid environment name '%1'", {env});
    auto mapped = p.configuration.handlers.get(HandlerKind::environment)->lookup(env);
    auto *macro = std::get_if<tex::Macro const *>(&mapped);
    auto *definition = std::get_if<EnvironmentDefinition const *>(&mapped);
    const bool sessionEnvironment = definition && *definition;
    if (((macro && *macro) || sessionEnvironment) && i.dispatch_token == u"\\end") {
        if (!sessionEnvironment && !flag((*macro)->arguments, 0)) {
            auto item = p.itemFactory().create(StackKind::end);
            dynamic_cast<EndItem &>(*item).name = env;
            return push(p, std::move(item));
        }
        p.stack.env().closing_environment = env;
    }
    parse_util::checkMaxMacros(p, false);
    p.parse(HandlerKind::environment, ParseInput{p, env});
    return NoParseResult{};
}
StackEntry BaseMethods::Array(TexParser &p, FrameHandle const &begin, Arguments args) {
    auto open = text(args, 0), close = text(args, 1), align = text(args, 2),
         spacing = text(args, 3), vspacing = text(args, 4), style = text(args, 5);
    auto name = beginName(begin);
    if (align.empty())
        align = required(p, u"\\begin{" + name + u"}");
    auto item = p.itemFactory().create(StackKind::array);
    auto &array = dynamic_cast<ArrayItem &>(*item);
    if (name == u"array")
        array.arrayPadding = u".5em .125em";
    array.parser = &p;
    array.arraydef.set(u"columnspacing", spacing.empty() ? u"1em" : spacing);
    array.arraydef.set(u"rowspacing", vspacing.empty() ? u"4pt" : vspacing);
    p.configuration.columnParser->process(p, align, array);
    if (!open.empty())
        array.open = p.convertDelimiter(open);
    if (!close.empty())
        array.close = p.convertDelimiter(close);
    if (style.size() > 1 && style[1] == u'\'') {
        array.arraydef.set(u"data-cramped", true);
        style.resize(1);
    }
    if (style == u"D")
        array.arraydef.set(u"displaystyle", true);
    else if (!style.empty())
        array.arraydef.set(u"displaystyle", false);
    array.arraydef.set(u"scriptlevel", style == u"S" ? 1.0 : 0.0);
    if (flag(args, 6))
        array.arraydef.set(u"useHeight", false);
    p.Push(begin);
    array.StartEntry();
    return item;
}
StackEntry BaseMethods::AlignedArray(TexParser &p, FrameHandle const &begin, Arguments args) {
    auto align = p.GetBrackets(u"\\begin{" + beginName(begin) + u"}");
    const std::array<TexArgument, 6> arrayArgs{NullArgument{}, NullArgument{}, NullArgument{},
                                               NullArgument{}, NullArgument{}, text(args, 0)};
    auto result = Array(p, begin, arrayArgs);
    auto &array = dynamic_cast<ArrayItem &>(*std::get<FrameHandle>(result));
    parse_util::setArrayAlign(array,
                              align ? std::optional<std::u16string_view>(*align) : std::nullopt);
    return result;
}
StackEntry BaseMethods::IndentAlign(TexParser &p, FrameHandle const &begin, Arguments) {
    auto name = u"\\begin{" + beginName(begin) + u"}";
    auto first = p.GetBrackets(name, u"").value(), shift = p.GetBrackets(name, u"").value(),
         last = p.GetBrackets(name, u"").value();
    if ((!first.empty() && !UnitUtil::instance().matchDimen(first)) ||
        (!shift.empty() && !UnitUtil::instance().matchDimen(shift)) ||
        (!last.empty() && !UnitUtil::instance().matchDimen(last)))
        error("BracketMustBeDimension", u"Bracket argument to %1 must be a dimension", {name});
    auto lcr = required(p, name);
    if (!lcr.empty() && !match(u"^([lcr]{1,3})?$", lcr))
        error("BadAlignment", u"Alignment must be one to three copies of l, c, or r");
    std::vector<std::u16string> align;
    for (auto c : lcr)
        align.push_back(c == u'l' ? u"left" : c == u'r' ? u"right" : u"center");
    if (align.size() == 1)
        align.push_back(align[0]);
    mml::AttributeSet attributes;
    if (!first.empty())
        attributes.set(u"indentshiftfirst", first);
    if (shift.empty())
        shift = first;
    if (!shift.empty())
        attributes.set(u"indentshift", shift);
    if (!last.empty())
        attributes.set(u"indentshiftlast", last);
    constexpr std::array keys{u"indentalignfirst", u"indentalign", u"indentalignlast"};
    for (std::size_t n = 0; n < align.size(); ++n)
        attributes.set(keys[n], align[n]);
    p.Push(p.itemFactory().create(StackKind::mstyle,
                                  MstyleItemInit{std::move(attributes), beginName(begin)}));
    return std::monostate{};
}
StackEntry BaseMethods::Equation(TexParser &p, FrameHandle const &begin, Arguments args) {
    auto display = present(args, 1) ? flag(args, 1) : true;
    p.configuration.request().mathItem->setDisplay(display);
    p.stack.env().display = display;
    parse_util::checkEqnEnv(p);
    p.Push(begin);
    auto item = p.itemFactory().create(StackKind::equation, EquationItemInit{flag(args, 0)});
    dynamic_cast<EquationItem &>(*item).name = beginName(begin);
    return item;
}
StackEntry BaseMethods::EqnArray(TexParser &p, FrameHandle const &begin, Arguments args) {
    auto name = beginName(begin);
    bool gather = name == u"gather" || name == u"gather*";
    if (flag(args, 1))
        parse_util::checkEqnEnv(p, !gather);
    p.Push(begin);
    auto align = text(args, 2);
    std::u16string calign;
    for (auto c : align)
        if (c == u'c' || c == u'l' || c == u'r') {
            if (!calign.empty())
                calign += u' ';
            calign += c == u'c' ? u"center" : c == u'l' ? u"left" : u"right";
        }
    auto balign = splitAlignArray(text(args, 3));
    auto item = p.itemFactory().create(StackKind::eqnarray,
                                       EqnArrayItemInit{name, flag(args, 0), flag(args, 1)});
    auto &array = dynamic_cast<ArrayItem &>(*item);
    auto spacing = text(args, 4);
    array.arraydef.set(u"displaystyle", true);
    array.arraydef.set(u"columnalign", calign);
    array.arraydef.set(u"columnspacing", spacing.empty() ? u"1em" : spacing);
    array.arraydef.set(u"rowspacing", u"3pt");
    array.arraydef.set(u"data-break-align", balign);
    if (p.options().tagSide)
        array.arraydef.set(u"side", *p.options().tagSide);
    if (p.options().tagIndent)
        array.arraydef.set(u"minlabelspacing", *p.options().tagIndent);
    if (gather)
        array.nestable = true;
    return item;
}
DispatchOutcome BaseMethods::HandleNoTag(TexParser &p, MacroInvocation const &) {
    p.tags().notag();
    return NoParseResult{};
}
DispatchOutcome BaseMethods::HandleLabel(TexParser &p, MacroInvocation const &i) {
    auto label = required(p, i.dispatch_token);
    if (label.empty())
        return NoParseResult{};
    if (!p.tags().label().empty())
        error("MultipleCommand", u"Multiple %1", {p.currentCS()});
    p.tags().setLabel(label);
    auto &equation = p.configuration.request().equation;
    if (!equation.ref_update) {
        if ((p.configuration.packageData.findLabel(label) || equation.labels.contains(label)) &&
            !p.options().ignoreDuplicateLabels.value_or(false))
            error("MultipleLabel", u"Label '%1' multiply defined", {label});
        equation.labels.insert_or_assign(label, LabelRecord{u"???", u""});
    }
    return NoParseResult{};
}
DispatchOutcome BaseMethods::HandleRef(TexParser &p, MacroInvocation const &i) {
    auto label = required(p, i.dispatch_token);
    auto &equation = p.configuration.request().equation;
    auto *ref = p.configuration.packageData.findLabel(label);
    if (!ref) {
        auto found = equation.labels.find(label);
        if (found != equation.labels.end())
            ref = &found->second;
    }
    LabelRecord unknown{u"???", u""};
    if (!ref) {
        if (!equation.ref_update)
            equation.redo = true;
        ref = &unknown;
    }
    auto tag = ref->tag;
    if (flag(arguments(i), 0)) {
        auto formatted = p.tags().formatRef(tag);
        tag.clear();
        if (auto *s = std::get_if<std::u16string>(&formatted))
            tag = *s;
        else
            for (auto const &part : std::get<std::vector<std::u16string>>(formatted))
                tag += part;
    }
    auto children = parse_util::internalMath(p, tag);
    return push(p, p.create(NodeKind::mrow, children,
                            definition({{u"href", p.tags().formatUrl(
                                                      ref->id, p.options().baseURL.value_or(u""))},
                                        {u"class", u"MathJax_ref"s}})));
}
DispatchOutcome BaseMethods::Macro(TexParser &p, MacroInvocation const &i) {
    std::u16string replacement;
    double count{};
    bool substitute{};
    std::optional<std::u16string> defaultFirst;
    if (auto *args = std::get_if<Arguments>(&i.arguments)) {
        replacement = text(*args, 0);
        count = number(*args, 1);
        substitute = flag(*args, 1);
        if (present(*args, 2))
            defaultFirst = text(*args, 2);
    } else {
        auto const &definition = std::get<MacroDefinition>(
            std::get<std::reference_wrapper<CommandDefinition const>>(i.arguments).get().body);
        replacement = definition.replacement_source;
        count = definition.arity;
        substitute = count != 0 || definition.substitute_parameters;
        defaultFirst = definition.default_first_argument;
    }
    if (substitute) {
        std::vector<std::u16string> values;
        if (defaultFirst) {
            auto optional = p.GetBrackets(i.dispatch_token);
            values.push_back(optional ? *optional : *defaultFirst);
        }
        for (auto n = static_cast<double>(values.size()); n < count; ++n)
            values.push_back(required(p, i.dispatch_token));
        replacement = parse_util::substituteArgs(p, values, replacement);
    }
    p.setString(parse_util::addArgs(p, replacement, p.string().substr(p.position())));
    p.setPosition(0);
    parse_util::checkMaxMacros(p);
    return NoParseResult{};
}
DispatchOutcome BaseMethods::MathChoice(TexParser &p, MacroInvocation const &i) {
    auto *D = p.ParseArg(i.dispatch_token), *T = p.ParseArg(i.dispatch_token),
         *S = p.ParseArg(i.dispatch_token), *SS = p.ParseArg(i.dispatch_token);
    return push(p, node(p, NodeKind::math_choice, {D, T, S, SS}));
}
} // namespace folia::math::tex
