#pragma once

#include "math/support/mathml_observations.hpp"

struct TableProfileBBox {
    double w{}, h{}, d{}, scale{}, rscale{}, L{}, R{};
};
struct TableProfileTable {
    std::size_t node_index{};
    TableProfileBBox bbox;
    std::size_t rows{}, cols{};
    bool has_labels{}, is_top{};
    double pwidth{};
    std::span<double const> H, D, W, NH, ND;
    double label_width{};
};
struct TableProfileMeasurement {
    std::u16string_view text;
    std::string_view variant, font_family;
    bool italic{}, bold{};
    double w{}, h{}, d{};
};
struct TableProfileSourceCase {
    std::string_view name, mathml;
    bool requested_display{}, display{};
    double em{}, ex{}, width{};
    std::string_view font_cache;
    TableProfileBBox root_bbox;
    std::string_view error_name, error_message, svg;
    std::span<std::string_view const> local_cache_prefixes;
    std::span<TableProfileTable const> tables;
    std::span<TableProfileMeasurement const> measurements;
    std::span<mathml_scene_test::Node const> nodes;
    std::span<mathml_scene_test::Attribute const> attributes;
};


template <class IO>
void observationFields(IO& io, const std::string& path, TableProfileBBox& value) {
    io.field(path + ".w", value.w);
    io.field(path + ".h", value.h);
    io.field(path + ".d", value.d);
    io.field(path + ".scale", value.scale);
    io.field(path + ".rscale", value.rscale);
    io.field(path + ".L", value.L);
    io.field(path + ".R", value.R);
}

template <class IO>
void observationFields(IO& io, const std::string& path, TableProfileTable& value) {
    io.field(path + ".node_index", value.node_index);
    io.field(path + ".bbox", value.bbox);
    io.field(path + ".rows", value.rows);
    io.field(path + ".cols", value.cols);
    io.field(path + ".has_labels", value.has_labels);
    io.field(path + ".is_top", value.is_top);
    io.field(path + ".pwidth", value.pwidth);
    io.field(path + ".H", value.H);
    io.field(path + ".D", value.D);
    io.field(path + ".W", value.W);
    io.field(path + ".NH", value.NH);
    io.field(path + ".ND", value.ND);
    io.field(path + ".label_width", value.label_width);
}

template <class IO>
void observationFields(IO& io, const std::string& path, TableProfileMeasurement& value) {
    io.field(path + ".text", value.text);
    io.field(path + ".variant", value.variant);
    io.field(path + ".font_family", value.font_family);
    io.field(path + ".italic", value.italic);
    io.field(path + ".bold", value.bold);
    io.field(path + ".w", value.w);
    io.field(path + ".h", value.h);
    io.field(path + ".d", value.d);
}

template <class IO>
void observationFields(IO& io, const std::string& path, TableProfileSourceCase& value) {
    io.field(path + ".name", value.name);
    io.field(path + ".mathml", value.mathml);
    io.field(path + ".requested_display", value.requested_display);
    io.field(path + ".display", value.display);
    io.field(path + ".em", value.em);
    io.field(path + ".ex", value.ex);
    io.field(path + ".width", value.width);
    io.field(path + ".font_cache", value.font_cache);
    io.field(path + ".root_bbox", value.root_bbox);
    io.field(path + ".error_name", value.error_name);
    io.field(path + ".error_message", value.error_message);
    io.field(path + ".svg", value.svg);
    io.field(path + ".local_cache_prefixes", value.local_cache_prefixes);
    io.field(path + ".tables", value.tables);
    io.field(path + ".measurements", value.measurements);
    io.field(path + ".nodes", value.nodes);
    io.field(path + ".attributes", value.attributes);
}

inline const auto& tableProfileObservations() {
    struct Corpus {
        math_test::fixtures::ObservationReader input{"math/fixtures/scenes/table-profile/observations.tsv"};
        std::span<TableProfileSourceCase const> sourceTableProfileCases;
        Corpus() {
            input.field("sourceTableProfileCases", sourceTableProfileCases);
            if (sourceTableProfileCases.size() != 14)
                throw std::runtime_error("Incomplete table-profile fixture: sourceTableProfileCases");
            input.finish();
        }
    };
    static const Corpus value;
    return value;
}
inline auto sourceTableProfileCases() { return tableProfileObservations().sourceTableProfileCases; }
