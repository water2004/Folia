#include "support/folia_test.hpp"
import folia.mermaid.railroad;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
using Kind = RailroadNodeKind;

suite railroad = [] {
    "mermaid_railroad_ir_metadata_and_expressions"_test = [] {
        const auto parsed = parse_railroad(uR"(railroad-beta
title "Grammar  example" %% ignored
accTitle: Accessible grammar
accDescr {
    First    line.

    Second line.
}
/* comment before the first rule */
start = sequence(terminal("a\n"), choice(nonterminal('next'), special("any character")),
                 optional(terminal("x")), oneOrMore(terminal("y")), zeroOrMore(terminal("z")));
next = sequence(terminal("a%%b"));
next = terminal("duplicate");
)");
        expect(fatal(parsed.has_value()));
        if (!parsed) return;
        expect(parsed->title == u"Grammar example");
        expect(parsed->acc_title == u"Accessible grammar");
        expect(parsed->acc_description == u"First line.\nSecond line.");
        expect(fatal(parsed->rules.size() == 3_u));
        const auto& start = parsed->rules[0].definition;
        expect(start.kind == Kind::sequence);
        expect(fatal(start.children.size() == 5_u));
        expect(start.children[0].kind == Kind::terminal);
        expect(start.children[0].text == u"a\n");
        expect(start.children[1].kind == Kind::choice);
        expect(fatal(start.children[1].children.size() == 2_u));
        expect(start.children[1].children[0].kind == Kind::nonterminal);
        expect(start.children[1].children[0].text == u"next");
        expect(start.children[1].children[1].kind == Kind::special);
        expect(start.children[1].children[1].text == u"any character");
        expect(start.children[2].kind == Kind::optional);
        expect(start.children[3].kind == Kind::repetition);
        expect(start.children[3].min == 1.0);
        expect(start.children[4].kind == Kind::repetition);
        expect(start.children[4].min == 0.0);
        expect(parsed->rules[1].name == parsed->rules[2].name);
        expect(parsed->rules[1].definition.kind == Kind::terminal);
        expect(parsed->rules[1].definition.text == u"a%%b");
    };

    "mermaid_railroad_ebnf_iso_syntax_and_postfixes"_test = [] {
        const auto parsed = parse_railroad(uR"(railroad-ebnf-beta
(* ISO comment *)
value ::= [ "prefix" ], { number | other }, ? any character ?;
number = digit - "0";
other = item?*+;
special = item? quoted?;
)");
        expect(fatal(parsed.has_value()));
        if (!parsed) return;
        expect(fatal(parsed->rules.size() == 4_u));
        const auto& value = parsed->rules[0].definition;
        expect(value.kind == Kind::sequence);
        expect(fatal(value.children.size() == 3_u));
        expect(value.children[0].kind == Kind::optional);
        expect(value.children[1].kind == Kind::repetition);
        expect(fatal(value.children[1].children.size() == 1_u));
        expect(value.children[1].children[0].kind == Kind::choice);
        expect(value.children[2].kind == Kind::special);
        expect(value.children[2].text == u"any character");
        const auto& exception = parsed->rules[1].definition;
        expect(exception.kind == Kind::sequence);
        expect(fatal(exception.children.size() == 3_u));
        expect(exception.children[0].text == u"digit");
        expect(exception.children[1].kind == Kind::terminal);
        expect(exception.children[1].text == u"-");
        expect(exception.children[2].text == u"0");
        const auto& other = parsed->rules[2].definition;
        expect(other.kind == Kind::repetition);
        expect(other.min == 1.0);
        expect(fatal(other.children.size() == 1_u));
        expect(other.children[0].kind == Kind::repetition);
        expect(other.children[0].min == 0.0);
        expect(fatal(other.children[0].children.size() == 1_u));
        expect(other.children[0].children[0].kind == Kind::optional);
        const auto& special = parsed->rules[3].definition;
        expect(fatal(special.children.size() == 2_u));
        expect(special.children[0].kind == Kind::nonterminal);
        expect(special.children[1].kind == Kind::special);
        expect(special.children[1].text == u"quoted");
    };

    "mermaid_railroad_abnf_numeric_values_and_repeat_bounds"_test = [] {
        const auto parsed = parse_railroad(uR"(railroad-abnf-beta
title "A\tB"
; ABNF comment
word = 0*1"prefix" 1*3(ALPHA / %x41-5A) 2DIGIT [ "\n" ] *%b10.01 ;
)");
        expect(fatal(parsed.has_value()));
        if (!parsed) return;
        expect(parsed->title == u"A\\tB");
        expect(fatal(parsed->rules.size() == 1_u));
        const auto& word = parsed->rules[0].definition;
        expect(word.kind == Kind::sequence);
        expect(fatal(word.children.size() == 5_u));
        expect(word.children[0].kind == Kind::optional);
        expect(word.children[1].kind == Kind::repetition);
        expect(word.children[1].min == 1.0);
        expect(word.children[1].max == 3.0);
        expect(fatal(word.children[1].children.size() == 1_u));
        const auto& alternatives = word.children[1].children[0];
        expect(alternatives.kind == Kind::choice);
        expect(fatal(alternatives.children.size() == 2_u));
        expect(alternatives.children[1].kind == Kind::terminal);
        expect(alternatives.children[1].text == u"%x41-5A");
        expect(word.children[2].min == 2.0);
        expect(word.children[2].max == 2.0);
        expect(word.children[3].kind == Kind::optional);
        expect(fatal(word.children[3].children.size() == 1_u));
        expect(word.children[3].children[0].text == u"\\n");
        expect(word.children[4].kind == Kind::repetition);
        expect(word.children[4].min == 0.0);
        expect(fatal(word.children[4].children.size() == 1_u));
        expect(word.children[4].children[0].text == u"%b10.01");
    };

    "mermaid_railroad_peg_lookahead_and_ordered_choice"_test = [] {
        const auto parsed = parse_railroad(uR"(railroad-peg-beta
# PEG comment
word <- &"x" !reserved .? / !(a b) 'y'+;
)");
        expect(fatal(parsed.has_value()));
        if (!parsed) return;
        expect(fatal(parsed->rules.size() == 1_u));
        const auto& word = parsed->rules[0].definition;
        expect(word.kind == Kind::choice);
        expect(fatal(word.children.size() == 2_u));
        const auto& first = word.children[0];
        expect(first.kind == Kind::sequence);
        expect(fatal(first.children.size() == 3_u));
        expect(first.children[0].kind == Kind::special);
        expect(first.children[0].text == u"&\"x\"");
        expect(first.children[1].text == u"!reserved");
        expect(first.children[2].kind == Kind::optional);
        expect(fatal(first.children[2].children.size() == 1_u));
        expect(first.children[2].children[0].kind == Kind::special);
        expect(first.children[2].children[0].text == u".");
        const auto& second = word.children[1];
        expect(fatal(second.children.size() == 2_u));
        expect(second.children[0].text == u"!(...)");
        expect(second.children[1].kind == Kind::repetition);
        expect(second.children[1].min == 1.0);
    };

    "mermaid_railroad_rejects_malformed_or_other_dialects"_test = [] {
        expect(parse_railroad(u"railroad-beta").has_value());
        expect(!parse_railroad(u"RAILROAD-BETA"));
        expect(!parse_railroad(u"railroad-beta;"));
        expect(!parse_railroad(u"railroad-beta\nx = sequence();"));
        expect(!parse_railroad(u"railroad-beta\nx = terminal('x')"));
        expect(!parse_railroad(u"railroad-beta\nsequence = terminal('x');"));
        expect(!parse_railroad(u"railroad-ebnf-beta\nx = a,;"));
        expect(!parse_railroad(u"railroad-ebnf-beta\nx = ? ?;"));
        expect(!parse_railroad(u"railroad-abnf-beta\nx = 'a';"));
        expect(!parse_railroad(u"railroad-abnf-beta\nx = 1 * 2'a';"));
        expect(!parse_railroad(u"railroad-abnf-beta\nx = \"a\"; inline comment"));
        expect(parse_railroad(u"railroad-abnf-beta\nx = \"a\"; inline comment\n;").has_value());
        expect(!parse_railroad(u"railroad-peg-beta\nx = 'a';"));
        expect(!parse_railroad(u"railroad-peg-beta\nx <- 'a'**;"));
        expect(!parse_railroad(u"railroad-peg-beta\nx <- &&'a';"));
    };
};
}
