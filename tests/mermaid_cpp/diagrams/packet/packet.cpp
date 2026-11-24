#include "support/folia_test.hpp"

import folia.mermaid.packet;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

struct PacketFixture {
    std::string id;
    std::u16string source;
    bool parse_ok{};
    PacketDocument document;
    PacketConfig config;
    std::optional<bool> layout_ok;
    std::u16string layout_error;
    std::vector<std::vector<PacketWordBlock>> words;
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
        throw std::runtime_error("Invalid Packet fixture column count at line " +
                                 std::to_string(line_number));
    }
}

std::u16string decode_utf16(std::string_view field) {
    if (field == "-") return {};
    if (field.size() % 4 != 0) {
        throw std::runtime_error("Invalid Packet UTF-16 fixture field");
    }
    std::u16string result;
    result.reserve(field.size() / 4);
    for (std::size_t index = 0; index < field.size(); index += 4) {
        std::uint16_t unit{};
        auto const parsed = std::from_chars(field.data() + index,
                                            field.data() + index + 4, unit, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != field.data() + index + 4) {
            throw std::runtime_error("Invalid Packet UTF-16 fixture digit");
        }
        result.push_back(static_cast<char16_t>(unit));
    }
    return result;
}

double parse_number(std::string_view field) {
    double result{};
    auto const parsed = std::from_chars(field.data(), field.data() + field.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size() ||
        !std::isfinite(result)) {
        throw std::runtime_error("Invalid finite Packet fixture number");
    }
    return result;
}

std::optional<double> parse_optional_number(std::string_view field) {
    if (field == "-") return std::nullopt;
    return parse_number(field);
}

std::size_t parse_index(std::string_view field) {
    std::size_t result{};
    auto const parsed = std::from_chars(field.data(), field.data() + field.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size()) {
        throw std::runtime_error("Invalid Packet fixture index");
    }
    return result;
}

bool parse_boolean(std::string_view field) {
    if (field == "0") return false;
    if (field == "1") return true;
    throw std::runtime_error("Invalid Packet fixture boolean");
}

std::vector<PacketFixture> load_packet_fixtures() {
    auto const fixture_override = std::getenv("FOLIA_MERMAID_FIXTURES");
    std::ifstream input{std::filesystem::path{
        fixture_override ? fixture_override : FOLIA_MERMAID_FIXTURE_DIR} / "packet.tsv"};
    if (!input.is_open()) throw std::runtime_error("Cannot open packet.tsv");

    std::vector<PacketFixture> fixtures;
    std::unordered_map<std::string, std::size_t> case_indices;
    std::string line;
    std::size_t line_number{};
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        auto const fields = split_fields(line);
        if (fields[0] == "case") {
            require_fields(fields, 17, line_number);
            PacketFixture fixture;
            fixture.id = fields[1];
            fixture.source = decode_utf16(fields[2]);
            fixture.parse_ok = parse_boolean(fields[3]);
            fixture.document.title = decode_utf16(fields[4]);
            fixture.document.acc_title = decode_utf16(fields[5]);
            fixture.document.acc_description = decode_utf16(fields[6]);
            fixture.config.row_height = parse_number(fields[7]);
            fixture.config.padding_x = parse_number(fields[8]);
            fixture.config.padding_y = parse_number(fields[9]);
            fixture.config.bit_width = parse_number(fields[10]);
            auto const bits_per_row = parse_index(fields[11]);
            if (bits_per_row > std::numeric_limits<std::uint32_t>::max()) {
                throw std::runtime_error("Packet fixture row width exceeds uint32_t");
            }
            fixture.config.bits_per_row = static_cast<std::uint32_t>(bits_per_row);
            fixture.config.show_bits = parse_boolean(fields[12]);
            fixture.config.use_max_width = parse_boolean(fields[13]);
            fixture.config.descending = parse_boolean(fields[14]);
            if (fields[15] != "-") fixture.layout_ok = parse_boolean(fields[15]);
            fixture.layout_error = decode_utf16(fields[16]);
            if (!case_indices.emplace(fixture.id, fixtures.size()).second) {
                throw std::runtime_error("Duplicate Packet fixture id: " + fixture.id);
            }
            fixtures.push_back(std::move(fixture));
        } else if (fields[0] == "block") {
            require_fields(fields, 6, line_number);
            auto& fixture = fixtures.at(case_indices.at(std::string{fields[1]}));
            fixture.document.blocks.push_back({
                parse_optional_number(fields[2]), parse_optional_number(fields[3]),
                parse_optional_number(fields[4]), decode_utf16(fields[5])});
        } else if (fields[0] == "word") {
            require_fields(fields, 8, line_number);
            auto& fixture = fixtures.at(case_indices.at(std::string{fields[1]}));
            auto const row = parse_index(fields[2]);
            auto const block = parse_index(fields[3]);
            if (row == fixture.words.size()) fixture.words.emplace_back();
            if (row >= fixture.words.size() || block != fixture.words[row].size()) {
                throw std::runtime_error("Packet fixture word indices are not contiguous");
            }
            fixture.words[row].push_back({parse_number(fields[4]), parse_number(fields[5]),
                                         parse_number(fields[6]), decode_utf16(fields[7])});
        } else if (fields[0] == "full_case") {
            // Full Mermaid preprocessing and sanitization are a separate API.
            // These observations must not become raw-grammar/layout expectations.
            require_fields(fields, 9, line_number);
        } else if (fields[0] == "full_word") {
            require_fields(fields, 8, line_number);
        } else if (fields[0] == "raw_error") {
            // Langium diagnostics are retained as evidence, not copied verbatim
            // into the native parser's source-offset diagnostic interface.
            require_fields(fields, 3, line_number);
        } else {
            throw std::runtime_error("Unknown Packet fixture record at line " +
                                     std::to_string(line_number));
        }
    }
    return fixtures;
}

void expect_document(PacketDocument const& actual, PacketDocument const& expected,
                     std::string_view id) {
    expect(bool(actual.title == expected.title)) << id << " title";
    expect(bool(actual.acc_title == expected.acc_title)) << id << " accessibility title";
    expect(bool(actual.acc_description == expected.acc_description)) << id << " description";
    expect(actual.blocks.size() == expected.blocks.size()) << id << " AST block count";
    for (std::size_t index = 0; index < std::min(actual.blocks.size(), expected.blocks.size());
         ++index) {
        auto const& observed = actual.blocks[index];
        auto const& source = expected.blocks[index];
        expect(bool(observed.start == source.start)) << id << " AST start " << index;
        expect(bool(observed.end == source.end)) << id << " AST end " << index;
        expect(bool(observed.bits == source.bits)) << id << " AST bits " << index;
        expect(bool(observed.label == source.label)) << id << " AST label " << index;
    }
}

void expect_config(PacketConfig const& actual, PacketConfig const& expected,
                   std::string_view id) {
    expect(actual.row_height == expected.row_height) << id << " row height";
    expect(actual.padding_x == expected.padding_x) << id << " horizontal padding";
    expect(actual.padding_y == expected.padding_y) << id << " vertical padding";
    expect(actual.bit_width == expected.bit_width) << id << " bit width";
    expect(actual.bits_per_row == expected.bits_per_row) << id << " bits per row";
    expect(actual.show_bits == expected.show_bits) << id << " show bits";
    expect(actual.use_max_width == expected.use_max_width) << id << " use maximum width";
    expect(actual.descending == expected.descending) << id << " descending";
}

PacketDocument bit_count_document(double bits) {
    PacketDocument document;
    document.blocks.push_back({std::nullopt, std::nullopt, bits, u"field"});
    return document;
}

suite packet = [] {
    "mermaid_packet_matches_official_raw_grammar_and_layout_observations"_test = [] {
        auto const fixtures = load_packet_fixtures();
        std::size_t accepted_parses{};
        std::size_t rejected_parses{};
        std::size_t compared_layouts{};
        for (auto const& fixture : fixtures) {
            auto const document = parse_packet(fixture.source);
            expect(document.has_value() == fixture.parse_ok) << fixture.id << " parse acceptance";
            if (!document) {
                ++rejected_parses;
                expect(document.error().offset <= fixture.source.size()) << fixture.id << " offset";
                expect(!document.error().message.empty()) << fixture.id << " diagnostic";
                continue;
            }
            ++accepted_parses;
            if (!fixture.parse_ok) continue;
            expect_document(*document, fixture.document, fixture.id);
            if (!fixture.layout_ok) continue;

            ++compared_layouts;
            auto const layout = layout_packet(*document, fixture.config);
            expect(layout.has_value() == *fixture.layout_ok) << fixture.id << " layout acceptance";
            if (!layout) {
                expect(layout.error().offset == std::size_t{0}) << fixture.id << " layout offset";
                if (!*fixture.layout_ok) {
                    expect(bool(layout.error().message == fixture.layout_error))
                        << fixture.id << " semantic diagnostic";
                }
                continue;
            }
            if (!*fixture.layout_ok) continue;
            expect_document(layout->document, fixture.document, fixture.id);
            expect_config(layout->config, fixture.config, fixture.id);
            expect(layout->words.size() == fixture.words.size()) << fixture.id << " row count";
            for (std::size_t row = 0; row < std::min(layout->words.size(), fixture.words.size());
                 ++row) {
                auto const& observed = layout->words[row];
                auto const& source = fixture.words[row];
                expect(observed.size() == source.size()) << fixture.id << " row fields " << row;
                for (std::size_t block = 0; block < std::min(observed.size(), source.size()); ++block) {
                    expect(observed[block].start == source[block].start)
                        << fixture.id << " row start " << row << " field " << block;
                    expect(observed[block].end == source[block].end)
                        << fixture.id << " row end " << row << " field " << block;
                    expect(observed[block].bits == source[block].bits)
                        << fixture.id << " preserved bits " << row << " field " << block;
                    expect(bool(observed[block].label == source[block].label))
                        << fixture.id << " row label " << row << " field " << block;
                }
            }
        }
        expect(fixtures.size() == std::size_t{254}) << "all source observations must execute";
        expect(accepted_parses == std::size_t{182});
        expect(rejected_parses == std::size_t{72});
        expect(compared_layouts == std::size_t{124});
    };

    "mermaid_packet_rejects_invalid_public_layout_configuration"_test = [] {
        auto const document = bit_count_document(1);
        constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
        constexpr auto infinity = std::numeric_limits<double>::infinity();
        for (auto member : {&PacketConfig::row_height, &PacketConfig::bit_width,
                            &PacketConfig::padding_x, &PacketConfig::padding_y}) {
            for (auto invalid : {nan, infinity, -infinity, -1.0}) {
                PacketConfig config;
                config.*member = invalid;
                auto const result = layout_packet(document, config);
                expect(!result.has_value()) << "invalid dimension " << invalid;
                if (!result) {
                    expect(result.error().offset == std::size_t{0});
                    expect(!result.error().message.empty());
                }
            }
        }
        for (auto member : {&PacketConfig::row_height, &PacketConfig::bit_width}) {
            for (auto invalid : {0.0, 0.5}) {
                PacketConfig config;
                config.*member = invalid;
                expect(!layout_packet(document, config).has_value()) << "subunit dimension";
            }
        }
        PacketConfig zero_width;
        zero_width.bits_per_row = 0;
        expect(!layout_packet(document, zero_width).has_value());
    };

    "mermaid_packet_rejects_nonintegral_or_nonfinite_public_bit_fields"_test = [] {
        constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
        constexpr auto infinity = std::numeric_limits<double>::infinity();
        for (auto member : {&PacketBlock::start, &PacketBlock::end, &PacketBlock::bits}) {
            for (auto invalid : {nan, infinity, -infinity, -1.0, 0.5}) {
                auto document = bit_count_document(1);
                document.blocks.front().*member = invalid;
                auto const result = layout_packet(document);
                expect(!result.has_value()) << "invalid bit field " << invalid;
                if (!result) {
                    expect(result.error().offset == std::size_t{0});
                    expect(!result.error().message.empty());
                }
            }
        }
        expect(!layout_packet(bit_count_document(0)).has_value());
    };

    "mermaid_packet_row_budget_accepts_the_boundary_and_rejects_partial_results"_test = [] {
        PacketConfig config;
        config.bits_per_row = 1;
        auto const boundary = layout_packet(bit_count_document(10'000), config);
        expect(fatal(boundary.has_value()));
        expect(boundary->words.size() == std::size_t{10'000});
        expect(fatal(!boundary->words.empty()));
        expect(fatal(boundary->words.front().size() == std::size_t{1}));
        expect(fatal(boundary->words.back().size() == std::size_t{1}));
        expect(boundary->words.front().front().start == 0.0);
        expect(boundary->words.front().front().end == 0.0);
        expect(boundary->words.back().front().start == 9'999.0);
        expect(boundary->words.back().front().end == 9'999.0);

        expect(!layout_packet(bit_count_document(10'001), config).has_value());
        auto document = bit_count_document(1);
        document.blocks.push_back({std::nullopt, std::nullopt, 10'000, u"oversized remainder"});
        auto const partial = layout_packet(document, config);
        expect(!partial.has_value()) << "completed earlier fields must not escape as partial output";
        if (!partial) {
            expect(partial.error().offset == std::size_t{0});
            expect(!partial.error().message.empty());
        }
    };

    "mermaid_packet_preserves_fractional_dimensions_and_large_row_widths"_test = [] {
        PacketConfig config;
        config.row_height = 2.5;
        config.bit_width = 1.25;
        config.padding_x = 0;
        config.padding_y = 0.75;
        config.bits_per_row = std::numeric_limits<std::uint32_t>::max();
        config.show_bits = false;
        config.use_max_width = false;
        config.descending = true;
        auto const width = static_cast<double>(config.bits_per_row);
        auto const result = layout_packet(bit_count_document(width + 1), config);
        expect(fatal(result.has_value()));
        expect_config(result->config, config, "public configuration");
        expect(fatal(result->words.size() == std::size_t{2}));
        expect(fatal(result->words[0].size() == std::size_t{1}));
        expect(fatal(result->words[1].size() == std::size_t{1}));
        expect(result->words[0][0].start == 0.0);
        expect(result->words[0][0].end == width - 1);
        expect(result->words[1][0].start == width);
        expect(result->words[1][0].end == width);
    };

    "mermaid_packet_results_own_source_metadata_labels_and_configuration"_test = [] {
        std::optional<PacketLayout> retained;
        std::u16string expected_label = u"owned\nlabel ";
        expected_label.push_back(u'\0');
        expected_label += u" \u03a9 \U0001f600";
        {
            std::u16string source = u"packet\ntitle Owned\naccTitle: Accessible\n"
                u"accDescr: Description\n0-3: \"owned\\nlabel \\0 \u03a9 \U0001f600\"";
            auto document = parse_packet(source);
            expect(fatal(document.has_value()));
            source.assign(source.size(), u'x');
            PacketConfig config;
            config.bits_per_row = 2;
            auto layout = layout_packet(*document, config);
            expect(fatal(layout.has_value()));
            retained = std::move(*layout);
            document->title.clear();
            document->blocks.clear();
            config.bits_per_row = 32;
        }
        expect(fatal(retained.has_value()));
        expect(bool(retained->document.title == u"Owned"));
        expect(bool(retained->document.acc_title == u"Accessible"));
        expect(bool(retained->document.acc_description == u"Description"));
        expect(fatal(retained->document.blocks.size() == std::size_t{1}));
        expect(bool(retained->document.blocks.front().label == expected_label));
        expect(retained->config.bits_per_row == std::uint32_t{2});
        expect(fatal(retained->words.size() == std::size_t{2}));
        for (auto const& word : retained->words) {
            expect(fatal(word.size() == std::size_t{1}));
            expect(bool(word.front().label == expected_label));
        }
    };
};
}
