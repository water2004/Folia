#include "support/folia_test.hpp"

import folia.mermaid.source_text;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

std::u16string decode(std::string_view value) {
    if (value == "-") return {};
    if (value.size() % 4 != 0) throw std::runtime_error("Invalid UTF-16 fixture field");
    std::u16string result;
    for (std::size_t i = 0; i < value.size(); i += 4) {
        unsigned int unit{};
        const auto parsed = std::from_chars(value.data() + i, value.data() + i + 4, unit, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + i + 4) {
            throw std::runtime_error("Invalid UTF-16 fixture digit");
        }
        result.push_back(static_cast<char16_t>(unit));
    }
    return result;
}

suite source_text = [] {
    "mermaid_preprocess_text_matches_official_typescript"_test = [] {
        const auto fixture_override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{
            fixture_override ? fixture_override : FOLIA_MERMAID_FIXTURE_DIR} / "preprocess-text.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count{};
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            std::vector<std::string_view> fields;
            std::string_view rest{line};
            while (true) {
                const auto separator = rest.find('\t');
                fields.push_back(rest.substr(0, separator));
                if (separator == std::string_view::npos) break;
                rest.remove_prefix(separator + 1);
            }
            expect(fatal(fields.size() == 5_u));
            const auto text = decode(fields[1]);
            expect(bool(cleanup_text(text) == decode(fields[2]))) << fields[0] << " cleanupText";
            expect(bool(remove_directives(text) == decode(fields[3]))) << fields[0] << " removeDirectives";
            expect(bool(remove_directives(cleanup_text(text)) == decode(fields[4]))) << fields[0] << " ordered";
            ++count;
        }
        expect(count == 1221_u);
    };

    "mermaid_preprocess_text_large_inputs_and_owned_results"_test = [] {
        std::u16string tags, directives, expected;
        for (int i = 0; i < 100000; ++i) {
            tags += u"<a";
            directives += u"%%{ ";
            expected += u"x\n";
        }
        expect(bool(cleanup_text(tags) == tags));
        expect(bool(remove_directives(directives) == directives));
        std::u16string lines;
        for (int i = 0; i < 100000; ++i) lines += u"x\r\n";
        expect(bool(cleanup_text(lines) == expected));
        std::u16string result;
        {
            std::u16string temporary = u"%%{init: {}}%%<b a=\"owned\">\r";
            result = remove_directives(cleanup_text(temporary));
        }
        expect(bool(result == u"<b a='owned'>\n"));
    };

    "mermaid_source_text_matches_official_typescript"_test = [] {
        const auto fixture_override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{
            fixture_override ? fixture_override : FOLIA_MERMAID_FIXTURE_DIR} / "source-text.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count{};
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            std::vector<std::string_view> fields;
            std::string_view rest{line};
            while (true) {
                const auto separator = rest.find('\t');
                fields.push_back(rest.substr(0, separator));
                if (separator == std::string_view::npos) break;
                rest.remove_prefix(separator + 1);
            }
            expect(fatal(fields.size() == 7_u)) << "malformed fixture row";
            const auto text = decode(fields[1]);
            expect(bool(strip_any_comments(text) == decode(fields[2]))) << fields[0] << " strip";
            expect(bool(cleanup_comments(text) == decode(fields[3]))) << fields[0] << " cleanup";
            const auto front = match_front_matter(text);
            expect(bool(front) == (fields[4] != "-")) << fields[0] << " match presence";
            if (front && fields[4] != "-") {
                const auto length = std::stoull(std::string{fields[4]});
                expect(front->length == length) << fields[0] << " UTF-16 length";
                expect(bool(front->indent == decode(fields[5]))) << fields[0] << " indent";
                expect(bool(front->body == decode(fields[6]))) << fields[0] << " body";
            }
            ++count;
        }
        expect(count == 862_u) << "all source observations must execute";
    };

    "mermaid_source_text_handles_large_adversarial_documents"_test = [] {
        std::u16string whitespace;
        for (int i = 0; i < 100000; ++i) whitespace += u" \n";
        expect(bool(strip_any_comments(whitespace) == whitespace));
        expect(cleanup_comments(whitespace).empty());
        expect(!match_front_matter(u"---\n" + whitespace));
        std::u16string markers = u"%%";
        for (int i = 0; i < 100000; ++i) markers += u"x%%";
        expect(bool(strip_any_comments(markers) == markers));
        expect(bool(strip_any_comments(markers + u"\n") == u"\n"));
    };

    "mermaid_source_text_returns_owned_results"_test = [] {
        std::optional<FrontMatterMatch> match;
        {
            std::u16string input = u"---\ntitle: owned\n---\npacket\n";
            match = match_front_matter(input);
        }
        expect(fatal(match.has_value()));
        expect(bool(match->body == u"title: owned"));
    };
};
}
