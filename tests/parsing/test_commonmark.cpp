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

import folia.core.parser;
import folia.core.ast;
import folia.core.block_source;
import folia.core.inline_cst;
import folia.core.inline_document;
import folia.core.serializer;
import folia.core.utf;

import folia.core.markdown_html;
#include "support/commonmark_projection.hpp"

#include "support/markdown_conformance.hpp"
using folia::test::conformance::MarkdownCase;
#include "commonmark_cases.inc"
static_assert(std::size(markdown_cases) == 652);

boost::ut::suite commonmark_official_tests = [] {
    auto dialect = folia::test::commonmark::dialect();
    folia::test::conformance::register_cases(markdown_cases, "CommonMark 0.31.2", dialect);
};
