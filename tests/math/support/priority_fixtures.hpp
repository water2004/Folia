#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct SourcePrioritySnapshot {
    std::vector<double> items;
    std::vector<double> priorities;
};

inline std::vector<double> priorityValues(std::string_view field, std::size_t count) {
    std::vector<double> result;
    if (!field.empty()) {
        std::size_t start{};
        for (;;) {
            const auto end = field.find(',', start);
            result.push_back(std::bit_cast<double>(binary64(field.substr(
                start, end == std::string_view::npos ? end : end - start))));
            if (end == std::string_view::npos) break;
            start = end + 1;
        }
    }
    if (result.size() != count) throw std::runtime_error("Priority reference list count changed");
    return result;
}

inline const auto& sourcePrioritySnapshots() {
    static const auto result = records<SourcePrioritySnapshot>(
        "math/fixtures/util/priority/cases.tsv", 4, 15, [](const auto& r) {
            return SourcePrioritySnapshot{priorityValues(r[1], integer<std::size_t>(r[0])),
                priorityValues(r[3], integer<std::size_t>(r[2]))};
        });
    return result;
}

} // namespace math_test::fixtures
