#include "support/folia_test.hpp"
import folia.math.input.tex.tex_error;
import folia.math.util.utf16;
import folia.math.util.js_numeric_input;

using namespace boost::ut;
using folia::math::tex::TexError;

suite tex_error_utf8_tests = [] {
    "what_encodes_valid_utf16_and_replaces_unpaired_surrogates"_test = [] {
        TexError valid("ID", u"A\U0001F600Z");
        expect(valid.message() == u"A\U0001F600Z");
        expect(std::string_view(valid.what()) == std::string_view("A\xF0\x9F\x98\x80Z"));

        std::u16string malformed;
        malformed.push_back(static_cast<char16_t>(0xD800));
        malformed.push_back(u'x');
        malformed.push_back(static_cast<char16_t>(0xDC00));
        TexError replaced("ID", std::move(malformed));
        expect(replaced.message().size() == 3_u);
        expect(static_cast<unsigned>(replaced.message()[0]) == 0xD800);
        expect(static_cast<unsigned>(replaced.message()[2]) == 0xDC00);
        expect(std::string_view(replaced.what()) == std::string_view("\xEF\xBF\xBDx\xEF\xBF\xBD"));
    };
};

namespace {
std::u16string u16Units(std::initializer_list<unsigned> values) {
    std::u16string result;
    result.reserve(values.size());
    for (const auto value : values) result.push_back(static_cast<char16_t>(value));
    return result;
}
std::string byteString(std::initializer_list<unsigned> values) {
    std::string result;
    result.reserve(values.size());
    for (const auto value : values) result.push_back(static_cast<char>(value));
    return result;
}
#include "fixtures/tex-source/input_utf_source_values.inc"
}


suite input_utf_complete_source_values = [] {
    "native_utf16_code_point_iteration_and_text_encoder_match_all_frozen_rows"_test = [] {
        for (const auto& row : inputHostUtf16Cases) {
            for (std::size_t index = 0; index < row.at.size(); ++index)
                expect(static_cast<bool>(folia::math::utf16::codePointAt(row.text, index) == row.at[index]));
            expect(static_cast<bool>(folia::math::utf16::codePoints(row.text) == row.points));
            const auto encoded = folia::math::utf16::encode(row.text);
            expect(encoded.size() == row.utf8.size());
            for (std::size_t index = 0; index < row.utf8.size(); ++index)
                expect(static_cast<unsigned char>(encoded[index]) == row.utf8[index]);
        }
    };

    "native_utf8_decoder_preserves_all_frozen_text_and_byte_offsets"_test = [] {
        for (const auto& row : inputHostDecodeCases) {
            const auto decoded = folia::math::utf16::decode(row.bytes);
            expect(static_cast<bool>(decoded.text == row.text));
            expect(static_cast<bool>(decoded.byteOffsets == row.offsets));
        }
    };

    "native_utf8_decoder_reports_each_frozen_invalid_lead_offset"_test = [] {
        for (const auto& row : inputHostInvalidCases) {
            bool threw = false;
            try {
                (void)folia::math::utf16::decode(row.bytes);
            } catch (const folia::math::utf16::DecodeError& error) {
                threw = true;
                expect(error.byteOffset == row.byteOffset);
            }
            expect(threw);
        }
    };

    "native_parse_int_rejects_unexposed_radix_values"_test = [] {
        for (int radix : {-1, 2, 36, 37}) {
            expect(throws<std::invalid_argument>([&] {
                (void)folia::math::js_input::parseInt(u"12",
                    static_cast<folia::math::js_input::IntegerRadix>(radix));
            }));
        }
    };
};
