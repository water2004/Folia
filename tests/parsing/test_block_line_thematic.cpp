#include <string>
#include <vector>
#include "support/folia_test.hpp"
import folia.markdown.block_line_recognizer;
using namespace boost::ut;
using folia::detail::BlockLineRecognizer;
suite block_line_thematic_rules=[] {
    "block_line_thematic_accepts_interspersed_marker_whitespace_read_only"_test=[] {
        for (const auto& source:std::vector<std::u32string>{
                U"*\t*\t*\t\n", U" * * * \r\n", U"-\t- -", U"_ _\t_\t", U"***"}) {
            const auto original=source;
            const BlockLineRecognizer recognizer(source);
            std::size_t end=0;
            expect(recognizer.line_is_thematic_break(0,&end));
            expect(end==source.find_first_of(U"\r\n") || end==source.size());
            expect(bool(source==original));
        }
        for (const auto& source:std::vector<std::u32string>{
                U"* *", U"* - *", U"    ***", U"* * * x", U"a***"})
            expect(!BlockLineRecognizer(source).line_is_thematic_break(0));
    };
};
