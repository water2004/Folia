#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.parser;
import folia.markdown.serializer;

using namespace folia;
using namespace boost::ut;

namespace {
void expect_source_roundtrip(std::string_view label, std::string const& source) {
    const auto parsed = parse_text(1, source);
    const auto saved = serialize_markdown(parsed.document);
    expect(saved == source) << label << "; saved=" << saved << "; expected=" << source;
    const auto reloaded = parse_text(2, saved);
    expect(serialize_markdown(reloaded.document) == source)
        << label << "; reload save differs";
}
}

suite nested_container_prefix_binding_tests = [] {
    "commonmark_259_nested_quote_list_blank_prefixes_roundtrip_exactly"_test = [] {
        constexpr std::string_view source =
            "   > > 1.  one\n"
            ">>\n"
            ">>     two\n";
        expect_source_roundtrip("CommonMark 259", std::string(source));
    };

    "commonmark_260_nested_quote_blank_and_lazy_prefixes_roundtrip_exactly"_test = [] {
        constexpr std::string_view source =
            ">>- one\n"
            ">>\n"
            "  >  > two\n";
        expect_source_roundtrip("CommonMark 260", std::string(source));
    };
};
