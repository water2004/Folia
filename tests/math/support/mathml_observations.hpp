#pragma once

#include "math/support/mathml_scene.hpp"
#include "math/support/observation_reader.hpp"

namespace mathml_scene_test {
inline constexpr int observationEnumMaximum(AttributeKind) { return 2; }

template <class IO>
void observationFields(IO& io, const std::string& path, Attribute& value) {
    io.field(path + ".name", value.name);
    io.field(path + ".kind", value.kind);
    io.field(path + ".text", value.text);
    io.field(path + ".number", value.number);
    io.field(path + ".boolean", value.boolean);
}

template <class IO>
void observationFields(IO& io, const std::string& path, Node& value) {
    io.field(path + ".kind", value.kind);
    io.field(path + ".is_inferred", value.is_inferred);
    io.field(path + ".parent", value.parent);
    io.field(path + ".attribute_first", value.attribute_first);
    io.field(path + ".attribute_count", value.attribute_count);
    io.field(path + ".text", value.text);
    io.field(path + ".xml_payload_index", value.xml_payload_index);
}

template <class IO>
void observationFields(IO& io, const std::string& path, Metrics& value) {
    io.field(path + ".em", value.em);
    io.field(path + ".ex", value.ex);
    io.field(path + ".container_width", value.container_width);
    io.field(path + ".display", value.display);
}

template <class IO>
void observationFields(IO& io, const std::string& path, Case& value) {
    io.field(path + ".name", value.name);
    io.field(path + ".metrics", value.metrics);
    io.field(path + ".nodes", value.nodes);
    io.field(path + ".attributes", value.attributes);
    io.field(path + ".source_svg", value.source_svg);
}
}
