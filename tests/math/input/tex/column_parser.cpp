#include "math/support/tex_fixture.hpp"
using namespace boost::ut;
using namespace folia::math::tex;
using namespace folia::math::mml;
suite column_parser = [] {
    "column parser real parser templates produce original alignment width spacing and rules"_test =
        [] {
            Fixture f;
            TexParser parser(u"", ParserEnvironmentSeed{}, *f.options);
            ColumnParser columns(f.session.columnDefinitions());
            struct Case {
                std::u16string input, align, width, spacing, lines;
            };
            const std::array cases{
                Case{u"lcr", u"left center right", u"", u"", u""},
                Case{u"p{2em}m{3em}b{4em}", u"left left left", u"2em 3em 4em", u"", u""},
                Case{u"lp{2em}", u"left left", u"auto 2em", u"", u""},
                Case{u"P{2em}M{3em}B{4em}", u"left left left", u"2em 3em 4em", u"", u""},
                Case{u"w{R}{2em}W{c}{3em}", u"right center", u"2em 3em", u"", u""},
                Case{u"|l:r|", u"left right", u"", u"", u"dashed"},
                Case{u"l@{+}r", u"left center right", u"", u"0 0", u""},
                Case{u"ll@{+}l", u"left left center left", u"", u"1em 0 0", u""},
                Case{u"ll|l", u"left left left", u"", u"", u"none solid"},
                Case{u"!{x}l!{y}", u"center left center", u"", u".5em .5em .5em", u""},
                Case{u"*{2}{lc}", u"left center left center", u"", u"", u""},
                Case{u"{{l}}", u"left", u"", u"", u""}};
            for (auto const &c : cases) {
                ArrayItem array(f.factory());
                columns.process(parser, c.input, array);
                expect(attr(array.arraydef, u"columnalign") == c.align);
                expect(attr(array.arraydef, u"columnwidth") == c.width);
                expect(attr(array.arraydef, u"columnspacing") == c.spacing);
                expect(attr(array.arraydef, u"columnlines") == c.lines);
            }
            ArrayItem alias(f.factory());
            ColumnState state(parser, u"l", alias);
            expect(&state.cstart == &alias.cstart);
            expect(&state.cend == &alias.cend);
            expect(&state.cextra == &alias.cextra);
            expect(&state.ralign == &alias.ralign);
            f.session.columnDefinitions().set(u'q', {u">{#1}l", 1});
            columns.process(parser, u"q{prefix}", alias);
            expect(alias.cstart[0] == u"prefix");
            for (auto const &[input, id] : std::array<std::pair<std::u16string, std::string>, 5>{
                     {{u"z", "BadPreamToken"},
                      {u"p", "MissingArgForColumn"},
                      {u"p{2}", "MissingColumnDimOrUnits"},
                      {u"p{2em", "MissingCloseBrace"},
                      {u"*{01}{l}", "ColArgNotNum"}}}) {
                ArrayItem failed(f.factory());
                texError([&] { columns.process(parser, input, failed); }, id);
            }
            parser.mml();
        };
};
