#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.tex_atom;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.core.mml_tree.mml_nodes.mn;
import folia.math.core.mml_tree.mml_nodes.mi;
import folia.math.core.mml_tree.mml_nodes.msubsup;
import folia.math.core.mml_tree.mml_nodes.munderover;
import folia.math.core.mml_tree.mml_nodes.mmultiscripts;

namespace {
using namespace boost::ut;
using namespace folia::math::mml;

suite typed_mml_script_families = [] {
    "script_family_kinds_arities_and_child_slots"_test = [] {
        MmlDocument document;
        auto &subsup = document.factory().make(NodeKind::msubsup);
        auto &sub = document.factory().make(NodeKind::msub);
        auto &sup = document.factory().make(NodeKind::msup);
        auto &munderover = document.factory().make(NodeKind::munderover);
        auto &under = document.factory().make(NodeKind::munder);
        auto &over = document.factory().make(NodeKind::mover);
        auto &multi = document.factory().make(NodeKind::mmultiscripts);
        auto &prescripts = document.factory().make(NodeKind::mprescripts);
        auto &none = document.factory().make(NodeKind::none);

        expect(subsup.kind() == NodeKind::msubsup && subsup.arity() == 3);
        expect(sub.kind() == NodeKind::msub && sub.arity() == 2);
        expect(sup.kind() == NodeKind::msup && sup.arity() == 2);
        expect(eq(dynamic_cast<MmlMsubsup &>(sub).base(), std::size_t{0}));
        expect(eq(dynamic_cast<MmlMsubsup &>(sub).sub(), std::size_t{1}));
        expect(eq(dynamic_cast<MmlMsubsup &>(sub).sup(), std::size_t{2}));
        expect(eq(dynamic_cast<MmlMsubsup &>(sup).sup(), std::size_t{1}));
        expect(eq(dynamic_cast<MmlMsubsup &>(sup).sub(), std::size_t{2}));

        expect(munderover.kind() == NodeKind::munderover && munderover.arity() == 3);
        expect(under.kind() == NodeKind::munder && under.arity() == 2);
        expect(over.kind() == NodeKind::mover && over.arity() == 2);
        expect(dynamic_cast<MmlMunderover &>(munderover).linebreakContainer());
        expect(eq(dynamic_cast<MmlMunderover &>(over).over(), std::size_t{1}));
        expect(eq(dynamic_cast<MmlMunderover &>(over).under(), std::size_t{2}));
        expect(multi.kind() == NodeKind::mmultiscripts && multi.arity() == 1);
        expect(prescripts.kind() == NodeKind::mprescripts && prescripts.arity() == 0);
        expect(none.kind() == NodeKind::none && none.arity() == 0);

        expect(subsup.attributes().defaultValue(u"subscriptshift") != nullptr);
        expect(munderover.attributes().defaultValue(u"accent") != nullptr);
        expect(under.attributes().defaultValue(u"accentunder") != nullptr);
    };
};

double inheritedScriptLevel(MmlNode const &node) {
    const auto *value = node.attributes().lookup(u"scriptlevel");
    if (!value)
        return -1;
    const auto *level = std::get_if<double>(value);
    return level ? *level : -1;
}

suite typed_mml_script_inheritance = [] {
    "msup_inherits_into_its_physical_script_slot_without_a_subscript"_test = [] {
        MmlDocument document;
        auto &base = document.factory().make(NodeKind::none);
        auto &script = document.factory().make(NodeKind::none);
        std::array<MmlNode *, 2> children{&base, &script};
        auto &node = document.factory().make(NodeKind::msup, {}, children);
        node.setInheritedAttributes({}, true, 2, false);
        expect(node.children().size() == 2_u);
        expect(inheritedScriptLevel(base) == 2.0);
        expect(inheritedScriptLevel(script) == 3.0);
        expect(!base.properties().tex.prime_style.value_or(false));
        expect(!script.properties().tex.prime_style.value_or(false));
        node.setInheritedAttributes({}, false, 4, true);
        expect(inheritedScriptLevel(script) == 5.0);
        expect(script.properties().tex.prime_style.value_or(false));
    };

    "subsup_increases_script_level_only_for_script_children"_test = [] {
        MmlDocument document;
        auto &base = document.factory().make(NodeKind::none);
        auto &sub = document.factory().make(NodeKind::none);
        auto &sup = document.factory().make(NodeKind::none);
        std::array<MmlNode *, 3> children{&base, &sub, &sup};
        auto &node = document.factory().make(NodeKind::msubsup, {}, children);
        node.setInheritedAttributes({}, true, 2, false);
        expect(eq(inheritedScriptLevel(base), 2.0));
        expect(eq(inheritedScriptLevel(sub), 3.0));
        expect(eq(inheritedScriptLevel(sup), 3.0));
    };

    "mover_uses_over_slot_and_explicit_accent_script_level"_test = [] {
        MmlDocument document;
        auto &base = document.factory().make(NodeKind::none);
        auto &over = document.factory().make(NodeKind::none);
        std::array<MmlNode *, 2> children{&base, &over};
        auto &node = document.factory().make(NodeKind::mover, {}, children);
        node.attributes().set(u"accent", true);
        node.setInheritedAttributes({}, true, 4, false);
        expect(eq(inheritedScriptLevel(base), 4.0));
        expect(eq(inheritedScriptLevel(over), 4.0));
    };

    "multiscripts_assigns_paired_post_and_prescripts_by_alternating_slot"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        auto &base = factory.make(NodeKind::none);
        auto &postSub = factory.make(NodeKind::none);
        auto &postSup = factory.make(NodeKind::none);
        auto &marker = factory.make(NodeKind::mprescripts);
        auto &preSub = factory.make(NodeKind::none);
        auto &preSup = factory.make(NodeKind::none);
        std::array<MmlNode *, 6> children{&base, &postSub, &postSup, &marker, &preSub, &preSup};
        auto &node = factory.make(NodeKind::mmultiscripts, {}, children);

        node.setInheritedAttributes({}, false, 0, false);
        expect(node.children().size() == 6_u);
        expect(inheritedScriptLevel(base) == 0.0);
        expect(inheritedScriptLevel(postSub) == 1.0);
        expect(inheritedScriptLevel(postSup) == 1.0);
        expect(inheritedScriptLevel(preSub) == 1.0);
        expect(inheritedScriptLevel(preSup) == 1.0);
        expect(postSub.properties().tex.prime_style.value_or(false));
        expect(!postSup.properties().tex.prime_style.value_or(false));
        expect(preSub.properties().tex.prime_style.value_or(false));
        expect(!preSup.properties().tex.prime_style.value_or(false));
    };

    "multiscripts_adds_owned_none_for_unpaired_postscript"_test = [] {
        MmlDocument document;
        auto &base = document.factory().make(NodeKind::none);
        auto &sub = document.factory().make(NodeKind::none);
        std::array<MmlNode *, 2> children{&base, &sub};
        auto &node = document.factory().make(NodeKind::mmultiscripts, {}, children);
        node.setInheritedAttributes({}, false, 1, true);
        expect(eq(node.children().size(), std::size_t{3}));
        expect(node.children().back()->kind() == NodeKind::none);
        expect(eq(inheritedScriptLevel(*node.children().back()), 2.0));
    };
    "fixMmultiscripts_true_suppresses_diagnostic_without_repairing_children"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        auto &base = factory.make(NodeKind::mi);
        auto &subscript = factory.make(NodeKind::none);
        const std::array<MmlNode *, 2> originalChildren{&base, &subscript};
        auto &multi = factory.make(NodeKind::mmultiscripts, {}, originalChildren);
        auto &row = factory.make(NodeKind::mrow);
        row.appendChild(multi);

        VerifyOptions options;
        options.checkArity = false;
        options.checkMathvariants = false;
        options.fixMmultiscripts = true;
        row.verifyTree(options);

        expect(row.children().size() == 1_u);
        if (row.children().empty()) return;
        expect(row.children().front() == &multi);
        expect(multi.parent() == &row);
        expect(multi.children().size() == 2_u);
        if (multi.children().size() != 2_u) return;
        expect(multi.children()[0] == &base);
        expect(multi.children()[1] == &subscript);
        expect(base.parent() == &multi);
        expect(subscript.parent() == &multi);
    };

    "fixMmultiscripts_false_wraps_original_node_with_parity_message"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        auto &base = factory.make(NodeKind::mi);
        auto &subscript = factory.make(NodeKind::none);
        const std::array<MmlNode *, 2> originalChildren{&base, &subscript};
        auto &multi = factory.make(NodeKind::mmultiscripts, {}, originalChildren);
        auto &row = factory.make(NodeKind::mrow);
        row.appendChild(multi);

        VerifyOptions options;
        options.checkArity = false;
        options.checkMathvariants = false;
        options.fixMmultiscripts = false;
        row.verifyTree(options);

        expect(row.children().size() == 1_u);
        if (row.children().empty()) return;
        auto *error = row.children().front();
        expect(error != nullptr);
        if (!error) return;
        expect(error->kind() == NodeKind::merror);
        expect(error->parent() == &row);
        const auto *message = error->attributes().own(u"data-mjx-message");
        expect(message && std::get_if<std::u16string>(message) &&
               *std::get_if<std::u16string>(message) ==
                   u"There must be an equal number of scripts of each type");
        expect(error->attributes().own(u"title") == nullptr);

        expect(error->children().size() == 1_u);
        if (error->children().empty()) return;
        auto *errorBody = error->children().front();
        expect(errorBody->kind() == NodeKind::inferred_mrow);
        expect(errorBody->isInferred());
        expect(errorBody->parent() == error);
        expect(errorBody->children().size() == 1_u);
        if (errorBody->children().empty()) return;
        expect(errorBody->children().front() == &multi);
        expect(multi.parent() == errorBody);
        expect(multi.children().size() == 2_u);
        if (multi.children().size() != 2_u) return;
        expect(multi.children()[0] == &base);
        expect(multi.children()[1] == &subscript);
        expect(base.parent() == &multi);
        expect(subscript.parent() == &multi);
    };

    "subsup_set_texclass_routes_each_base_branch_and_isolates_scripts"_test = [] {
        struct Case {
            NodeKind base_kind;
            std::u16string_view text;
            TexClass base_class;
            TexClass wrapper_class;
            std::optional<TexClass> base_previous_class;
            bool wrapper_is_embellished;
            bool returns_base;
            bool is_tex_atom;
        };
        constexpr Case cases[] = {
            {NodeKind::mo, u"+", TexClass::ord, TexClass::ord, std::nullopt, true, true, false},
            {NodeKind::mi, u"x", TexClass::ord, TexClass::ord, std::nullopt, false, true, false},
            {NodeKind::mn, u"1", TexClass::ord, TexClass::ord, std::nullopt, false, false, false},
            {NodeKind::tex_atom, u"1", TexClass::rel, TexClass::rel,
             TexClass::none, false, false, true},
        };

        for (const auto &test_case : cases) {
            MmlDocument document;
            auto &factory = document.factory();
            auto makeToken = [&factory](NodeKind kind, std::u16string_view value,
                                        TexClass tex_class) -> MmlNode & {
                auto &node = factory.make(kind);
                auto &text = factory.make(NodeKind::text);
                dynamic_cast<TextNode &>(text).setText(std::u16string(value));
                node.appendChild(text);
                node.setTexClass(tex_class);
                return node;
            };

            MmlNode *base = nullptr;
            MmlNode *wrapperCoreMO = nullptr;
            if (test_case.is_tex_atom) {
                auto &atom = factory.make(NodeKind::tex_atom);
                atom.properties().tex.explicit_tex_class = TexClass::rel;
                atom.setTexClass(TexClass::rel);
                const auto atomChildren = atom.children();
                expect(atomChildren.size() == 1u)
                    << "TeXAtom factory supplies its inferred row";
                if (atomChildren.size() != 1u) continue;

                auto *inferredRow = atomChildren.front();
                expect(inferredRow && inferredRow->kind() == NodeKind::inferred_mrow &&
                       inferredRow->isInferred()) << "TeXAtom child is its inferred row";
                if (!inferredRow || inferredRow->kind() != NodeKind::inferred_mrow ||
                    !inferredRow->isInferred()) continue;
                expect(inferredRow->parent() == &atom) << "inferred row is owned by TeXAtom";

                auto &digit = makeToken(NodeKind::mn, u"1", TexClass::ord);
                inferredRow->appendChild(digit);
                const auto rowChildren = inferredRow->children();
                expect(rowChildren.size() == 1u && rowChildren.front() == &digit)
                    << "inferred row contains the numeric child";
                if (rowChildren.size() != 1u || rowChildren.front() != &digit) continue;
                expect(digit.parent() == inferredRow) << "numeric child parent is inferred row";
                base = &atom;
                wrapperCoreMO = inferredRow;
            } else {
                const auto initialClass = test_case.base_kind == NodeKind::mo
                                              ? TexClass::bin : TexClass::ord;
                base = &makeToken(test_case.base_kind, test_case.text, initialClass);
                wrapperCoreMO = base;
                const auto tokenChildren = base->children();
                expect(tokenChildren.size() == 1u &&
                       tokenChildren.front()->kind() == NodeKind::text)
                    << "token base owns its text child";
                if (tokenChildren.size() != 1u ||
                    tokenChildren.front()->kind() != NodeKind::text) continue;
                expect(tokenChildren.front()->parent() == base)
                    << "token text child parent is the base";
            }

            auto &sub = makeToken(NodeKind::mo, u"s", TexClass::bin);
            auto &sup = makeToken(NodeKind::mo, u"p", TexClass::bin);
            std::array<MmlNode *, 3> scriptSlots{base, &sub, &sup};
            auto &wrapper = factory.make(NodeKind::msubsup, {}, scriptSlots);
            auto &previous = makeToken(NodeKind::mo, u"=", TexClass::rel);
            const auto previousClass = previous.texClass();
            const auto previousPreviousClass = previous.prevClass();
            const auto previousLevel = previous.prevLevel();

            const auto wrapperChildren = wrapper.children();
            expect(wrapperChildren.size() == 3u) << "msubsup has base, sub, and sup slots";
            if (wrapperChildren.size() != 3u) continue;
            expect(wrapperChildren[0] == base && wrapperChildren[1] == &sub &&
                   wrapperChildren[2] == &sup) << "msubsup preserves its child slots";
            expect(base->parent() == &wrapper && sub.parent() == &wrapper &&
                   sup.parent() == &wrapper && wrapper.parent() == nullptr)
                << "base and scripts are parented by the wrapper";
            expect(wrapper.isEmbellished() == test_case.wrapper_is_embellished)
                << "wrapper embellishment matches Source branch";

            auto *result = wrapper.setTeXclass(&previous);
            expect(result == (test_case.returns_base ? base : &wrapper))
                << "return value follows Source base-class branch";
            expect(wrapper.texClass() == test_case.wrapper_class &&
                   wrapper.prevClass() == TexClass::rel &&
                   wrapper.prevLevel() == std::optional<double>{0.0})
                << "wrapper class and previous state match Source";
            expect(base->texClass() == test_case.base_class &&
                   base->prevClass() == test_case.base_previous_class && !base->prevLevel())
                << "base class and previous state match Source";
            expect(sub.texClass() == TexClass::ord && sub.prevClass() == TexClass::none &&
                   !sub.prevLevel()) << "subscript is classified independently from previous";
            expect(sup.texClass() == TexClass::ord && sup.prevClass() == TexClass::none &&
                   !sup.prevLevel()) << "superscript is classified independently from previous";
            expect(wrapper.core() == base && wrapper.coreMO() == wrapperCoreMO)
                << "wrapper core accessors match Source base and core-MO identities";
            expect(previous.texClass() == previousClass &&
                   previous.prevClass() == previousPreviousClass &&
                   previous.prevLevel() == previousLevel)
                << "class transfer leaves incoming previous node unchanged";
        }
    };

};
} // namespace

namespace {
suite retained_hidden_script_texclass = [] {
    "hidden_phantom_script_keeps_the_copied_operator_spacing_chain"_test = [] {
        MmlDocument document;
        auto make = [&](NodeKind kind) -> MmlNode& { return document.factory().make(kind); };
        auto& x = make(NodeKind::mi);
        auto& plus = make(NodeKind::mo);
        auto& y = make(NodeKind::mi);
        auto& copied = make(NodeKind::inferred_mrow);
        copied.appendChild(x); copied.appendChild(plus); copied.appendChild(y);
        auto& phantom = make(NodeKind::mphantom);
        phantom.appendChild(copied);
        auto& padded = make(NodeKind::mpadded);
        padded.appendChild(phantom);
        auto& visible = make(NodeKind::mrow);
        visible.appendChild(make(NodeKind::mi));
        visible.appendChild(make(NodeKind::mo));
        visible.appendChild(make(NodeKind::mi));
        auto& under = make(NodeKind::munder);
        under.appendChild(visible); under.appendChild(padded);
        x.setTexClass(TexClass::ord);
        plus.setTexClass(TexClass::bin);
        y.setTexClass(TexClass::ord);
        under.setTeXclass(nullptr);
        expect(plus.prevClass() == TexClass::ord);
        expect(y.prevClass() == TexClass::bin);
        expect(plus.texSpacing() == "mediummathspace");
        expect(y.texSpacing() == "mediummathspace");
    };
};
}
