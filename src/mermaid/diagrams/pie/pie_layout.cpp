// Source-derived filtering, labels, legend placement and viewBox expansion
// from Mermaid 12.2.0 pieRenderer.ts, commit
// 1fb7869866dd619168d840291c9c54b4f129758b.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT licensed; see ../LICENSE.
module folia.mermaid.pie_layout;
import std;
import folia.mermaid.number;

namespace folia::mermaid {
namespace {
std::unexpected<PieError> failure(std::u16string message) {
    return std::unexpected(PieError{0, std::move(message)});
}

std::vector<std::u16string> labels(PieDocument const& document) {
    std::vector<std::u16string> result;
    for (auto const& section : document.sections) {
        auto text = section.label;
        if (document.show_data) {
            auto const value = *format_number(section.value);
            text += u" [" + std::u16string(value.begin(), value.end()) + u"]";
        }
        result.push_back(std::move(text));
    }
    return result;
}

bool valid_width(double width) { return std::isfinite(width) && width >= 0; }
}

std::expected<std::vector<std::u16string>, PieError> pie_legend_text(PieDocument const& input) {
    auto document = populate_pie(input);
    if (!document) return std::unexpected(document.error());
    return labels(*document);
}

std::expected<PieLayout, PieError> layout_pie(
    PieDocument const& input, PieMeasurements const& measurements, PieLayoutConfig const& config) {
    auto populated = populate_pie(input);
    if (!populated) return std::unexpected(populated.error());
    if (measurements.legend_widths.size() != populated->sections.size()) {
        return failure(u"Pie needs one measured legend width per populated section.");
    }
    if (!valid_width(measurements.title_width) ||
        !std::ranges::all_of(measurements.legend_widths, valid_width)) {
        return failure(u"Pie text measurements must be finite and nonnegative.");
    }
    PieLayout result;
    result.document = std::move(*populated);
    double total{};
    for (auto const& section : result.document.sections) total += section.value;
    if (!std::isfinite(total)) return failure(u"Pie section sum is not finite.");

    std::vector<double> values;
    std::vector<std::size_t> indices;
    for (std::size_t index = 0; index < result.document.sections.size(); ++index) {
        auto const value = result.document.sections[index].value;
        // Zero total produces no slices, just as upstream's NaN comparison.
        if (total > 0 && (value / total) * 100 >= 1) {
            values.push_back(value);
            indices.push_back(index);
        }
    }
    auto geometry = layout_pie_geometry(values, config.geometry);
    if (!geometry) {
        auto const& message = geometry.error().message;
        return failure(std::u16string(message.begin(), message.end()));
    }
    for (std::size_t index = 0; index < geometry->size(); ++index) {
        auto const percentage = (values[index] / total) * 100;
        // Number.toFixed(0) rounds a positive exact halfway value upward.
        // Compare the fraction rather than adding 0.5 (which can round twice).
        auto rounded = std::floor(percentage);
        if (percentage - rounded >= 0.5) rounded += 1;
        if (rounded == 0) continue;
        result.slices.push_back({indices[index], std::move((*geometry)[index]),
            *format_number(rounded) + "%"});
    }

    auto texts = labels(result.document);
    auto longest = -std::numeric_limits<double>::infinity();
    for (auto width : measurements.legend_widths) longest = std::max(longest, width);
    double chart_width = 490;
    result.height = 450;
    auto const legend_height = 22.0 * static_cast<double>(texts.size());
    double legend_x{}, first_y{};
    switch (config.legend_position) {
    case PieLegendPosition::center:
        legend_x = -longest / 2 - 22;
        first_y = -legend_height / 2;
        break;
    case PieLegendPosition::top:
        result.height += legend_height;
        legend_x = -longest / 2 - 22;
        first_y = -185;
        result.pie_y = legend_height + 22;
        break;
    case PieLegendPosition::bottom:
        result.height += legend_height;
        legend_x = -longest / 2 - 22;
        first_y = 207;
        break;
    case PieLegendPosition::left:
        chart_width += 22 + longest;
        legend_x = -207;
        first_y = -legend_height / 2;
        result.pie_x = longest + 22;
        break;
    case PieLegendPosition::right:
        chart_width += 22 + longest;
        legend_x = 216;
        first_y = -legend_height / 2;
        break;
    default: return failure(u"Invalid Pie legend position.");
    }
    for (std::size_t index = 0; index < texts.size(); ++index) {
        result.legends.push_back({std::move(texts[index]), legend_x,
            22.0 * static_cast<double>(index) + first_y});
    }
    auto const title_left = 225 - measurements.title_width / 2;
    auto const title_right = 225 + measurements.title_width / 2;
    result.viewbox_x = std::min(0.0, title_left);
    result.width = std::max(chart_width, title_right) - result.viewbox_x;
    if (!valid_width(result.width) || !valid_width(result.height) ||
        !std::isfinite(result.viewbox_x) || !std::isfinite(result.pie_x) ||
        !std::isfinite(result.pie_y) ||
        !std::ranges::all_of(result.legends, [](auto const& legend) {
            return std::isfinite(legend.x) && std::isfinite(legend.y);
        })) {
        // In particular, official empty left-legend output has an infinite
        // group translation. Do not publish invalid native SVG geometry.
        return failure(u"Pie layout produced non-finite geometry.");
    }
    return result;
}
}
