export const cases = [
  ['ir-simple', "railroad-beta\ndigit = terminal('0');\n"],
  ['ir-nested', `railroad-beta
accTitle: JSON values
accDescr: Recursive grammar
object = sequence(terminal('{'), optional(sequence(nonterminal('member'), zeroOrMore(sequence(terminal(','), nonterminal('member'))))), terminal('}'));
member = sequence(nonterminal('string'), terminal(':'), choice(nonterminal('number'), terminal('true'), special('anything')));
`],
  ['ebnf', `railroad-ebnf-beta
identifier = letter, { letter | digit | '_' };
letter = ? Unicode letter ? ;
digit = '0' | '1' | '2';
`],
  ['abnf', `railroad-abnf-beta
scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) ;
digits = 1*3DIGIT ;
`],
  ['peg', `railroad-peg-beta
Expression <- Term (('+' / '-') Term)* ;
Term <- Digit+ ('x' Digit)? ;
Digit <- '0' / '1' / '2' ;
`],
  ['empty', 'railroad-beta\n'],
  ['entities', "railroad-beta\nchars = sequence(terminal('#amp;'), terminal('#lt;'));\n"],
  ['configured', `---
config:
  railroad:
    padding: 7
    verticalSeparation: 13
    horizontalSeparation: 17
    arcRadius: 9
    markerRadius: 3
    strokeWidth: 1.5
    fontSize: 20
    fontFamily: Arial
    terminalFill: '#abcdef'
    lineColor: purple
    useMaxWidth: false
---
railroad-beta
item = sequence(terminal(''), terminal('é  ñ'), optional(nonterminal('item')));
`],
];
