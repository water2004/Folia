#pragma once

#include "math/support/mathml_observations.hpp"

struct MerrorSourceBBox { double width, height, depth, left, right; };
struct MerrorSourceCase { mathml_scene_test::Case source; std::array<MerrorSourceBBox, 2> boxes; std::size_t box_count; };
struct MerrorSourceStyleRule { std::u16string_view selector, fill, stroke; };
template <class IO>
void observationFields(IO& io, const std::string& path, MerrorSourceBBox& value) {
    io.field(path + ".width", value.width);
    io.field(path + ".height", value.height);
    io.field(path + ".depth", value.depth);
    io.field(path + ".left", value.left);
    io.field(path + ".right", value.right);
}

template <class IO>
void observationFields(IO& io, const std::string& path, MerrorSourceCase& value) {
    io.field(path + ".source", value.source);
    io.field(path + ".boxes", value.boxes);
    io.field(path + ".box_count", value.box_count);
}

template <class IO>
void observationFields(IO& io, const std::string& path, MerrorSourceStyleRule& value) {
    io.field(path + ".selector", value.selector);
    io.field(path + ".fill", value.fill);
    io.field(path + ".stroke", value.stroke);
}

inline const auto& merrorObservations() {
    struct Corpus {
        math_test::fixtures::ObservationReader input{"math/fixtures/scenes/merror/observations.tsv"};
        std::span<MerrorSourceStyleRule const> sourceMerrorStyleRules;
        std::span<MerrorSourceCase const> sourceMerrorCases;
        Corpus() {
            input.field("sourceMerrorStyleRules", sourceMerrorStyleRules);
            if (sourceMerrorStyleRules.size() != 2)
                throw std::runtime_error("Incomplete merror fixture: sourceMerrorStyleRules");
            input.field("sourceMerrorCases", sourceMerrorCases);
            if (sourceMerrorCases.size() != 7)
                throw std::runtime_error("Incomplete merror fixture: sourceMerrorCases");
            input.finish();
        }
    };
    static const Corpus value;
    return value;
}
inline auto sourceMerrorStyleRules() { return merrorObservations().sourceMerrorStyleRules; }
inline auto sourceMerrorCases() { return merrorObservations().sourceMerrorCases; }
