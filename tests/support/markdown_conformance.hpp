#pragma once

// Shared native source-invariant runner for official Markdown fixtures.
namespace folia::test::conformance {
using namespace boost::ut;

struct MarkdownCase {
    int example;
    std::string_view section;
    std::string_view markdown;
    std::string_view html;
};

inline std::mutex report_mutex;

inline std::string report_path() {
    const auto* const value = std::getenv("FOLIA_MARKDOWN_REPORT");
    return value ? value : "";
}

inline void append_error_report(const MarkdownCase& test_case, std::string_view error) noexcept {
    try {
        const auto path = report_path();
        if (path.empty()) return;
        const std::lock_guard lock(report_mutex);
        std::ofstream output(path, std::ios::binary | std::ios::app);
        if (!output) return;
        output << "{\"example\":" << test_case.example
               << ",\"section\":" << folia::test::commonmark::json_string(test_case.section)
               << ",\"markdown\":" << folia::test::commonmark::json_string(test_case.markdown)
               << ",\"expected_html\":" << folia::test::commonmark::json_string(test_case.html)
               << ",\"error\":" << folia::test::commonmark::json_string(error) << "}\n";
        output.flush();
    } catch (...) {
        // Report output is optional and must not alter test execution.
    }
}

inline void append_case_report(
    const MarkdownCase& test_case,
    bool inline_lossless,
    bool roundtrip,
    const folia::test::commonmark::Projection& projection,
    std::string_view serialized) noexcept {
    try {
        const auto path = report_path();
        if (path.empty()) return;
        const std::lock_guard lock(report_mutex);
        std::ofstream output(path, std::ios::binary | std::ios::app);
        if (!output) return;
        output << "{\"example\":" << test_case.example
               << ",\"section\":" << folia::test::commonmark::json_string(test_case.section)
               << ",\"markdown\":" << folia::test::commonmark::json_string(test_case.markdown)
               << ",\"expected_html\":" << folia::test::commonmark::json_string(test_case.html)
               << ",\"inline_lossless\":" << (inline_lossless ? "true" : "false")
               << ",\"roundtrip\":" << (roundtrip ? "true" : "false")
               << ",\"limitations\":[";
        for (std::size_t index = 0; index < projection.limitations.size(); ++index) {
            if (index) output << ',';
            output << folia::test::commonmark::json_string(projection.limitations[index]);
        }
        output << "]"
               << ",\"projected_html\":" << folia::test::commonmark::json_string(projection.projected_html)
               << ",\"serialized\":" << folia::test::commonmark::json_string(serialized)
               << "}\n";
        output.flush();
    } catch (...) {
        // Report output is optional and must not alter test execution.
    }
}

inline void run_case(const MarkdownCase& test_case, std::string_view suite_name,
    const MarkdownDialect& dialect,
    const folia::test::commonmark::ProjectionOptions& options = {}) {
    const auto case_label = std::string(suite_name) + " example "
        + std::to_string(test_case.example) + " [" + std::string(test_case.section) + "]";
    try {
        const auto parsed = parse_text(1, std::string(test_case.markdown), dialect);
        const auto projection = folia::test::commonmark::project(parsed.document.root, options);
        const bool lossless = folia::test::commonmark::inline_lossless(parsed.document.root);
        const auto serialized = serialize_markdown(parsed.document);
        const bool roundtrip = std::string_view(serialized) == test_case.markdown;

        append_case_report(
            test_case,
            lossless,
            roundtrip,
            projection,
            serialized);

        // The formal Python runner canonicalizes projected_html with the same
        // HTMLParser used for official expected HTML. Keep this native case
        // focused on the independent lossless/save invariants.
        expect(lossless) << case_label << "; inline CST did not preserve source";
        expect(roundtrip) << case_label << "; Markdown serialization changed the source";
    } catch (const std::exception& error) {
        append_error_report(test_case, error.what());
        expect(false) << case_label << "; exception recorded in FOLIA_MARKDOWN_REPORT JSONL";
    } catch (...) {
        append_error_report(test_case, "unknown exception");
        expect(false) << case_label << "; exception recorded in FOLIA_MARKDOWN_REPORT JSONL";
    }
}


inline void register_cases(std::span<const MarkdownCase> cases, std::string_view suite_name,
    const MarkdownDialect& dialect) {
    const auto path = report_path();
    if (!path.empty()) {
        std::ofstream reset(path, std::ios::binary | std::ios::trunc);
    }
    for (const auto& test_case : cases) {
        auto name = std::string(suite_name) + " example "
            + std::to_string(test_case.example) + " [" + std::string(test_case.section) + "]";
        boost::ut::test(std::move(name)) = [test_case, suite_name, dialect] {
            run_case(test_case, suite_name, dialect);
        };
    }
}
} // namespace folia::test::conformance
