#include "support/folia_test.hpp"
#include "support/fixture.hpp"

namespace {
using namespace boost::ut;
using namespace folia::test::data;

suite fixture_data_tests = [] {
    "fixture_data_preserves_bytes_and_utf16_code_units"_test = [] {
        const auto bytes = decode_bytes(R"(a\x00\xff\n\r\t\\)");
        expect(bytes.size() == 7u);
        expect(bytes[1] == '\0');
        expect(static_cast<unsigned char>(bytes[2]) == 0xff);
        expect(bytes.substr(3) == "\n\r\t\\");
        const auto units = decode_utf16(R"(\ud800x\udfff\ud83d\ude00)");
        expect(units.size() == 5u);
        expect(units[0] == char16_t{0xd800});
        expect(units[2] == char16_t{0xdfff});
        expect(units[3] == char16_t{0xd83d});
        expect(units[4] == char16_t{0xde00});
    };
    "fixture_data_decodes_scalar_and_literal_unicode_exactly"_test = [] {
        expect(bool(decode_utf32(R"(\u96ea\ud83d\ude00\U0001f600)") == U"雪😀😀"));
        expect(bool(decode_utf32("雪😀") == U"雪😀"));
        expect(bool(decode_utf32(R"(\x00)") == std::u32string(1, U'\0')));
        for (const auto source : {R"(\ud800)", R"(\udfff)", R"(\uD800x)", R"(\U00110000)"}) {
            bool rejected{};
            try { (void)decode_utf32(source); } catch (const std::runtime_error&) { rejected = true; }
            expect(rejected) << source;
        }
    };
    "fixture_data_rejects_truncated_and_invalid_encodings"_test = [] {
        for (const std::string source : {"\\", "\\q", "\\x0", "\\u00x1", "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80"}) {
            bool rejected{};
            try { (void)decode_utf16(source); } catch (const std::runtime_error&) { rejected = true; }
            expect(rejected) << source;
        }
        bool rejected{};
        try { (void)decode_bytes(R"(\u0100)"); } catch (const std::runtime_error&) { rejected = true; }
        expect(rejected);
    };
    "fixture_tables_preserve_empty_cells_and_report_corruption"_test = [] {
        const auto rows = read_tsv("fixtures/harness/valid.tsv", 3);
        expect(rows.size() == 2u);
        expect(rows[0][1].empty());
        expect(rows[0][2] == "last");
        expect(rows[1][2].empty());
        for (const auto file : {"fixtures/harness/empty.tsv", "fixtures/harness/wrong_columns.tsv", "fixtures/harness/missing.tsv"}) {
            bool rejected{};
            try { (void)read_tsv(file, 3); } catch (const std::runtime_error& error) {
                rejected = std::string_view(error.what()).find(file) != std::string_view::npos;
            }
            expect(rejected) << file;
        }
    };
    "fixture_paths_reject_escape_from_the_data_root"_test = [] {
        for (const auto relative : {"", "../README.md", "fixtures/../../README.md", "/absolute.tsv"}) {
            bool rejected{};
            try { (void)path(relative); } catch (const std::invalid_argument&) { rejected = true; }
            expect(rejected) << relative;
        }
        expect(path("fixtures/harness/valid.tsv").is_absolute());
    };
};
} // namespace
