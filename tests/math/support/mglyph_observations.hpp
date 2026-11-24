#pragma once

#include "math/support/mathml_observations.hpp"

enum class MglyphSourceKind { image, legacy, verification };
struct MglyphImageAttribute { unsigned image_index; std::string_view name; std::string_view value; };
struct MglyphSourceMeasurement { std::u16string_view text; std::string_view variant; std::string_view font_family; bool italic; bool bold; double w,h,d; };
struct MglyphSourceBBox { double w,h,d,scale,rscale,L,R; };
struct MglyphSourceCase { std::string_view name; MglyphSourceKind kind; std::string_view mathml; bool requested_display; bool display; double em,ex,width; std::string_view font_cache,local_id; double root_w,root_h,root_d; std::optional<MglyphSourceBBox> mglyph_bbox; std::string_view error_name,error_message,svg; std::span<std::string_view const> local_cache_prefixes; std::span<MglyphImageAttribute const> image_attributes; std::span<MglyphSourceMeasurement const> measurements; std::span<mathml_scene_test::Node const> nodes; std::span<mathml_scene_test::Attribute const> attributes; };
inline constexpr int observationEnumMaximum(MglyphSourceKind) { return 2; }

template <class IO>
void observationFields(IO& io, const std::string& path, MglyphImageAttribute& value) {
    io.field(path + ".image_index", value.image_index);
    io.field(path + ".name", value.name);
    io.field(path + ".value", value.value);
}

template <class IO>
void observationFields(IO& io, const std::string& path, MglyphSourceMeasurement& value) {
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
void observationFields(IO& io, const std::string& path, MglyphSourceBBox& value) {
    io.field(path + ".w", value.w);
    io.field(path + ".h", value.h);
    io.field(path + ".d", value.d);
    io.field(path + ".scale", value.scale);
    io.field(path + ".rscale", value.rscale);
    io.field(path + ".L", value.L);
    io.field(path + ".R", value.R);
}

template <class IO>
void observationFields(IO& io, const std::string& path, MglyphSourceCase& value) {
    io.field(path + ".name", value.name);
    io.field(path + ".kind", value.kind);
    io.field(path + ".mathml", value.mathml);
    io.field(path + ".requested_display", value.requested_display);
    io.field(path + ".display", value.display);
    io.field(path + ".em", value.em);
    io.field(path + ".ex", value.ex);
    io.field(path + ".width", value.width);
    io.field(path + ".font_cache", value.font_cache);
    io.field(path + ".local_id", value.local_id);
    io.field(path + ".root_w", value.root_w);
    io.field(path + ".root_h", value.root_h);
    io.field(path + ".root_d", value.root_d);
    io.field(path + ".mglyph_bbox", value.mglyph_bbox);
    io.field(path + ".error_name", value.error_name);
    io.field(path + ".error_message", value.error_message);
    io.field(path + ".svg", value.svg);
    io.field(path + ".local_cache_prefixes", value.local_cache_prefixes);
    io.field(path + ".image_attributes", value.image_attributes);
    io.field(path + ".measurements", value.measurements);
    io.field(path + ".nodes", value.nodes);
    io.field(path + ".attributes", value.attributes);
}

inline const auto& mglyphObservations() {
    struct Corpus {
        math_test::fixtures::ObservationReader input{"math/fixtures/scenes/mglyph/observations.tsv"};
        std::span<MglyphSourceCase const> sourceMglyphCases;
        Corpus() {
            input.field("sourceMglyphCases", sourceMglyphCases);
            if (sourceMglyphCases.size() != 13)
                throw std::runtime_error("Incomplete mglyph fixture: sourceMglyphCases");
            input.finish();
        }
    };
    static const Corpus value;
    return value;
}
inline auto sourceMglyphCases() { return mglyphObservations().sourceMglyphCases; }
