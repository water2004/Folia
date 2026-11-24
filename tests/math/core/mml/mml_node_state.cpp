#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
using namespace boost::ut;
namespace mml = folia::math::mml;

suite native_mml_node_properties = [] {
    "bussproofs proof properties preserve finite insertion update and erase order"_test = [] {
        using K = mml::ProofAttribute;
        mml::ProofNodeProperties values;
        values.setSequent(true);
        values.setAxiom(false);
        values.setProof(true);
        values.setPremiseCount(0);
        values.setRuleDirection(mml::ProofRuleDirection::up);
        values.setLabelledRule(mml::ProofLabels::both);
        values.setProofLabel(mml::ProofSide::left);
        values.setSequentAdjustRight(0);
        values.setSequentAdjustLeft(.5);
        values.setSequentProcessed(true);
        const std::array expected{K::sequent,
                                  K::axiom,
                                  K::proof,
                                  K::premise_count,
                                  K::rule_direction,
                                  K::labelled_rule,
                                  K::proof_label,
                                  K::sequent_adjust_right,
                                  K::sequent_adjust_left,
                                  K::sequent_processed};
        expect(std::ranges::equal(values.orderedKeys(), expected));
        expect(values.axiom() == false);
        expect(values.premise_count() == 0u);
        expect(values.sequent_adjust_right() == 0.0);
        values.setAxiom(true);
        expect(std::ranges::equal(values.orderedKeys(), expected));
        values.erase(K::sequent_processed);
        expect(values.orderedKeys().size() == 9_u);
        expect(!values.sequent_processed());
        values.setSequentProcessed(false);
        expect(std::ranges::equal(values.orderedKeys(), expected));
        expect(values.sequent_processed() == false);
        values.erase(K::axiom);
        values.erase(K::axiom);
        values.setAxiom(false);
        const std::array reinserted{
            K::sequent,           K::proof,       K::premise_count,        K::rule_direction,
            K::labelled_rule,     K::proof_label, K::sequent_adjust_right, K::sequent_adjust_left,
            K::sequent_processed, K::axiom};
        expect(std::ranges::equal(values.orderedKeys(), reinserted));
        for (int repeat = 0; repeat < 192; ++repeat) {
            values.setAxiom(repeat % 2 == 0);
            values.setPremiseCount(static_cast<std::size_t>(repeat % 6));
            expect(std::ranges::equal(values.orderedKeys(), reinserted));
        }
        for (auto key : reinserted)
            values.erase(key);
        expect(values == mml::ProofNodeProperties{});
    };
    "bussproofs property copy and merge keep existing target order and native values"_test = [] {
        using K = mml::ProofAttribute;
        mml::NodeProperties source, destination;
        source.proof.setAxiom(false);
        source.proof.setLabelledRule(mml::ProofLabels::both);
        source.proof.setProof(false);
        source.proof.left_space = 0;
        source.proof.right_space = .5;
        destination.proof.setProof(true);
        destination.proof.setRuleDirection(mml::ProofRuleDirection::up);
        mml::mergeNodeProperties(source, destination);
        const std::array expected{K::proof, K::rule_direction, K::axiom, K::labelled_rule};
        expect(std::ranges::equal(destination.proof.orderedKeys(), expected));
        expect(destination.proof.proof() == false);
        expect(destination.proof.axiom() == false);
        expect(destination.proof.rule_direction() == mml::ProofRuleDirection::up);
        expect(destination.proof.left_space == 0.0);
        expect(destination.proof.right_space == .5);
        auto before = destination;
        mml::mergeNodeProperties(mml::NodeProperties{}, destination);
        expect(destination == before);
        mml::copyNodeProperties(source, destination);
        expect(destination == source);
        destination.proof.erase(K::axiom);
        expect(source.proof.axiom() == false);
        source.proof.mergeFrom(source.proof);
        const std::array sourceKeys{K::axiom, K::labelled_rule, K::proof};
        expect(std::ranges::equal(source.proof.orderedKeys(), sourceKeys));
    };
    "bussproofs move retains zero false and the Source three key transfer sequence"_test = [] {
        using K = mml::ProofAttribute;
        mml::ProofNodeProperties source, destination;
        source.setLabelledRule(mml::ProofLabels::right);
        source.setAxiom(true);
        source.setPremiseCount(0);
        source.setProof(false);
        source.left_space = .25;
        destination.setProof(true);
        destination.setRuleDirection(mml::ProofRuleDirection::up);
        mml::moveProofProperties(source, destination);
        const std::array expected{K::proof, K::rule_direction, K::premise_count, K::labelled_rule};
        expect(std::ranges::equal(destination.orderedKeys(), expected));
        expect(destination.premise_count() == 0u);
        expect(destination.proof() == false);
        expect(destination.labelled_rule() == mml::ProofLabels::right);
        expect(!destination.left_space);
        mml::ProofNodeProperties remaining;
        remaining.setAxiom(true);
        remaining.left_space = .25;
        expect(source == remaining);
        mml::moveProofProperties(destination, destination);
        const std::array afterSelfMove{K::rule_direction};
        expect(std::ranges::equal(destination.orderedKeys(), afterSelfMove));
        expect(!destination.proof());
        expect(!destination.premise_count());
        expect(!destination.labelled_rule());
    };
    "constructor_intrinsic_state_and_family_defaults_match_source"_test = [] {
        mml::MmlDocument document;
        auto &factory = document.factory();
        for (const auto kind : mml::allNodeKinds) {
            std::optional<mml::TexClass> expected;
            switch (kind) {
            case mml::NodeKind::mi:
            case mml::NodeKind::mn:
            case mml::NodeKind::mtext:
            case mml::NodeKind::ms:
            case mml::NodeKind::msqrt:
            case mml::NodeKind::mroot:
            case mml::NodeKind::merror:
            case mml::NodeKind::mphantom:
            case mml::NodeKind::menclose:
            case mml::NodeKind::mtable:
            case mml::NodeKind::mglyph:
            case mml::NodeKind::tex_atom:
                expected = mml::TexClass::ord;
                break;
            case mml::NodeKind::mspace:
                expected = mml::TexClass::none;
                break;
            case mml::NodeKind::mfenced:
                expected = mml::TexClass::inner;
                break;
            default:
                break;
            }
            auto &node = factory.make(kind);
            expect(node.state().working_tex_class == expected);
            if (kind == mml::NodeKind::tex_atom)
                expect(node.properties().tex.explicit_tex_class == mml::TexClass::ord);
            if (kind == mml::NodeKind::mtable)
                expect(node.properties().table.use_height == true);
            if (kind == mml::NodeKind::annotation)
                expect(node.properties().annotation.character_data == true);
        }
        auto &math = factory.make(mml::NodeKind::math);
        expect(!math.state().working_tex_class.has_value());
        expect(!math.properties().table.use_height.has_value());
        expect(!math.properties().annotation.character_data.has_value());
    };

    "all_typed_fields_copy_merge_and_absence_semantics_are_preserved"_test = [] {
        mml::NodeProperties source, destination;
        source.tex.auto_operator = false;
        source.tex.unicode = false;
        source.tex.fix_bold = true;
        source.tex.fix_prescript = false;
        source.tex.fix_stretchy = true;
        source.tex.ignore_variant = false;
        source.tex.math_accent = true;
        source.tex.movable_limits = false;
        source.tex.move_sup_sub = true;
        source.tex.no_dictionary_definition = false;
        source.tex.pseudo_script = true;
        source.tex.relations_combined = false;
        source.tex.small_matrix = true;
        source.tex.sub_sup_allowed = false;
        source.tex.prime_style = true;
        source.tex.variant_form = false;
        source.tex.with_delimiters = true;
        source.tex.open = u"";
        source.tex.close = u")";
        source.tex.left_delimiter = u"[";
        source.tex.right_delimiter = u"]";
        source.tex.primes = u"′′";
        source.tex.keep_attributes = u"class";
        source.tex.vertical_box = u"parbox:baseline";
        source.tex.script_alignment = mml::HorizontalAlignment::center;
        source.tex.script_position = mml::ScriptPosition::superscript;
        source.tex.thickness = mml::Thickness{std::u16string{}};
        source.tex.explicit_tex_class = mml::TexClass::rel;
        source.proof.setAxiom(false);
        source.proof.setProof(true);
        source.proof.setSequent(false);
        source.proof.setSequentProcessed(true);
        source.proof.setPremiseCount(0);
        source.proof.setRuleDirection(mml::ProofRuleDirection::down);
        source.proof.setLabelledRule(mml::ProofLabels::both);
        source.proof.setProofLabel(mml::ProofSide::left);
        source.proof.setSequentAdjustLeft(0.0);
        source.proof.setSequentAdjustRight(-0.25);
        source.proof.left_space = 0.0;
        source.proof.right_space = 0.5;
        source.layout.breakable = false;
        source.layout.force_break = true;
        source.layout.inline_breaks = false;
        source.layout.inline_forced = true;
        source.layout.inline_marked = false;
        source.layout.newline = true;
        source.layout.process_breaks = false;
        source.table.use_height = false;
        source.annotation.character_data = false;

        destination.tex.open = u"preserved";
        destination.proof.setAxiom(true);
        mml::mergeNodeProperties(source, destination);
        expect(destination == source);
        expect(destination.tex.auto_operator == false);
        expect(destination.tex.unicode == false);
        expect(destination.tex.open == u"");
        expect(destination.tex.vertical_box == u"parbox:baseline");
        expect(destination.tex.thickness == source.tex.thickness);
        expect(destination.proof.premise_count() == 0u);
        expect(destination.proof.axiom() == false);
        expect(destination.layout.process_breaks == false);
        expect(destination.table.use_height == false);
        expect(destination.annotation.character_data == false);
        mml::NodeProperties exact;
        mml::copyNodeProperties(source, exact);
        expect(exact == source);
        expect(exact.tex.auto_operator == source.tex.auto_operator);
        expect(exact.tex.explicit_tex_class == mml::TexClass::rel);
        expect(exact.proof.sequent_adjust_right() == -0.25);
        expect(exact.layout.inline_marked == false);
        expect(exact.proof.right_space == 0.5);
        expect(exact.table.use_height == false);
        mml::NodeProperties absent;
        mml::mergeNodeProperties(absent, destination);
        expect(destination == source);
        expect(destination.tex.open == u"");
        expect(destination.proof.premise_count() == 0u);
        expect(destination.layout.process_breaks == false);
    };

    "proof_move_transfers_only_three_present_fields_and_clears_source"_test = [] {
        mml::ProofNodeProperties from, to;
        from.setPremiseCount(2);
        from.setProof(false);
        from.setLabelledRule(mml::ProofLabels::right);
        from.setAxiom(true);
        from.setRuleDirection(mml::ProofRuleDirection::up);
        to.setPremiseCount(0);
        to.setProof(true);
        to.setLabelledRule(mml::ProofLabels::left);
        mml::moveProofProperties(from, to);
        expect(to.premise_count() == 2u);
        expect(to.proof() == false);
        expect(to.labelled_rule() == mml::ProofLabels::right);
        expect(!from.premise_count());
        expect(!from.proof());
        expect(!from.labelled_rule());
        expect(from.axiom() == true);
        expect(from.rule_direction() == mml::ProofRuleDirection::up);
        mml::ProofNodeProperties empty;
        mml::moveProofProperties(empty, to);
        expect(to.premise_count() == 2u);
        expect(to.proof() == false);
    };

    "clone_copies_complete_typed_state_independently"_test = [] {
        mml::MmlDocument source_document, target_document;
        auto &factory = source_document.factory();
        auto &root = factory.make(mml::NodeKind::mrow);
        auto &child = factory.make(mml::NodeKind::mi);
        child.properties().tex.primes = u"′";
        child.properties().proof.setPremiseCount(3);
        child.state().working_tex_class = mml::TexClass::bin;
        child.state().previous_tex_class = mml::TexClass::rel;
        child.state().previous_script_level = -2.375;
        source_document.append(root, child);
        auto &clone = target_document.cloneSubtree(root);
        auto &clone_child = *clone.children()[0];
        expect(clone_child.properties() == child.properties());
        expect(clone_child.state() == child.state());
        expect(clone_child.properties().tex.primes == u"′");
        expect(clone_child.properties().proof.premise_count() == 3u);
        expect(clone_child.state().working_tex_class == mml::TexClass::bin);
        expect(clone_child.state().previous_tex_class == mml::TexClass::rel);
        expect(clone_child.state().previous_script_level == -2.375);
        clone_child.properties().tex.primes = u"changed";
        clone_child.state().working_tex_class.reset();
        expect(child.properties().tex.primes == u"′");
        expect(child.state().working_tex_class == mml::TexClass::bin);
    };
};
