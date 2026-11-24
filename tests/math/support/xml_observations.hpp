#pragma once

#include "math/support/mathml_observations.hpp"

enum class XmlHdwPolicy { automatic, use, force, ignore };
enum class XmlSourceStyleKind { text, number };
struct XmlSourceStyleEntry { std::u16string_view selector,property; XmlSourceStyleKind kind; std::u16string_view text; double number; };
struct XmlSourceObservation { double w,h,d,scale,rscale; std::u16string_view family,font_size; std::string_view html,wrapper_styles,source_html_hdw; bool can_measure_nodes; double output_scale,metrics_scale,em,ex; std::u16string_view metrics_family; };
struct XmlSourceCase { mathml_scene_test::Case source; XmlHdwPolicy hdw; double scale; bool can_measure_nodes; std::string_view start_kind,start_parent_kind; std::span<std::u16string_view const> payloads; std::span<XmlSourceObservation const> observations; };
inline constexpr int observationEnumMaximum(XmlSourceStyleKind) { return 1; }

inline constexpr int observationEnumMaximum(XmlHdwPolicy) { return 3; }

template <class IO>
void observationFields(IO& io, const std::string& path, XmlSourceStyleEntry& value) {
    io.field(path + ".selector", value.selector);
    io.field(path + ".property", value.property);
    io.field(path + ".kind", value.kind);
    io.field(path + ".text", value.text);
    io.field(path + ".number", value.number);
}

template <class IO>
void observationFields(IO& io, const std::string& path, XmlSourceObservation& value) {
    io.field(path + ".w", value.w);
    io.field(path + ".h", value.h);
    io.field(path + ".d", value.d);
    io.field(path + ".scale", value.scale);
    io.field(path + ".rscale", value.rscale);
    io.field(path + ".family", value.family);
    io.field(path + ".font_size", value.font_size);
    io.field(path + ".html", value.html);
    io.field(path + ".wrapper_styles", value.wrapper_styles);
    io.field(path + ".source_html_hdw", value.source_html_hdw);
    io.field(path + ".can_measure_nodes", value.can_measure_nodes);
    io.field(path + ".output_scale", value.output_scale);
    io.field(path + ".metrics_scale", value.metrics_scale);
    io.field(path + ".em", value.em);
    io.field(path + ".ex", value.ex);
    io.field(path + ".metrics_family", value.metrics_family);
}

template <class IO>
void observationFields(IO& io, const std::string& path, XmlSourceCase& value) {
    io.field(path + ".source", value.source);
    io.field(path + ".hdw", value.hdw);
    io.field(path + ".scale", value.scale);
    io.field(path + ".can_measure_nodes", value.can_measure_nodes);
    io.field(path + ".start_kind", value.start_kind);
    io.field(path + ".start_parent_kind", value.start_parent_kind);
    io.field(path + ".payloads", value.payloads);
    io.field(path + ".observations", value.observations);
}

inline const auto& xmlObservations() {
    struct Corpus {
        math_test::fixtures::ObservationReader input{"math/fixtures/scenes/xml/observations.tsv"};
        std::span<XmlSourceStyleEntry const> sourceXmlStyleEntries;
        std::span<XmlSourceStyleEntry const> sourceAnnotationXmlStyleEntries;
        std::span<XmlSourceCase const> sourceXmlCases;
        Corpus() {
            input.field("sourceXmlStyleEntries", sourceXmlStyleEntries);
            if (sourceXmlStyleEntries.size() != 17)
                throw std::runtime_error("Incomplete xml fixture: sourceXmlStyleEntries");
            input.field("sourceAnnotationXmlStyleEntries", sourceAnnotationXmlStyleEntries);
            if (sourceAnnotationXmlStyleEntries.size() != 3)
                throw std::runtime_error("Incomplete xml fixture: sourceAnnotationXmlStyleEntries");
            input.field("sourceXmlCases", sourceXmlCases);
            if (sourceXmlCases.size() != 11)
                throw std::runtime_error("Incomplete xml fixture: sourceXmlCases");
            input.finish();
        }
    };
    static const Corpus value;
    return value;
}
inline auto sourceXmlStyleEntries() { return xmlObservations().sourceXmlStyleEntries; }
inline auto sourceAnnotationXmlStyleEntries() { return xmlObservations().sourceAnnotationXmlStyleEntries; }
inline auto sourceXmlCases() { return xmlObservations().sourceXmlCases; }
