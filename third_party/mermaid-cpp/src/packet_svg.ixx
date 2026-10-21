// Native Packet SVG rendering from mermaid@12.2.0,
// commit 1fb7869866dd619168d840291c9c54b4f129758b.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT licensed; see ../LICENSE.
export module folia.mermaid.packet_svg;
export import folia.mermaid.packet;
import std;

export namespace folia::mermaid {

struct PacketSvg {
    std::string svg;
    double width{};
    double height{};
};

// Renders the official Packet family's elements with its default Packet CSS.
// This does not implement MermaidAPI preprocessing, theme overrides, or the
// outer API's accessibility DOM. The document and config are authoritative:
// words are rebuilt rather than trusting a caller-modified layout cache.
//
// XML text is escaped and encoded as UTF-8. Invalid UTF-16, characters forbidden
// by XML 1.0, non-finite coordinates and negative dimensions return an error.
// Render errors use offset zero because PacketLayout has no source-location
// map; text diagnostics identify the field-local UTF-16 offset in the message.
std::expected<PacketSvg, PacketError> render_packet_svg(PacketLayout const& layout);

}
