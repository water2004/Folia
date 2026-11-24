#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct CommonSceneCase {
    std::string name, tex;
    bool display;
    double em, ex;
    int width;
    std::string tag_side, svg, root_width, root_height, root_view_box, geometry;
    std::string error_name, error_message;
};

struct CanonicalSceneMetrics { double width, height, depth; };
struct CanonicalSceneMode {
    bool display{}, initial_allow_tex_html{};
    CanonicalSceneMetrics metrics{};
    std::string error_name, error_message, svg;
    std::vector<std::string> local_cache_prefixes;
    std::size_t listener_count{};
    bool listener_dom_unchanged{};
};
struct CanonicalSceneCase {
    std::string name, tex, tag_side;
    bool requested_display;
    double em, ex, width;
    std::array<CanonicalSceneMode, 2> modes;
};

inline double real(std::string_view value) {
    return std::bit_cast<double>(binary64(value));
}

inline std::vector<std::string> strings(std::string_view value, std::size_t count) {
    std::vector<std::string> result;
    if (!count && !value.empty())
        throw std::runtime_error("Nonempty string list with zero count in math fixture");
    for (std::size_t index{}; index < count; ++index) {
        const auto end = value.find('|');
        if ((end == std::string_view::npos) != (index + 1 == count))
            throw std::runtime_error("Incorrect string list count in math fixture");
        result.push_back(bytes(value.substr(0, end)));
        if (end != std::string_view::npos) value.remove_prefix(end + 1);
    }
    return result;
}

inline const std::vector<CommonSceneCase>& commonSceneCases() {
    static const auto result = [] {
        std::vector<CommonSceneCase> cases;
        for (const auto& r : rows("math/fixtures/scenes/common.tsv", 14, 150))
            cases.push_back({bytes(r[0]), bytes(r[1]), boolean(r[2]), real(r[3]), real(r[4]),
                             integer<int>(r[5]), bytes(r[6]), bytes(r[7]), bytes(r[8]), bytes(r[9]),
                             bytes(r[10]), bytes(r[11]), bytes(r[12]), bytes(r[13])});
        return cases;
    }();
    return result;
}

inline const std::vector<CanonicalSceneCase>& sourceCanonicalSceneCases() {
    static const auto result = [] {
        std::vector<CanonicalSceneCase> cases;
        for (const auto& r : rows("math/fixtures/scenes/canonical-cases.tsv", 7, 75))
            cases.push_back({bytes(r[0]), bytes(r[1]), bytes(r[2]), boolean(r[3]),
                             real(r[4]), real(r[5]), real(r[6]), {}});
        std::size_t index{};
        for (const auto& r : rows("math/fixtures/scenes/canonical-modes.tsv", 14, 150)) {
            const auto caseIndex = integer<std::size_t>(r[0]);
            const auto modeIndex = integer<std::size_t>(r[1]);
            if (caseIndex != index / 2 || modeIndex != index % 2)
                throw std::runtime_error("Canonical mode fixture order/identity changed");
            cases.at(caseIndex).modes.at(modeIndex) = {
                boolean(r[2]), boolean(r[3]), {real(r[4]), real(r[5]), real(r[6])},
                bytes(r[7]), bytes(r[8]), bytes(r[9]), strings(r[11], integer<std::size_t>(r[10])),
                integer<std::size_t>(r[12]), boolean(r[13])};
            ++index;
        }
        return cases;
    }();
    return result;
}

} // namespace math_test::fixtures
