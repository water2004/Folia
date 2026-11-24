#include "support/folia_test.hpp"
import folia.math.input.tex.session;

using namespace boost::ut;
using folia::math::tex::MacroColumnDefinition;
using folia::math::tex::TexSessionState;

suite native_tex_column_definition_session_tests = [] {
    "new_column_definitions_survive_equation_finish_and_tag_reset"_test = [] {
        TexSessionState session;
        auto& definitions = session.columnDefinitions();
        definitions.set(u'Q', MacroColumnDefinition{u"p{#1}", 1.0});
        auto first = session.beginEquation();
        session.finishEquation(first);
        auto second = session.beginEquation();
        session.finishEquation(second);
        session.resetTags(7);
        expect(session.columnDefinitions().find(u'Q') != nullptr);
        expect(session.columnDefinitions().find(u'Q')->replacement == u"p{#1}");
        expect(session.columnDefinitions().find(u'Q')->argument_count == 1.0);
    };

    "definitions_are_session_values_and_session_copies_are_isolated"_test = [] {
        TexSessionState original;
        original.columnDefinitions().set(u'R', MacroColumnDefinition{u"r", 0.0});
        TexSessionState copy = original;
        original.columnDefinitions().set(u'R', MacroColumnDefinition{u"l", 2.0});
        original.columnDefinitions().set(u'X', MacroColumnDefinition{u"center", 0.0});
        expect(original.columnDefinitions().find(u'R')->replacement == u"l");
        expect(copy.columnDefinitions().find(u'R')->replacement == u"r");
        expect(copy.columnDefinitions().find(u'X') == nullptr);
        expect(original.columnDefinitions().erase(u'X'));
        expect(!original.columnDefinitions().erase(u'X'));
    };

    "custom_definition_can_replace_builtin_character_data_without_a_callback"_test = [] {
        TexSessionState session;
        auto& definitions = session.columnDefinitions();
        definitions.set(u'l', MacroColumnDefinition{u"c", 0.0});
        expect(definitions.find(u'l')->replacement == u"c");
        definitions.set(u'l', MacroColumnDefinition{u"p{#1}", 1.0});
        expect(definitions.size() == 1_u);
        expect(definitions.find(u'l')->replacement == u"p{#1}");
        expect(definitions.find(u'l')->argument_count == 1.0);
    };

    "definition_arity_is_source_numeric_data_without_an_invented_cap"_test = [] {
        TexSessionState session;
        auto& definitions = session.columnDefinitions();
        definitions.set(u'N', MacroColumnDefinition{u"c", 1.0e100});
        definitions.set(u'I', MacroColumnDefinition{u"c", std::numeric_limits<double>::infinity()});
        expect(definitions.find(u'N')->argument_count == 1.0e100);
        expect(std::isinf(definitions.find(u'I')->argument_count));
        expect(definitions.size() == 2_u);
    };
};
