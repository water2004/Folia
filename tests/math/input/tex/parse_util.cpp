#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.parse_util;
import folia.math.input.tex.base_mappings;

using namespace boost::ut;
using namespace folia::math::tex;
using namespace folia::math::tex::parse_util;

namespace {
void helperSetup(Fixture &fixture) {
    BaseMappings::initialize(fixture.maps);
    fixture.effective.numberPattern =
        std::make_shared<folia::math::regexp_host::RegExp>(u"^[0-9]+");
    fixture.effective.identifierPattern =
        std::make_shared<folia::math::regexp_host::RegExp>(u"^[a-zA-Z]+");
    const std::array registrations{
        HandlerRegistration{HandlerKind::character, {u"command", u"special"}},
        HandlerRegistration{HandlerKind::character, {u"letter", u"digit"}, nullptr, 4},
        HandlerRegistration{HandlerKind::macro,
                            {u"delimiter", u"macros", u"lcGreek", u"ucGreek", u"mathchar0mi",
                             u"mathchar0mo", u"mathchar7"}},
        HandlerRegistration{HandlerKind::delimiter, {u"delimiter"}},
        HandlerRegistration{HandlerKind::environment, {u"environment"}}};
    fixture.options->handlers.add(registrations);
}
std::u16string shape(MmlNode const &node) {
    if (node.kind() == NodeKind::text)
        return u"text(" + std::u16string(node.text()) + u")";
    auto name = nodeKindName(node.kind());
    std::u16string result(name.begin(), name.end());
    result += u'(';
    for (std::size_t i = 0; i < node.children().size(); ++i) {
        if (i)
            result += u',';
        result += node.children()[i] ? shape(*node.children()[i]) : u"_";
    }
    return result + u')';
}
thread_local std::vector<std::string> *creationCalls{};
MmlNode *traceText(NodeFactory &factory, std::optional<std::u16string_view> text) {
    creationCalls->push_back("text");
    return NodeFactory::createText(factory, text);
}
MmlNode &traceNode(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                   NodeDefinition definition, MmlNode *text) {
    creationCalls->push_back("node");
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
MmlNode &unexpectedToken(NodeFactory &, NodeKind, NodeDefinition, std::optional<std::u16string_view>) {
    throw std::logic_error("Internal text must use text and node creators");
}
struct CreationCallsScope {
    std::vector<std::string> *previous = creationCalls;
    explicit CreationCallsScope(std::vector<std::string> &calls) {
        creationCalls = &calls;
    }
    ~CreationCallsScope() {
        creationCalls = previous;
    }
};
template <class Operation>
void expectTexError(Operation &&operation, std::string_view expected_id,
                    std::u16string_view expected_message) {
    bool caught = false;
    try {
        operation();
    } catch (TexError const &error) {
        caught = true;
        expect(error.id() == expected_id);
        expect(error.message() == expected_message);
    } catch (...) {
        expect(false) << "wrong exception type";
    }
    expect(caught) << "expected a typed TexError";
}
} // namespace

suite parse_util_tests = [] {
    "ParseUtil internal text and math match original configured parser trees"_test = [] {
        struct Case {
            std::u16string text, expected;
        };
        const std::array cases{
            Case{u"", u""},
            Case{u"  a  b\n\n c  ", u"mtext(text(\u00a0a b  c\u00a0))"},
            Case{u"a $x$ b", u"mrow(mtext(text(a\u00a0)),TeXAtom(inferredMrow(mi(text(x)))),mtext("
                             u"text(\u00a0b)))"},
            Case{u"a \\(x\\) b", u"mrow(mtext(text(a\u00a0)),TeXAtom(inferredMrow(mi(text(x)))),"
                                 u"mtext(text(\u00a0b)))"},
            Case{u"\\$\\{x\\}\\\\", u"mtext(text(${x}\\))"},
            Case{u"\\U{1F600}", u"mtext(text(😀))"},
            Case{u"\\U4A", u"mtext(text(\u0004A))"}};
        Fixture fixture;
        helperSetup(fixture);
        TexParser parser(u"", ParserEnvironmentSeed{TexEnvironment{.font = u"bold"}},
                         *fixture.options);
        for (auto const &item : cases) {
            auto nodes = internalMath(parser, item.text);
            expect(nodes.size() == (item.expected.empty() ? 0u : 1u));
            if (!nodes.empty())
                expect(shape(*nodes[0]) == item.expected);
        }
        auto level = internalMath(parser, u"", ScriptLevel{0.0});
        expect(level.size() == 1_u);
        expect(level[0]->kind() == NodeKind::mstyle);
        expect(!std::get<bool>(*level[0]->attributes().own(u"displaystyle")));
        expect(std::get<double>(*level[0]->attributes().own(u"scriptlevel")) == 0.0);
        texError([&] { internalMath(parser, u"$x"); }, "MathNotTerminated");
        texError([&] { internalMath(parser, u"\\(x"); }, "MathNotTerminated");
        texError([&] { internalMath(parser, u"\\U{xyz}"); }, "BadRawUnicode");
        expect(throws<std::out_of_range>([&] { internalMath(parser, u"\\U{110000}"); }));
        std::vector<std::string> calls;
        CreationCallsScope scope(calls);
        fixture.options->request().nodeFactory.setCreators(
            NodeCreators{.node = traceNode, .token = unexpectedToken, .text = traceText});
        NodeDefinition definition;
        definition.attributes.set(u"mathvariant", u"-tex-calligraphic");
        auto &node = internalText(parser, u"  a\n\n b  ", std::move(definition));
        expect(std::ranges::equal(calls, std::array<std::string, 2>{"text", "node"}));
        expect(NodeUtil::getText(node) == u"\u00a0a  b\u00a0");
        expect(node.properties().tex.ignore_variant.value_or(false));
    };
    "ParseUtil copies Source nodes and registers copied sparse script lists"_test = [] {
        Fixture fixture;
        TexParser parser(u"", ParserEnvironmentSeed{}, *fixture.options);
        auto &base = fixture.token();
        auto &upper = fixture.token(NodeKind::mi, u"n");
        auto &script = fixture.node(NodeKind::msubsup);
        NodeUtil::setChild(script, 0, base);
        NodeUtil::setChild(script, 2, upper);
        script.attributes().set(u"id", u"drop");
        fixture.options->addNode(u"custom", script);
        auto &copy = copyNode(script, parser);
        expect(&copy != &script);
        expect(copy.children().size() == 3_u);
        expect(copy.children()[1] == nullptr);
        expect(copy.children()[0] != &base);
        expect(copy.children()[2]->parent() == &copy);
        expect(copy.attributes().own(u"id") == nullptr);
        expect(copy.properties().tex.in_lists->size() == 2_u);
        expect(fixture.options->request().nodeLists[u"custom"].back() == &copy);
        expect(script.properties().tex.in_lists->size() == 1_u);
    };
    "ParseUtil under over and finite NodeUtil definitions preserve Source properties"_test = [] {
        Fixture fixture;
        TexParser parser(u"", ParserEnvironmentSeed{}, *fixture.options);
        auto &base = fixture.token(NodeKind::mo, u"∑");
        auto &script = fixture.token(NodeKind::mi, u"a");
        auto &node = underOver(parser, base, script, u"over", true);
        expect(base.properties().tex.movable_limits == false);
        expect(!std::get<bool>(*base.attributes().own(u"movablelimits")));
        expect(node.kind() == NodeKind::tex_atom);
        expect(node.properties().tex.sub_sup_allowed.value_or(false));
        expect(node.properties().tex.move_sup_sub.value_or(false));
        auto inner = node.findNodes(NodeKind::munderover);
        expect(inner.size() == 1_u);
        expect(inner[0]->children()[1] == nullptr);
        expect(inner[0]->children()[2] == &script);
        AttributeSet values;
        values.set(u"variantForm", true);
        values.set(u"useHeight", false);
        values.set(u"texClass", 3.0);
        values.set(u"movablelimits", true);
        values.set(u"autoOP", false);
        values.set(u"open", u"(");
        values.set(u"inferred", true);
        auto &mapped = parser.createToken(NodeKind::mo, u"∑", std::move(values));
        expect(mapped.properties().tex.variant_form.value_or(false));
        expect(mapped.properties().table.use_height == false);
        expect(mapped.properties().tex.explicit_tex_class == TexClass::rel);
        expect(mapped.properties().tex.auto_operator == false);
        expect(mapped.properties().tex.open.value() == u"(");
        expect(mapped.attributes().own(u"variantForm") == nullptr);
        expect(mapped.attributes().own(u"inferred") == nullptr);
        expect(std::get<bool>(*NodeUtil::getMoAttribute(mapped, u"movablelimits")));
        mapped.attributes().set(u"movablelimits", false);
        expect(!std::get<bool>(*NodeUtil::getMoAttribute(mapped, u"movablelimits")));
    };
    "ParseUtil array equation and expansion helpers use actual request state"_test = [] {
        Fixture fixture;
        TexParser parser(u"", ParserEnvironmentSeed{}, *fixture.options);
        auto array = fixture.item(StackKind::array);
        auto &item = dynamic_cast<ArrayItem &>(*array);
        setArrayAlign(item, u" t ");
        expect(attr(item.arraydef, u"align") == u"baseline 1");
        parser.setString(u"oldtail");
        parser.setPosition(3);
        setArrayAlign(item, u" l ", &parser);
        expect(parser.string() == u"[ l ]tail");
        expect(parser.position() == 0_u);
        parser.stack.Pop();
        parser.stack.Push(fixture.item(StackKind::start, StartItemInit{parser.stack.global()}));
        checkEqnEnv(parser);
        parser.stack.Push(array);
        item.nestable = true;
        checkEqnEnv(parser);
        texError([&] { checkEqnEnv(parser, false); }, "ErroneousNestingEq");
        expect(addArgs(parser, u"\\alpha", u"beta") == u"\\alpha beta");
        const std::array<std::u16string, 1> args{u"x"};
        expect(substituteArgs(parser, args, u"#1#1") == u"xx");
        fixture.effective.maxMacros = 0;
        texError([&] { checkMaxMacros(parser, false); }, "MaxMacroSub2");
        expect(parser.macroCount == 1_u);
        expect(mmlFilterAttribute(parser, u"id", u" value ") == u" value ");
    };
    "add_args_control_word_boundaries"_test = [] {
        expect(addArgs(u"left", u"right", 20) == u"leftright");
        expect(addArgs(u"\\alpha", u"beta", 20) == u"\\alpha beta");
        expect(addArgs(u"prefix\\alpha", u"beta", 20) == u"prefix\\alpha beta");
        expect(addArgs(u"\\\\alpha", u"beta", 20) == u"\\\\alphabeta");
        expect(addArgs(u"\\\\\\alpha", u"beta", 20) == u"\\\\\\alpha beta");
        expect(addArgs(u"\\#", u"beta", 20) == u"\\#beta");
        expect(addArgs(u"", u"beta", 20) == u"beta");
    };

    "add_args_buffer_limit_includes_separator"_test = [] {
        expect(addArgs(u"\\alpha", u"b", 8) == u"\\alpha b");
        expectTexError([] { static_cast<void>(addArgs(u"\\alpha", u"b", 7)); }, "MaxBufferSize",
                       u"MathJax internal buffer size exceeded; is there a recursive macro call?");
        expect(addArgs(u"123", u"45", 5) == u"12345");
        expectTexError([] { static_cast<void>(addArgs(u"123", u"45", 4)); }, "MaxBufferSize",
                       u"MathJax internal buffer size exceeded; is there a recursive macro call?");
    };

    "substitute_args_preserves_escapes_and_boundaries"_test = [] {
        const std::array<std::u16string, 2> arguments{u"one", u"😀"};
        const std::array<std::u16string, 1> one_argument{u"beta"};
        const std::array<std::u16string, 2> extra_arguments{u"one", u"unused"};
        const std::array<std::u16string, 9> nine_arguments{u"1", u"2", u"3", u"4", u"5",
                                                           u"6", u"7", u"8", u"9"};
        expect(substituteArgs(arguments, u"#1/#2", 20) == u"one/😀");
        expect(substituteArgs(arguments, u"\\#1", 20) == u"\\#1");
        expect(substituteArgs(one_argument, u"\\{#1}", 20) == u"\\{beta}");
        expect(substituteArgs(one_argument, u"\\\\#1", 20) == u"\\\\beta");
        expect(substituteArgs(arguments, u"##1", 20) == u"#1");
        expect(substituteArgs(one_argument, u"\\alpha#1", 20) == u"\\alpha beta");
        expect(substituteArgs(extra_arguments, u"#1", 20) == u"one");
        expect(substituteArgs(nine_arguments, u"#9", 20) == u"9");
        expect(substituteArgs(std::span<std::u16string const>{}, u"tail\\", 20) == u"tail\\");
        expect(substituteArgs(arguments, u"plain text", 20) == u"plain text");
    };

    "substitute_args_parameter_errors_and_order"_test = [] {
        const std::array<std::u16string, 1> arguments{u"one"};
        for (const auto replacement : {u"#", u"#0", u"#x", u"#2", u"#9"}) {
            expectTexError(
                [replacement, &arguments] {
                    static_cast<void>(substituteArgs(arguments, replacement, 0));
                },
                "IllegalMacroParam", u"Illegal macro parameter reference");
        }
        expectTexError(
            [] {
                static_cast<void>(substituteArgs(std::span<std::u16string const>{}, u"#1", 100));
            },
            "IllegalMacroParam", u"Illegal macro parameter reference");
        expectTexError([&arguments] { static_cast<void>(substituteArgs(arguments, u"#1", 2)); },
                       "MaxBufferSize",
                       u"MathJax internal buffer size exceeded; is there a recursive macro call?");
    };

    "substitute_args_limit_counts_utf16_code_units"_test = [] {
        const std::array<std::u16string, 1> emoji{u"😀"};
        expect(addArgs(u"", u"😀", 2) == u"😀");
        expectTexError([] { static_cast<void>(addArgs(u"", u"😀", 1)); }, "MaxBufferSize",
                       u"MathJax internal buffer size exceeded; is there a recursive macro call?");
        expectTexError([&emoji] { static_cast<void>(substituteArgs(emoji, u"#1", 1)); },
                       "MaxBufferSize",
                       u"MathJax internal buffer size exceeded; is there a recursive macro call?");
    };

    "expansion limits remain numeric comparison thresholds"_test = [] {
        expect(addArgs(u"", u"", 0.0).empty());
        texError([] { (void)addArgs(u"", u"", -1.0); }, "MaxBufferSize");
        expect(addArgs(u"a", u"b", 2.9) == u"ab");
        texError([] { (void)addArgs(u"ab", u"c", 2.9); }, "MaxBufferSize");
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        const auto infinity = std::numeric_limits<double>::infinity();
        expect(addArgs(u"a", u"b", nan) == u"ab");
        expect(addArgs(u"a", u"b", infinity) == u"ab");
        ExpansionBudget rejected{nan, infinity, 0};
        texError([&] { checkMaxMacros(rejected); }, "MaxMacroSub1");
        expect(rejected.macroCount == 1_u);
        ExpansionBudget unlimited{infinity, nan, 0};
        checkMaxMacros(unlimited);
        expect(unlimited.macroCount == 1_u);
    };
    "check_max_macros_increments_before_error"_test = [] {
        ExpansionBudget budget{2, 100, 0};
        checkMaxMacros(budget);
        checkMaxMacros(budget, false);
        expect(budget.macroCount == 2);
        expectTexError(
            [&budget] { checkMaxMacros(budget, true); }, "MaxMacroSub1",
            u"MathJax maximum macro substitution count exceeded; is here a recursive macro call?");
        expect(budget.macroCount == 3);

        ExpansionBudget environment{0, 100, 0};
        expectTexError([&environment] { checkMaxMacros(environment, false); }, "MaxMacroSub2",
                       u"MathJax maximum substitution count exceeded; is there a recursive latex "
                       u"environment?");
        expect(environment.macroCount == 1);
    };

    "fix_initial_mo_skips_null_and_mspace_then_stops_at_first_eligible_node"_test = [] {
        Fixture fixture;
        helperSetup(fixture);
        auto &space = fixture.node(NodeKind::mspace);
        auto &ordinary = fixture.token(NodeKind::mi, u"x");
        auto &laterOperator = fixture.token(NodeKind::mo, u"=");
        std::vector<MmlNode *> nodes{nullptr, &space, &ordinary, &laterOperator};
        const auto documentSize = fixture.document.size();

        parse_util::fixInitialMO(*fixture.options, nodes);

        expect(fixture.document.size() == documentSize);
        expect(nodes.size() == 4_u);
        if (nodes.size() != 4_u)
            return;
        expect(nodes[0] == nullptr);
        expect(nodes[1] == &space);
        expect(nodes[2] == &ordinary);
        expect(nodes[3] == &laterOperator);
    };

    "fix_initial_mo_inserts_an_empty_mi_before_an_embellished_operator"_test = [] {
        Fixture fixture;
        helperSetup(fixture);
        auto &space = fixture.node(NodeKind::mspace);
        auto &operatorNode = fixture.token(NodeKind::mo, u"=");
        auto &script = fixture.token(NodeKind::mn, u"2");
        const std::array<MmlNode *, 2> scriptedChildren{&operatorNode, &script};
        auto &embellished = fixture.node(NodeKind::msup, scriptedChildren);
        expect(embellished.isEmbellished());
        std::vector<MmlNode *> nodes{&space, &embellished};
        const auto documentSize = fixture.document.size();

        parse_util::fixInitialMO(*fixture.options, nodes);

        expect(fixture.document.size() == documentSize + 1u);
        expect(nodes.size() == 3_u);
        if (nodes.size() != 3_u)
            return;
        expect(nodes[1] == &space);
        expect(nodes[2] == &embellished);
        expect(nodes[0] != nullptr && nodes[0] != &space && nodes[0] != &embellished);
        if (!nodes[0])
            return;
        expect(nodes[0]->kind() == NodeKind::mi);
        expect(nodes[0]->children().empty());
        expect(&nodes[0]->document() == &fixture.document);
    };

    "fix_initial_mo_skips_empty_tex_atoms_and_inserts_for_a_nonempty_rel_tex_atom"_test = [] {
        Fixture fixture;
        helperSetup(fixture);
        auto &emptyChild = fixture.node(NodeKind::mspace);
        const std::array<MmlNode *, 1> emptyAtomChildren{&emptyChild};
        auto &emptyRelAtom = fixture.node(NodeKind::tex_atom, emptyAtomChildren);
        emptyRelAtom.setTexClass(TexClass::rel);

        auto &relationContent = fixture.token(NodeKind::mi, u"r");
        const std::array<MmlNode *, 1> relationRowChildren{&relationContent};
        auto &relationRow = fixture.node(NodeKind::mrow, relationRowChildren);
        const std::array<MmlNode *, 1> relationAtomChildren{&relationRow};
        auto &relationAtom = fixture.node(NodeKind::tex_atom, relationAtomChildren);
        relationAtom.setTexClass(TexClass::rel);
        std::vector<MmlNode *> nodes{&emptyRelAtom, &relationAtom};
        const auto documentSize = fixture.document.size();

        parse_util::fixInitialMO(*fixture.options, nodes);

        expect(fixture.document.size() == documentSize + 1u);
        expect(nodes.size() == 3_u);
        if (nodes.size() != 3_u)
            return;
        expect(nodes[1] == &emptyRelAtom);
        expect(nodes[2] == &relationAtom);
        expect(nodes[0] != nullptr && nodes[0]->kind() == NodeKind::mi);
        if (!nodes[0])
            return;
        expect(nodes[0]->children().empty());
        expect(&nodes[0]->document() == &fixture.document);
    };

    "fix_initial_mo_stops_at_a_non_rel_tex_atom_before_a_later_relation"_test = [] {
        Fixture fixture;
        helperSetup(fixture);
        auto &ordinaryContent = fixture.token(NodeKind::mi, u"a");
        const std::array<MmlNode *, 1> ordinaryRowChildren{&ordinaryContent};
        auto &ordinaryRow = fixture.node(NodeKind::mrow, ordinaryRowChildren);
        const std::array<MmlNode *, 1> ordinaryAtomChildren{&ordinaryRow};
        auto &ordinaryAtom = fixture.node(NodeKind::tex_atom, ordinaryAtomChildren);
        ordinaryAtom.setTexClass(TexClass::ord);

        auto &relationContent = fixture.token(NodeKind::mi, u"r");
        const std::array<MmlNode *, 1> relationRowChildren{&relationContent};
        auto &relationRow = fixture.node(NodeKind::mrow, relationRowChildren);
        const std::array<MmlNode *, 1> relationAtomChildren{&relationRow};
        auto &relationAtom = fixture.node(NodeKind::tex_atom, relationAtomChildren);
        relationAtom.setTexClass(TexClass::rel);
        std::vector<MmlNode *> nodes{&ordinaryAtom, &relationAtom};
        const auto documentSize = fixture.document.size();

        parse_util::fixInitialMO(*fixture.options, nodes);

        expect(fixture.document.size() == documentSize);
        expect(nodes.size() == 2_u);
        if (nodes.size() != 2_u)
            return;
        expect(nodes[0] == &ordinaryAtom);
        expect(nodes[1] == &relationAtom);
    };
    "NodeUtil copies attribute values and merged native properties"_test = [] {
        Fixture fixture;
        auto &source = fixture.node(NodeKind::mi);
        auto &destination = fixture.node(NodeKind::mrow);
        source.attributes().set(u"data-copy", u"source");
        source.attributes().setInherited(u"mathcolor", u"purple");
        source.properties().tex.auto_operator = true;
        source.properties().table.use_height = false;
        destination.properties().tex.move_sup_sub = true;

        NodeUtil::copyAttributes(source, destination);

        const auto *own = destination.attributes().own(u"data-copy");
        const auto *inherited = destination.attributes().inherited(u"mathcolor");
        expect(own != nullptr);
        if (own) expect(std::get<std::u16string>(*own) == u"source");
        expect(inherited != nullptr);
        if (inherited) expect(std::get<std::u16string>(*inherited) == u"purple");
        expect(destination.properties().tex.auto_operator == true);
        expect(destination.properties().table.use_height == false);
        expect(destination.properties().tex.move_sup_sub == true);
        destination.attributes().set(u"data-copy", u"changed");
        const auto *sourceValue = source.attributes().own(u"data-copy");
        expect(sourceValue != nullptr);
        const auto *sourceText = sourceValue ? std::get_if<std::u16string>(sourceValue) : nullptr;
        expect(sourceText && *sourceText == u"source");
    };

    "NodeUtil selects operator forms and postfix attribute fallback"_test = [] {
        Fixture fixture;
        auto &plus = fixture.token(NodeKind::mo, u"+");
        const auto *infix = NodeUtil::getOp(plus, u"infix");
        const auto *prefix = NodeUtil::getOp(plus, u"prefix");
        expect(infix != nullptr);
        expect(prefix != nullptr);
        if (infix) expect(infix->left == 4.0);
        if (prefix) expect(prefix->left == 0.0);

        auto &bang = fixture.token(NodeKind::mo, u"!");
        const auto *fence = NodeUtil::getMoAttribute(bang, u"fence");
        expect(fence != nullptr);
        if (fence) {
            const auto *value = std::get_if<bool>(fence);
            expect(value != nullptr);
            if (value) expect(*value);
        }
        bang.attributes().set(u"fence", false);
        const auto *explicitFence = NodeUtil::getMoAttribute(bang, u"fence");
        const auto *explicitValue = explicitFence ? std::get_if<bool>(explicitFence) : nullptr;
        expect(explicitValue && !*explicitValue);
        bang.attributes().erase(u"fence");
        bang.attributes().setInherited(u"fence", false);
        const auto *inheritedFence = NodeUtil::getMoAttribute(bang, u"fence");
        const auto *inheritedValue = inheritedFence ? std::get_if<bool>(inheritedFence) : nullptr;
        expect(inheritedValue && !*inheritedValue);
    };

    "NodeUtil setChild updates child slot and parent links"_test = [] {
        Fixture fixture;
        auto &parent = fixture.node(NodeKind::msubsup);
        auto &other = fixture.node(NodeKind::mrow);
        auto &child = fixture.token(NodeKind::mi, u"n");

        NodeUtil::setChild(parent, 2, child);
        expect(parent.children().size() == 3_u);
        if (parent.children().size() != 3)
            return;
        expect(parent.children()[0] == nullptr);
        expect(parent.children()[1] == nullptr);
        expect(parent.children()[2] == &child);
        expect(child.parent() == &parent);

        NodeUtil::setChild(other, 1, child);
        expect(parent.children().size() == 2_u);
        if (parent.children().size() != 2)
            return;
        expect(parent.children()[0] == nullptr);
        expect(parent.children()[1] == nullptr);
        expect(other.children().size() == 2_u);
        if (other.children().size() != 2)
            return;
        expect(other.children()[0] == nullptr);
        expect(other.children()[1] == &child);
        expect(child.parent() == &other);
    };

};
