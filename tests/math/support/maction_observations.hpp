#pragma once

#include "math/support/mathml_observations.hpp"

struct MactionSourceListener { std::string_view type; unsigned target; unsigned host; std::string_view before; std::string_view after; };
struct MactionSourceWrapper { std::size_t node_index; std::string_view action_type; std::string_view selection; std::string_view selected_kind; double tip_dx,tip_dy,w,h,d,scale,rscale,L,R; std::string_view pwidth; };
struct MactionSourceStyleEntry { std::u16string_view selector; std::u16string_view property; bool numeric; std::u16string_view text; double number; };
struct MactionSourceCase { std::string_view name; std::string_view mathml; bool display; double em; double ex; double width; std::string_view font_cache; std::string_view local_id; double root_w,root_h,root_d,root_scale,root_rscale,root_L,root_R; std::string_view root_pwidth; std::span<std::string_view const> local_cache_prefixes; std::string_view cache_summary; std::string_view svg; MactionSourceListener listeners[4]; unsigned listener_count; MactionSourceWrapper wrappers[4]; unsigned wrapper_count; std::span<mathml_scene_test::Node const> nodes; std::span<mathml_scene_test::Attribute const> attributes; };
template <class IO>
void observationFields(IO& io, const std::string& path, MactionSourceListener& value) {
    io.field(path + ".type", value.type);
    io.field(path + ".target", value.target);
    io.field(path + ".host", value.host);
    io.field(path + ".before", value.before);
    io.field(path + ".after", value.after);
}

template <class IO>
void observationFields(IO& io, const std::string& path, MactionSourceWrapper& value) {
    io.field(path + ".node_index", value.node_index);
    io.field(path + ".action_type", value.action_type);
    io.field(path + ".selection", value.selection);
    io.field(path + ".selected_kind", value.selected_kind);
    io.field(path + ".tip_dx", value.tip_dx);
    io.field(path + ".tip_dy", value.tip_dy);
    io.field(path + ".w", value.w);
    io.field(path + ".h", value.h);
    io.field(path + ".d", value.d);
    io.field(path + ".scale", value.scale);
    io.field(path + ".rscale", value.rscale);
    io.field(path + ".L", value.L);
    io.field(path + ".R", value.R);
    io.field(path + ".pwidth", value.pwidth);
}

template <class IO>
void observationFields(IO& io, const std::string& path, MactionSourceStyleEntry& value) {
    io.field(path + ".selector", value.selector);
    io.field(path + ".property", value.property);
    io.field(path + ".numeric", value.numeric);
    io.field(path + ".text", value.text);
    io.field(path + ".number", value.number);
}

template <class IO>
void observationFields(IO& io, const std::string& path, MactionSourceCase& value) {
    io.field(path + ".name", value.name);
    io.field(path + ".mathml", value.mathml);
    io.field(path + ".display", value.display);
    io.field(path + ".em", value.em);
    io.field(path + ".ex", value.ex);
    io.field(path + ".width", value.width);
    io.field(path + ".font_cache", value.font_cache);
    io.field(path + ".local_id", value.local_id);
    io.field(path + ".root_w", value.root_w);
    io.field(path + ".root_h", value.root_h);
    io.field(path + ".root_d", value.root_d);
    io.field(path + ".root_scale", value.root_scale);
    io.field(path + ".root_rscale", value.root_rscale);
    io.field(path + ".root_L", value.root_L);
    io.field(path + ".root_R", value.root_R);
    io.field(path + ".root_pwidth", value.root_pwidth);
    io.field(path + ".local_cache_prefixes", value.local_cache_prefixes);
    io.field(path + ".cache_summary", value.cache_summary);
    io.field(path + ".svg", value.svg);
    io.field(path + ".listeners", value.listeners);
    io.field(path + ".listener_count", value.listener_count);
    io.field(path + ".wrappers", value.wrappers);
    io.field(path + ".wrapper_count", value.wrapper_count);
    io.field(path + ".nodes", value.nodes);
    io.field(path + ".attributes", value.attributes);
}

inline const auto& mactionObservations() {
    struct Corpus {
        math_test::fixtures::ObservationReader input{"math/fixtures/scenes/maction/observations.tsv"};
        std::span<MactionSourceStyleEntry const> sourceMactionStyles;
        std::span<MactionSourceCase const> sourceMactionCases;
        Corpus() {
            input.field("sourceMactionStyles", sourceMactionStyles);
            if (sourceMactionStyles.size() != 32)
                throw std::runtime_error("Incomplete maction fixture: sourceMactionStyles");
            input.field("sourceMactionCases", sourceMactionCases);
            if (sourceMactionCases.size() != 14)
                throw std::runtime_error("Incomplete maction fixture: sourceMactionCases");
            input.finish();
        }
    };
    static const Corpus value;
    return value;
}
inline auto sourceMactionStyles() { return mactionObservations().sourceMactionStyles; }
inline auto sourceMactionCases() { return mactionObservations().sourceMactionCases; }
