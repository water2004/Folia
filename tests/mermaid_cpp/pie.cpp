#include "support/folia_test.hpp"

import folia.mermaid.pie;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

struct PieFixture {
    std::string id;
    std::u16string source;
    bool raw_ok{};
    bool native_finite{};
    PieDocument document;
    std::optional<bool> db_ok;
    std::u16string db_error;
    std::vector<PieSection> db_sections;
};

std::vector<std::string_view> split_fields(std::string_view line) {
    std::vector<std::string_view> fields;
    for (;;) {
        auto const separator = line.find('\t');
        fields.push_back(line.substr(0, separator));
        if (separator == std::string_view::npos) return fields;
        line.remove_prefix(separator + 1);
    }
}

void require_fields(std::span<std::string_view const> fields, std::size_t count,
                    std::size_t line_number) {
    if (fields.size() != count) {
        throw std::runtime_error("Invalid Pie fixture column count at line " +
                                 std::to_string(line_number));
    }
}

std::u16string decode_utf16(std::string_view field) {
    if (field == "-") return {};
    if (field.size() % 4 != 0) throw std::runtime_error("Invalid Pie UTF-16 fixture field");
    std::u16string result;
    result.reserve(field.size() / 4);
    for (std::size_t index = 0; index < field.size(); index += 4) {
        std::uint16_t unit{};
        auto const parsed = std::from_chars(field.data() + index,
                                            field.data() + index + 4, unit, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != field.data() + index + 4) {
            throw std::runtime_error("Invalid Pie UTF-16 fixture digit");
        }
        result.push_back(static_cast<char16_t>(unit));
    }
    return result;
}

double parse_number(std::string_view field) {
    // The official grammar accepts overflowing literals. Preserve this evidence
    // in the fixture, then explicitly test the native finite-value boundary.
    if (field == "Infinity") return std::numeric_limits<double>::infinity();
    if (field == "-Infinity") return -std::numeric_limits<double>::infinity();
    double result{};
    auto const parsed = std::from_chars(field.data(), field.data() + field.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size() ||
        !std::isfinite(result)) {
        throw std::runtime_error("Invalid Pie fixture number");
    }
    return result;
}

bool parse_boolean(std::string_view field) {
    if (field == "0") return false;
    if (field == "1") return true;
    throw std::runtime_error("Invalid Pie fixture boolean");
}

std::vector<PieFixture> load_pie_fixtures() {
    auto const fixture_override = std::getenv("FOLIA_MERMAID_FIXTURES");
    std::ifstream input{std::filesystem::path{
        fixture_override ? fixture_override : FOLIA_MERMAID_FIXTURE_DIR} / "pie.tsv"};
    if (!input.is_open()) throw std::runtime_error("Cannot open pie.tsv");
    std::vector<PieFixture> fixtures;
    std::unordered_map<std::string, std::size_t> case_indices;
    std::string line;
    std::size_t line_number{};
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        auto const fields = split_fields(line);
        if (fields[0] == "case") {
            require_fields(fields, 11, line_number);
            PieFixture fixture;
            fixture.id = fields[1];
            fixture.source = decode_utf16(fields[2]);
            fixture.raw_ok = parse_boolean(fields[3]);
            fixture.native_finite = parse_boolean(fields[4]);
            fixture.document.title = decode_utf16(fields[5]);
            fixture.document.acc_title = decode_utf16(fields[6]);
            fixture.document.acc_description = decode_utf16(fields[7]);
            fixture.document.show_data = parse_boolean(fields[8]);
            if (fields[9] != "-") fixture.db_ok = parse_boolean(fields[9]);
            fixture.db_error = decode_utf16(fields[10]);
            if (!case_indices.emplace(fixture.id, fixtures.size()).second) {
                throw std::runtime_error("Duplicate Pie fixture id: " + fixture.id);
            }
            fixtures.push_back(std::move(fixture));
        } else if (fields[0] == "section" || fields[0] == "db_section") {
            require_fields(fields, 4, line_number);
            auto& fixture = fixtures.at(case_indices.at(std::string{fields[1]}));
            auto& sections = fields[0] == "section" ? fixture.document.sections : fixture.db_sections;
            sections.push_back({decode_utf16(fields[2]), parse_number(fields[3])});
        } else if (fields[0] == "full_case") {
            // The complete API also preprocesses and sanitizes. These retained
            // observations never become raw parse/populate expectations.
            require_fields(fields, 8, line_number);
        } else if (fields[0] == "full_section") {
            require_fields(fields, 4, line_number);
        } else if (fields[0] == "raw_error") {
            // Langium diagnostics are retained as evidence. Native diagnostics
            // promise a source offset and message, not Langium's exact wording.
            require_fields(fields, 3, line_number);
        } else {
            throw std::runtime_error("Unknown Pie fixture record at line " +
                                     std::to_string(line_number));
        }
    }
    return fixtures;
}

void expect_sections(std::span<PieSection const> actual, std::span<PieSection const> expected,
                     std::string_view id) {
    expect(actual.size() == expected.size()) << id << " section count";
    for (std::size_t index = 0; index < std::min(actual.size(), expected.size()); ++index) {
        expect(bool(actual[index].label == expected[index].label)) << id << " label " << index;
        expect(actual[index].value == expected[index].value) << id << " value " << index;
        if (actual[index].value == 0.0 && expected[index].value == 0.0) {
            expect(std::signbit(actual[index].value) == std::signbit(expected[index].value))
                << id << " signed zero " << index;
        }
    }
}

void expect_metadata(PieDocument const& actual, PieDocument const& expected,
                     std::string_view id) {
    expect(bool(actual.title == expected.title)) << id << " title";
    expect(bool(actual.acc_title == expected.acc_title)) << id << " accessibility title";
    expect(bool(actual.acc_description == expected.acc_description)) << id << " description";
    expect(actual.show_data == expected.show_data) << id << " show data";
}

suite pie_tests = [] {
    "mermaid_pie_matches_pinned_official_raw_parser_and_comparable_db_observations"_test = [] {
        auto const fixtures = load_pie_fixtures();
        std::size_t accepted_parses{};
        std::size_t rejected_syntax{};
        std::size_t safety_rejections{};
        std::size_t accepted_databases{};
        std::size_t rejected_databases{};
        for (auto const& fixture : fixtures) {
            auto const document = parse_pie(fixture.source);
            auto const native_ok = fixture.raw_ok && fixture.native_finite;
            expect(document.has_value() == native_ok) << fixture.id << " parse acceptance";
            if (!document) {
                if (fixture.raw_ok && !fixture.native_finite) ++safety_rejections;
                else ++rejected_syntax;
                expect(document.error().offset <= fixture.source.size()) << fixture.id << " offset";
                expect(!document.error().message.empty()) << fixture.id << " diagnostic";
                continue;
            }
            ++accepted_parses;
            if (!native_ok) continue;
            expect_metadata(*document, fixture.document, fixture.id);
            expect_sections(document->sections, fixture.document.sections, fixture.id);
            if (!fixture.db_ok) continue;
            auto const populated = populate_pie(*document);
            expect(populated.has_value() == *fixture.db_ok) << fixture.id << " DB acceptance";
            if (!populated) {
                ++rejected_databases;
                expect(populated.error().offset == std::size_t{0}) << fixture.id << " DB offset";
                if (!*fixture.db_ok) {
                    expect(bool(populated.error().message == fixture.db_error))
                        << fixture.id << " DB semantic diagnostic";
                }
                continue;
            }
            ++accepted_databases;
            if (!*fixture.db_ok) continue;
            expect_metadata(*populated, fixture.document, fixture.id);
            expect_sections(populated->sections, fixture.db_sections, fixture.id);
        }
        expect(fixtures.size() == std::size_t{291}) << "all official observations execute";
        expect(accepted_parses == std::size_t{187});
        expect(rejected_syntax == std::size_t{101});
        expect(safety_rejections == std::size_t{3});
        expect(accepted_databases == std::size_t{113});
        expect(rejected_databases == std::size_t{16});
    };

    "mermaid_pie_rejects_nonfinite_public_values_even_after_a_duplicate_label"_test = [] {
        constexpr auto infinity = std::numeric_limits<double>::infinity();
        constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
        for (auto const invalid : {nan, infinity, -infinity}) {
            for (auto const duplicate : {false, true}) {
                PieDocument document;
                document.sections.push_back({u"first", 1});
                document.sections.push_back({duplicate ? u"first" : u"second", invalid});
                auto const result = populate_pie(document);
                expect(!result.has_value()) << "invalid public value " << invalid;
                if (!result) {
                    expect(result.error().offset == std::size_t{0});
                    expect(!result.error().message.empty());
                }
                expect(document.sections.size() == std::size_t{2});
                expect(document.sections.front().value == 1.0);
                if (std::isnan(invalid)) expect(std::isnan(document.sections.back().value));
                else expect(document.sections.back().value == invalid);
            }
        }
    };

    "mermaid_pie_validates_every_public_section_before_deduplication"_test = [] {
        PieDocument document;
        document.sections = {{u"first", 0}, {u"second", 2}, {u"first", -1}};
        auto const rejected = populate_pie(document);
        expect(!rejected.has_value()) << "negative duplicate still fails";
        if (!rejected) {
            expect(bool(rejected.error().message ==
                u"\"first\" has invalid value: -1. Negative values are not allowed in pie charts. All slice values must be >= 0."));
        }
        document.sections.back().value = 3;
        document.sections.push_back({u"second", -0.0});
        document.sections.push_back({u"third", -0.0});
        auto const populated = populate_pie(document);
        expect(fatal(populated.has_value()));
        std::vector<PieSection> const expected{{u"first", 0}, {u"second", 2}, {u"third", -0.0}};
        expect_sections(populated->sections, expected, "public insertion order");
        expect(document.sections.size() == std::size_t{5}) << "input remains intact";
    };

    "mermaid_pie_raw_results_preserve_markup_and_own_all_text"_test = [] {
        std::optional<PieDocument> retained;
        std::u16string expected_label = u"owned\nlabel ";
        expected_label.push_back(u'\0');
        expected_label += u" \u03a9 \U0001f600";
        {
            std::u16string source = u"pie showData\ntitle <b>Owned</b>\naccTitle: Accessible\n"
                u"accDescr: Description\n\"owned\\nlabel \\0 \u03a9 \U0001f600\":-0";
            auto document = parse_pie(source);
            expect(fatal(document.has_value()));
            source.assign(source.size(), u'x');
            auto populated = populate_pie(*document);
            expect(fatal(populated.has_value()));
            retained = std::move(*populated);
            document->title.clear();
            document->sections.clear();
        }
        expect(fatal(retained.has_value()));
        expect(bool(retained->title == u"<b>Owned</b>"));
        expect(bool(retained->acc_title == u"Accessible"));
        expect(bool(retained->acc_description == u"Description"));
        expect(retained->show_data);
        expect(fatal(retained->sections.size() == std::size_t{1}));
        expect(bool(retained->sections.front().label == expected_label));
        expect(retained->sections.front().value == 0.0);
        expect(std::signbit(retained->sections.front().value));
    };
};
}
