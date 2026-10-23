#include <algorithm>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"

import folia.core.parser;
import folia.core.ast;
import folia.core.block_source;
import folia.core.inline_cst;
import folia.core.inline_document;
import folia.core.serializer;
import folia.core.utf;

import folia.core.markdown_html;
#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

struct CommonMarkCase {
    int example;
    std::string_view section;
    std::string_view markdown;
    std::string_view html;
    std::string_view expected_tree;
    std::string_view relaxed_tree;
};

#include "commonmark_cases.inc"

static_assert(std::size(commonmark_cases) == 652,
              "the official CommonMark fixture must contain all 652 examples");

namespace {

std::mutex commonmark_report_mutex;

std::string commonmark_report_path() {
    const auto* const value = std::getenv("FOLIA_COMMONMARK_REPORT");
    return value ? value : "";
}

void append_error_report(const CommonMarkCase& test_case, std::string_view error) noexcept {
    try {
        const auto path = commonmark_report_path();
        if (path.empty()) return;
        const std::lock_guard lock(commonmark_report_mutex);
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

void append_case_report(
    const CommonMarkCase& test_case,
    bool inline_lossless,
    bool roundtrip,
    const folia::test::commonmark::Projection& projection,
    std::string_view serialized) noexcept {
    try {
        const auto path = commonmark_report_path();
        if (path.empty()) return;
        const std::lock_guard lock(commonmark_report_mutex);
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

void run_commonmark_case(const CommonMarkCase& test_case) {
    const auto case_label = std::string("CommonMark 0.31.2 example ")
        + std::to_string(test_case.example) + " [" + std::string(test_case.section) + "]";
    try {
        const auto parsed = parse_text(1, std::string(test_case.markdown), folia::test::commonmark::dialect());
        const auto projection = folia::test::commonmark::project(parsed.document.root);
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
        expect(false) << case_label << "; exception recorded in FOLIA_COMMONMARK_REPORT JSONL";
    } catch (...) {
        append_error_report(test_case, "unknown exception");
        expect(false) << case_label << "; exception recorded in FOLIA_COMMONMARK_REPORT JSONL";
    }
}

} // namespace

suite commonmark_official_tests = [] {
    const auto report_path = commonmark_report_path();
    if (!report_path.empty()) {
        try {
            std::ofstream reset(report_path, std::ios::binary | std::ios::trunc);
        } catch (...) {
            // An unavailable optional report must not prevent registration or execution.
        }
    }

    for (const auto& test_case : commonmark_cases) {
        auto name = std::string("CommonMark 0.31.2 example ")
            + std::to_string(test_case.example) + " [" + std::string(test_case.section) + "]";
        boost::ut::test(std::move(name)) = [test_case] {
            run_commonmark_case(test_case);
        };
    }
};
