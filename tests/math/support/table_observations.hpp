#pragma once

#include "math/support/mathml_observations.hpp"

using MathmlAttributeKind = mathml_scene_test::AttributeKind;
using MathmlSceneAttribute = mathml_scene_test::Attribute;
using MathmlSceneNode = mathml_scene_test::Node;
using MathmlSceneMetrics = mathml_scene_test::Metrics;
struct MathmlSceneBBox {
    double w{}, h{}, d{}, L{}, R{};
};
struct MathmlTableSceneCase {
    std::string_view name;
    MathmlSceneMetrics metrics;
    std::span<MathmlSceneNode const> nodes;
    std::span<MathmlSceneAttribute const> attributes;
    std::span<double const> H, D, W;
    double L{};
    std::span<double const> row_rscales;
    std::size_t dotted_lines{};
    std::size_t dotted_frames{};
    MathmlSceneBBox bbox;
    std::string_view source_svg;
};

template <class IO>
void observationFields(IO& io, const std::string& path, MathmlSceneBBox& value) {
    io.field(path + ".w", value.w);
    io.field(path + ".h", value.h);
    io.field(path + ".d", value.d);
    io.field(path + ".L", value.L);
    io.field(path + ".R", value.R);
}

template <class IO>
void observationFields(IO& io, const std::string& path, MathmlTableSceneCase& value) {
    io.field(path + ".name", value.name);
    io.field(path + ".metrics", value.metrics);
    io.field(path + ".nodes", value.nodes);
    io.field(path + ".attributes", value.attributes);
    io.field(path + ".H", value.H);
    io.field(path + ".D", value.D);
    io.field(path + ".W", value.W);
    io.field(path + ".L", value.L);
    io.field(path + ".row_rscales", value.row_rscales);
    io.field(path + ".dotted_lines", value.dotted_lines);
    io.field(path + ".dotted_frames", value.dotted_frames);
    io.field(path + ".bbox", value.bbox);
    io.field(path + ".source_svg", value.source_svg);
}

inline const auto& tableObservations() {
    struct Corpus {
        math_test::fixtures::ObservationReader input{"math/fixtures/scenes/table/observations.tsv"};
        std::span<MathmlTableSceneCase const> sourceTableCases;
        Corpus() {
            input.field("sourceTableCases", sourceTableCases);
            if (sourceTableCases.size() != 8)
                throw std::runtime_error("Incomplete table fixture: sourceTableCases");
            input.finish();
        }
    };
    static const Corpus value;
    return value;
}
inline auto sourceTableCases() { return tableObservations().sourceTableCases; }
