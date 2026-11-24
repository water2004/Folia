#pragma once

#include "math/support/mathml_observations.hpp"

struct LimitsSceneBBox { double w,h,d,scale,rscale,L,R; };
struct LimitsSceneSourceCase { std::string_view name,font_cache; bool display; double em,ex,width; std::span<mathml_scene_test::Node const> nodes; std::span<mathml_scene_test::Attribute const> attributes; LimitsSceneBBox root_bbox; std::size_t limits_node_index; LimitsSceneBBox limits_bbox; std::span<std::string_view const> local_cache_prefixes; std::string_view svg; };
template <class IO>
void observationFields(IO& io, const std::string& path, LimitsSceneBBox& value) {
    io.field(path + ".w", value.w);
    io.field(path + ".h", value.h);
    io.field(path + ".d", value.d);
    io.field(path + ".scale", value.scale);
    io.field(path + ".rscale", value.rscale);
    io.field(path + ".L", value.L);
    io.field(path + ".R", value.R);
}

template <class IO>
void observationFields(IO& io, const std::string& path, LimitsSceneSourceCase& value) {
    io.field(path + ".name", value.name);
    io.field(path + ".font_cache", value.font_cache);
    io.field(path + ".display", value.display);
    io.field(path + ".em", value.em);
    io.field(path + ".ex", value.ex);
    io.field(path + ".width", value.width);
    io.field(path + ".nodes", value.nodes);
    io.field(path + ".attributes", value.attributes);
    io.field(path + ".root_bbox", value.root_bbox);
    io.field(path + ".limits_node_index", value.limits_node_index);
    io.field(path + ".limits_bbox", value.limits_bbox);
    io.field(path + ".local_cache_prefixes", value.local_cache_prefixes);
    io.field(path + ".svg", value.svg);
}

inline const auto& limitsObservations() {
    struct Corpus {
        math_test::fixtures::ObservationReader input{"math/fixtures/scenes/limits/observations.tsv"};
        std::span<LimitsSceneSourceCase const> sourceLimitsSceneCases;
        Corpus() {
            input.field("sourceLimitsSceneCases", sourceLimitsSceneCases);
            if (sourceLimitsSceneCases.size() != 24)
                throw std::runtime_error("Incomplete limits fixture: sourceLimitsSceneCases");
            input.finish();
        }
    };
    static const Corpus value;
    return value;
}
inline auto sourceLimitsSceneCases() { return limitsObservations().sourceLimitsSceneCases; }
