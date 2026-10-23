#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "md4c-html.h"
#include "support/folia_test.hpp"

using namespace boost::ut;

struct MarkdownCase {
    int example;
    std::string_view section;
    std::string_view markdown;
    std::string_view html;
};

#include "commonmark_cases.inc"
#include "md4c_failed_examples.inc"

static_assert(std::size(markdown_cases) == 652,
              "the official CommonMark fixture must contain all 652 examples");

namespace {

std::mutex md4c_report_mutex;

struct HtmlOutput {
    std::string text;
    bool failed = false;
};

void collect_html(const MD_CHAR* chunk, MD_SIZE size, void* userdata) noexcept {
    auto& output = *static_cast<HtmlOutput*>(userdata);
    if (output.failed) return;
    try {
        output.text.append(chunk, static_cast<std::size_t>(size));
    } catch (...) {
        output.failed = true;
    }
}

std::string json_string(std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (ch < 0x20) {
                    escaped += "\\u00";
                    escaped.push_back(hex[ch >> 4]);
                    escaped.push_back(hex[ch & 0x0f]);
                } else {
                    escaped.push_back(static_cast<char>(ch));
                }
        }
    }
    escaped.push_back('"');
    return escaped;
}

std::string report_path() {
    const auto* const path = std::getenv("FOLIA_MD4C_REPORT");
    return path ? path : "";
}

const MarkdownCase* find_case(int example) {
    for (const auto& test_case : markdown_cases) {
        if (test_case.example == example) return &test_case;
    }
    return nullptr;
}

void append_report(
    const MarkdownCase& test_case,
    bool exact_html_match,
    std::string_view actual_html,
    int return_code) noexcept {
    try {
        const auto path = report_path();
        if (path.empty()) return;
        const std::lock_guard lock(md4c_report_mutex);
        std::ofstream output(path, std::ios::binary | std::ios::app);
        if (!output) return;
        output << "{\"example\":" << test_case.example
               << ",\"section\":" << json_string(test_case.section)
               << ",\"match\":" << (exact_html_match ? "true" : "false")
               << ",\"expected_html\":" << json_string(test_case.html)
               << ",\"actual_html\":" << json_string(actual_html)
               << ",\"return_code\":" << return_code << "}\n";
        output.flush();
    } catch (...) {
        // The optional report must not change the reference test result.
    }
}

void run_md4c_case(const MarkdownCase& test_case) {
    std::string markdown(test_case.markdown);
    HtmlOutput output;
    const int return_code = md_html(
        markdown.data(),
        static_cast<MD_SIZE>(markdown.size()),
        collect_html,
        &output,
        MD_DIALECT_COMMONMARK,
        MD_HTML_FLAG_XHTML);
    const bool exact_html_match = !output.failed
        && std::string_view(output.text) == test_case.html;

    append_report(test_case, exact_html_match, output.text, return_code);

    const auto label = std::string("md4c CommonMark example ")
        + std::to_string(test_case.example) + " [" + std::string(test_case.section) + "]";
    expect(return_code == 0) << label << "; md_html returned " << return_code;
    expect(!output.failed) << label << "; output collection failed";
    expect(exact_html_match) << label << "; see FOLIA_MD4C_REPORT JSONL for exact HTML";
}

} // namespace

suite md4c_reference_tests = [] {
    const auto path = report_path();
    if (!path.empty()) {
        try {
            std::ofstream reset(path, std::ios::binary | std::ios::trunc);
        } catch (...) {
            // An unavailable optional report must not prevent test execution.
        }
    }

    for (const int example : md4c_failed_examples) {
        const auto* const test_case = find_case(example);
        auto name = std::string("md4c CommonMark example ")
            + std::to_string(example);
        if (test_case) {
            name += " [";
            name += test_case->section;
            name += ']';
            test(std::move(name)) = [test_case] {
                run_md4c_case(*test_case);
            };
        } else {
            name += " [missing official fixture]";
            test(std::move(name)) = [example] {
                expect(false) << "md4c CommonMark example " << example
                              << "; missing from commonmark_cases.inc";
            };
        }
    }
};
