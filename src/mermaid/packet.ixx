// Native Packet grammar and row construction from mermaid@12.2.0,
// commit 1fb7869866dd619168d840291c9c54b4f129758b.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT licensed; see ../LICENSE.
export module folia.mermaid.packet;
import std;

export namespace folia::mermaid {

struct PacketBlock {
    std::optional<double> start;
    std::optional<double> end;
    std::optional<double> bits;
    std::u16string label;
};

struct PacketDocument {
    std::u16string title;
    std::u16string acc_title;
    std::u16string acc_description;
    std::vector<PacketBlock> blocks;
};

struct PacketError {
    // UTF-16 source offset for parse errors. Layout has no source-location
    // table in PacketDocument and reports zero instead of a fabricated offset.
    std::size_t offset{};
    std::u16string message;
};

// Parses the official Packet grammar, including its hidden tokens and common
// title/accessibility value conversion. It does not apply Mermaid's outer
// frontmatter/directive configuration, entity encoding, or DOM sanitization.
std::expected<PacketDocument, PacketError> parse_packet(std::u16string_view source);

struct PacketConfig {
    double row_height = 32;
    double padding_x = 5;
    double padding_y = 5;
    double bit_width = 32;
    std::uint32_t bits_per_row = 32;
    bool show_bits = true;
    bool use_max_width = true;
    bool descending = false;
};

struct PacketWordBlock {
    double start{};
    double end{};
    // Preserves the official populate/getNextFittingBlock data field. For
    // split fields it is not necessarily end - start + 1; painting uses the
    // inclusive endpoints to determine the visible field width.
    double bits{};
    std::u16string label;
};

struct PacketLayout {
    PacketDocument document;
    // Unmodified caller configuration: the renderer applies show_bits' extra
    // ten units of vertical padding, matching PacketDB.getConfig().
    PacketConfig config;
    std::vector<std::vector<PacketWordBlock>> words;
};

// Constructs the official contiguous rows and preserves semantic error order.
// Native safety boundaries reject non-finite/non-integral bit indices, invalid
// dimensions, and fields exceeding the 10,000-row budget instead of publishing
// the partial result that upstream's capped populate loop can return.
std::expected<PacketLayout, PacketError> layout_packet(
    PacketDocument const& document,
    PacketConfig const& config = {});

}
