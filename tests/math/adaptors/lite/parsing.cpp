#include "math/support/lite_test.hpp"

suite lite_source_json_surrogate_witness = [] {
    "Lite test observation JSON preserves lone UTF16 surrogates"_test = [] {
        const std::array<char16_t, 1> lone{static_cast<char16_t>(0xD800)};
        expect(quote(std::u16string_view(lone.data(), lone.size())) == "\"\\ud800\"");
    };
};

suite lite_source_parser_records = [] {
    "Lite parser matches Source trees and serialization"_test = [] {
        lite::LiteAdaptor adaptor;
        std::size_t compared = 0;
        for (const auto &row : liteSourceFixtures()) {
            if (row.kind != LiteSourceFixtureKind::parse)
                continue;
            try {
                const auto document = adaptor.parse(row.html, row.format);
                expect(sourceMatch(parseJson(adaptor, document), row.expectedObservation, row.id,
                                   row.html));
            } catch (const std::range_error &) {
                expect(row.id == u"parse/unknown-numeric-entity-edge")
                    << "Native range error is separate from Source RangeError naming: "
                    << utf16::encode(row.id);
            }
            ++compared;
        }
        expect(compared == 30_u);
    };
};

