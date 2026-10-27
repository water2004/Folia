#include <unordered_map>
#include <unordered_set>
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
#include <span>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"

import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.serializer;
import folia.markdown.utf;
import folia.markdown.markdown_html;
import folia.markdown.dialect;
#include "support/commonmark_projection.hpp"
#include "support/markdown_conformance.hpp"

using folia::MarkdownDialect;
using folia::test::conformance::MarkdownCase;
using folia::test::conformance::report_path;
using folia::test::conformance::run_case;
#include "gfm_cases.inc"

namespace {

std::vector<std::string_view> profile_parts(std::string_view section) {
    const auto end = section.find('|');
    const auto profile = section.substr(0, end);
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= profile.size()) {
        const auto next = profile.find('+', start);
        parts.push_back(profile.substr(start, next == std::string_view::npos
            ? profile.size() - start : next - start));
        if (next == std::string_view::npos) break;
        start = next + 1;
    }
    return parts;
}

MarkdownDialect dialect_for_case(const MarkdownCase& test_case) {
    const auto parts = profile_parts(test_case.section);
    if (parts.front() == "extensions") {
        auto dialect = folia::test::commonmark::gfm_dialect();
        dialect.footnotes = true;
        return dialect;
    }

    auto dialect = folia::test::commonmark::dialect();
    for (const auto part : parts) {
        if (part == "table") {
            dialect.gfm.tables = true;
            dialect.tables = {true, true};
        } else if (part == "tasklist") {
            dialect.gfm.task_lists = true;
        } else if (part == "strikethrough") {
            dialect.gfm.strikethrough = true;
        } else if (part == "autolink") {
            dialect.gfm.autolinks = true;
        } else if (part == "footnotes") {
            dialect.footnotes = true;
        }
    }
    return dialect;
}

folia::test::commonmark::ProjectionOptions projection_options_for_case(
    const MarkdownCase& test_case) {
    const auto parts = profile_parts(test_case.section);
    folia::test::commonmark::ProjectionOptions options;
    options.table_prefer_style_attributes = std::ranges::find(
        parts, "table-style-option") != parts.end();
    options.full_info_string = std::ranges::find(
        parts, "full-info-option") != parts.end();
    return options;
}

void reset_markdown_report() {
    const auto path = report_path();
    if (!path.empty()) {
        std::ofstream reset(path, std::ios::binary | std::ios::trunc);
    }
}

} // namespace

static_assert(std::size(gfm_all_cases) == 1426);

boost::ut::suite gfm_expanded_tests = [] {
    reset_markdown_report();
    for (const auto& test_case : gfm_all_cases) {
        const auto dialect = dialect_for_case(test_case);
        const auto projection_options = projection_options_for_case(test_case);
        const auto name = std::string("GFM ") + std::string(test_case.section);
        boost::ut::test(name) = [test_case, dialect, projection_options] {
            run_case(test_case, "expanded GFM/cmark-gfm", dialect, projection_options);
        };
    }
};
