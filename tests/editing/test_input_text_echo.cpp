#include "support/folia_test.hpp"

import folia.core.input_text_echo;
import folia.core.text_navigation;
import folia.markdown.utf;

using namespace boost::ut;
using namespace folia;

suite input_text_echo_tests = [] {

"text input echo consumes multi-scalar commits one character event at a time"_test = [] {
    InputTextEcho echo;
    const InputTextEcho::TimePoint start{};
    for (auto text : {U"hello", U"你好世界", U"a\U0001f600b", U"e\u0301", U"\U0001f469\u200d\U0001f4bb"}) {
        const std::u32string committed{text};
        echo.record(committed, 7, start);
        for (auto character : committed)
            expect(echo.consume(std::u32string(1, character), 7, start + std::chrono::milliseconds(1)));
        expect(!echo.consume(committed, 7, start + std::chrono::milliseconds(2)));
    }
    echo.record(U"alpha beta", 7, start);
    expect(echo.consume(U"alpha", 7, start));
    expect(echo.consume(U" ", 7, start));
    expect(echo.consume(U"beta", 7, start));
    expect(!echo.consume(U"beta", 7, start));
};

"text input echo never swallows a fresh key or mismatched text revision"_test = [] {
    InputTextEcho echo;
    const InputTextEcho::TimePoint start{};
    echo.record(U"same", 1, start);
    echo.clear(); // A new physical key owns its following text event.
    expect(!echo.consume(U"s", 1, start));
    echo.record(U"same", 1, start);
    expect(!echo.consume(U"s", 2, start));
    expect(!echo.consume(U"s", 1, start));
    echo.record(U"same", 1, start);
    expect(!echo.consume(U"different", 1, start));
    expect(!echo.consume(U"s", 1, start));
    echo.record(U"same", 1, start);
    expect(echo.consume(U"s", 1, start + std::chrono::milliseconds(249)));
    expect(!echo.consume(U"a", 1, start + std::chrono::milliseconds(250)));
    echo.record(U"old", 1, start);
    echo.record(U"new", 2, start);
    expect(echo.consume(U"n", 2, start));
    expect(echo.consume(U"ew", 2, start));
    echo.record({}, 1, start);
    expect(!echo.consume({}, 1, start));
    expect(!echo.consume(U"x", 1, start));
};

"word selection covers whitespace and punctuation with whole Unicode clusters"_test = [] {
    auto selected = [](std::u32string_view text, std::size_t offset) {
        auto range = text_word_selection_range(text, offset);
        expect(fatal(range.has_value()));
        expect(!range->empty() && range->valid_for(text.size()));
        expect(is_grapheme_boundary_char(text, range->start));
        expect(is_grapheme_boundary_char(text, range->end));
        return std::u32string{text.substr(range->start, range->length())};
    };
    expect(!text_word_selection_range({}, 0));
    expect(selected(U"alpha beta", 2) == U"alpha");
    expect(selected(U"alpha beta", 5) == U" ");
    expect(selected(U"alpha beta", 10) == U"beta");
    expect(selected(U"alpha beta", 999) == U"beta");
    expect(selected(U"one   two", 5) == U"   ");
    expect(selected(U"a::b", 1) == U"::");
    expect(selected(U"a::b", 2) == U"::");
    expect(selected(U"你好，世界", 2) == U"，");
    expect(selected(U"hello_world42!", 6) == U"hello_world42");
    expect(selected(U"e\u0301 xyz", 1) == U"e\u0301");
    const std::u32string emoji = U"a\U0001f469\u200d\U0001f4bb b";
    for (std::size_t i = 1; i < 4; ++i)
        expect(selected(emoji, i) == U"\U0001f469\u200d\U0001f4bb");
    expect(selected(U"a\r\nb", 2) == U"\r\n");
};

};
