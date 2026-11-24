#include "support/folia_test.hpp"
import folia.math.input.tex.session;

using namespace boost::ut;
using folia::math::tex::LabelRecord;
using folia::math::tex::MacroDefinition;
using folia::math::tex::TexSessionState;

suite native_tex_session_tests = [] {
    "tex_color_session_lifetime_follows_jax_not_equation_or_tag_reset"_test = [] {
        TexSessionState session;
        expect(session.colorModel() == nullptr);
        auto &colors = session.enableColor();
        colors.defineColor(u"rgb", u"Stored", u"0,0.5,1");
        auto first = session.beginEquation();
        session.finishEquation(first);
        auto second = session.beginEquation();
        session.finishEquation(second);
        expect(session.colorModel() == &colors);
        expect(session.colorModel()->getColor(u"named", u"Stored") == u"#007fff");
        session.resetTags();
        expect(&session.enableColor() == &colors);
        expect(std::as_const(session).colorModel()->getColor(u"named", u"Stored") == u"#007fff");

        TexSessionState other;
        expect(other.enableColor().getColor(u"named", u"Stored") == u"Stored");
        auto copied = session;
        session.colorModel()->defineColor(u"named", u"Stored", u"blue");
        expect(copied.colorModel()->getColor(u"named", u"Stored") == u"#007fff");
        expect(session.colorModel()->getColor(u"named", u"Stored") == u"blue");
    };

    "macro_definition_owns_utf16_replacement_prefix_delimiters_and_default"_test = [] {
        MacroDefinition macro{
            u"\\frac{#1}{#2}", u"\\foo", {u"", u"+"}, 2, std::u16string{u"x\U0001F600"}};
        macro.validate();
        expect(macro.replacement_source == u"\\frac{#1}{#2}");
        expect(macro.parameter_prefix == u"\\foo");
        expect(macro.parameter_delimiters.size() == 2_u);
        expect(macro.parameter_delimiters[1] == u"+");
        expect(macro.default_first_argument.value() == u"x\U0001F600");
    };

    "macro_definition_checks_templates_and_preserves_explicit_parameter_declarations"_test = [] {
        MacroDefinition bad{u"#2", {}, {}, 1, std::nullopt};
        bad.parameter_template = true;
        expect(throws<std::invalid_argument>([&] { bad.validate(); }));
        MacroDefinition no_parameter{u"x", {}, {}, 0, std::u16string{u"default"}};
        no_parameter.substitute_parameters = true;
        expect(nothrow([&] { no_parameter.validate(); }));
        MacroDefinition many{.arity = 10};
        expect(nothrow([&] { many.validate(); }));
        many.parameter_template = true;
        expect(throws<std::invalid_argument>([&] { many.validate(); }));
    };

    "equation_state_is_owned_and_source_finish_commits_labels_ids_and_counter"_test = [] {
        TexSessionState session;
        auto equation = session.beginEquation();
        equation.current_tag.environment = "align";
        equation.current_tag.taggable = true;
        equation.current_tag.tag_format = std::vector<std::u16string>{u"(", u"7", u")"};
        equation.history.push_back(equation.current_tag);
        equation.stack.push_back(equation.current_tag);
        expect(equation.labels.empty());
        expect(equation.ids.empty());
        equation.labels.insert_or_assign(u"δ-equation", LabelRecord{u"7", u"mjx-eqn:δ"});
        equation.ids.insert(u"mjx-eqn:δ");
        equation.counter.equation = 1;
        auto finish = session.finishEquation(equation);
        expect(!finish.recompile_counter.has_value());
        expect(session.findLabel(u"δ-equation") != nullptr);
        expect(session.findLabel(u"δ-equation")->id == u"mjx-eqn:δ");
        expect(session.containsId(u"mjx-eqn:δ"));
        expect(session.counters().equation == 1_i);

        auto next = session.beginEquation();
        expect(next.current_tag.environment.empty());
        expect(next.history.empty());
        expect(next.stack.empty());
        expect(next.labels.empty());
        expect(next.ids.empty());
        expect(next.counter.equation == 1_i);
    };

    "recompile_holds_counter_and_redo_reports_precommit_counter"_test = [] {
        TexSessionState session;
        auto initial = session.beginEquation();
        initial.counter.equation = 5;
        session.finishEquation(initial);

        auto replay = session.beginEquation(true, 2);
        expect(replay.ref_update);
        expect(replay.counter.equation == 2_i);
        expect(throws<std::invalid_argument>([&] { (void)session.beginEquation(true); }));
        replay.counter.equation = 3;
        replay.redo = true;
        replay.labels.insert_or_assign(u"再計算", LabelRecord{u"2", u"mjx-eqn:再計算"});
        auto finish = session.finishEquation(replay);
        expect(finish.recompile_counter.value() == 5_i);
        expect(session.counters().equation == 5_i);
        expect(session.findLabel(u"再計算") != nullptr);
    };

    "finish_commits_given_source_effects_and_tag_reset_preserves_macros"_test = [] {
        TexSessionState session;
        session.defineMacro(u"foo", MacroDefinition{u"x", {}, {}, 0, std::nullopt});
        auto equation = session.beginEquation();
        equation.labels.insert_or_assign(u"kept-after-error", LabelRecord{u"1", u"id"});
        // finishEquation commits the state supplied by its caller; it exposes
        // no transactional rollback facility.
        session.finishEquation(equation);
        expect(session.findLabel(u"kept-after-error") != nullptr);
        session.resetTags(12);
        expect(session.findLabel(u"kept-after-error") == nullptr);
        expect(session.counters().equation == 12_i);
        expect(session.findMacro(u"foo") != nullptr);
        session.clearMacros();
        expect(session.findMacro(u"foo") == nullptr);
    };

    "macro_keys_own_full_utf16_control_sequence_names"_test = [] {
        TexSessionState session;
        // Source's newcommand accepts one BMP control symbol, while def uses
        // TexParser.GetCS and also accepts the supplementary pair. The actual
        // accepted/rejected spellings and Source error are recorded in the
        // paired source-witness.json fixture.
        MacroDefinition alpha{u"α-body", {}, {}, 0, std::nullopt};
        MacroDefinition supplementary{u"😀-body", {}, {}, 0, std::nullopt};
        MacroDefinition control_symbol{u"%-body", {}, {}, 0, std::nullopt};
        session.defineMacro(u"α", alpha);
        session.defineMacro(u"😀", supplementary);
        session.defineMacro(u"%", control_symbol);
        expect(std::u16string_view(u"😀").size() == 2_u);

        expect(session.findMacro(u"α") != nullptr);
        expect(session.findMacro(u"α")->replacement_source == u"α-body");
        expect(session.findMacro(u"😀") != nullptr);
        expect(session.findMacro(u"😀")->replacement_source == u"😀-body");
        expect(session.findMacro(u"%") != nullptr);
        expect(session.findMacro(u"absent") == nullptr);

        expect(session.eraseMacro(u"😀"));
        expect(session.findMacro(u"😀") == nullptr);
        expect(!session.eraseMacro(u"😀"));
        expect(session.findMacro(u"α") != nullptr);
    };
};
