// Mermaid 12.2.0 Event Modeling layout and SVG rendering. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.eventmodeling_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;

namespace folia::mermaid {
namespace {

using View = std::u16string_view;
constexpr double lane_padding = 15;
constexpr double lane_gap = 10;
constexpr double box_padding = 10;
constexpr double box_overlap = 90;
constexpr double start_x = 250;
constexpr double text_size = 16;
constexpr double text_width = 430;
constexpr View fixed_font = u"\"trebuchet ms\", verdana, arial, sans-serif";
constexpr View code_tag = u"<code style=\"text-align: left; display: block;max-width:430px\">";

std::string number(double value) {
    const auto formatted = format_number(value);
    if (!formatted) throw std::runtime_error("Invalid Event Modeling geometry");
    return *formatted;
}

std::string escape(View text) {
    const auto escaped = escape_svg_text(text, u"Event Modeling text");
    if (!escaped) throw std::runtime_error("Invalid Event Modeling SVG text");
    return *escaped;
}

std::string attribute(std::string_view name, std::string_view value) {
    return " " + std::string(name) + "=\"" + std::string(value) + "\"";
}

std::string attribute(std::string_view name, View value) {
    auto escaped = escape(value);
    std::string quoted;
    for (const auto ch : escaped) {
        if (ch == '"') quoted += "&quot;";
        else quoted += ch;
    }
    return attribute(name, quoted);
}

std::string attribute(std::string_view name, double value) {
    return attribute(name, number(value));
}

bool ascii_equal(char16_t ch, char16_t lower) {
    return ch == lower || ch == lower - u'a' + u'A';
}

char16_t ascii_lower(char16_t ch) {
    return ch >= u'A' && ch <= u'Z' ? ch - u'A' + u'a' : ch;
}

std::size_t break_size(View text) {
    if (!text.starts_with(u"<")) return 0;
    std::size_t index = text.starts_with(u"</") ? 2 : 1;
    if (index + 1 >= text.size() || !ascii_equal(text[index], u'b') ||
        !ascii_equal(text[index + 1], u'r')) return 0;
    index += 2;
    while (index < text.size() && View(u" \t\r\n\v\f").find(text[index]) != View::npos) ++index;
    if (index < text.size() && text[index] == u'/') ++index;
    return index < text.size() && text[index] == u'>' ? index + 1 : 0;
}

constexpr View inline_tags[] = {
    u"b", u"code", u"em", u"i", u"kbd", u"mark", u"s", u"samp", u"small",
    u"span", u"strike", u"strong", u"sub", u"sup", u"u",
};

struct HtmlTag {
    std::u16string name;
    std::size_t size;
    bool closing;
};

std::optional<HtmlTag> html_tag(View text) {
    if (!text.starts_with(u"<")) return std::nullopt;
    const bool closing = text.starts_with(u"</");
    std::size_t index = closing ? 2 : 1;
    const auto start = index;
    std::u16string name;
    while (index < text.size()) {
        const auto ch = ascii_lower(text[index]);
        if (ch < u'a' || ch > u'z') break;
        name += ch;
        ++index;
    }
    if (index == start || index == text.size() || View(u">/ \r\n\t\f").find(text[index]) == View::npos) {
        return std::nullopt;
    }
    char16_t quote{};
    for (; index < text.size(); ++index) {
        const auto ch = text[index];
        if (quote) {
            if (ch == quote) quote = 0;
        } else if (ch == u'\'' || ch == u'"') {
            quote = ch;
        } else if (ch == u'>') {
            return HtmlTag{std::move(name), index + 1, closing};
        }
    }
    return std::nullopt;
}

constexpr std::pair<View, View> text_entities[] = {
    {u"&amp;", u"&amp;"}, {u"&lt;", u"&lt;"}, {u"&gt;", u"&gt;"},
    {u"&nbsp;", u"&nbsp;"}, {u"&quot;", u"\""}, {u"&apos;", u"'"},
};

std::u16string safe_html_text(View text) {
    std::u16string html;
    std::vector<std::u16string> open;
    auto close_tag = [&] {
        html += u"</" + open.back() + u">";
        open.pop_back();
    };
    while (!text.empty()) {
        if (text.starts_with(u"<!--")) {
            const auto end = text.find(u"-->", 4);
            if (end == View::npos) break;
            text.remove_prefix(end + 3);
            continue;
        }
        if (auto tag = html_tag(text)) {
            text.remove_prefix(tag->size);
            if (!tag->closing && (tag->name == u"script" || tag->name == u"style")) {
                View remaining;
                for (auto start = text.find(u'<'); start != View::npos; start = text.find(u'<', start + 1)) {
                    const auto end = html_tag(text.substr(start));
                    if (end && end->closing && end->name == tag->name) {
                        remaining = text.substr(start + end->size);
                        break;
                    }
                }
                text = remaining;
            } else if (tag->name == u"br") {
                html += u"<br>";
            } else if (std::ranges::find(inline_tags, tag->name) != std::end(inline_tags)) {
                if (!tag->closing) {
                    html += u"<" + tag->name + u">";
                    open.push_back(std::move(tag->name));
                } else {
                    const auto matching = std::find(open.rbegin(), open.rend(), tag->name);
                    const auto closing = static_cast<std::size_t>(matching - open.rbegin());
                    if (closing < open.size()) {
                        for (std::size_t i = 0; i <= closing; ++i) close_tag();
                    }
                }
            }
            continue;
        }
        if (text.front() == u'&') {
            const auto entity = std::ranges::find_if(text_entities, [&](const auto& item) {
                return text.starts_with(item.first);
            });
            if (entity != std::end(text_entities)) {
                html += entity->second;
                text.remove_prefix(entity->first.size());
                continue;
            }
        }
        switch (text.front()) {
        case u'&': html += u"&amp;"; break;
        case u'<': html += u"&lt;"; break;
        case u'>': html += u"&gt;"; break;
        default: html += text.front(); break;
        }
        text.remove_prefix(1);
    }
    while (!open.empty()) close_tag();
    return html;
}

std::string label_markup(View text) {
    constexpr View tokens[] = {
        code_tag, u"<br>", u"<br/>",
        u"&lt;", u"&gt;", u"&amp;", u"&nbsp;"
    };
    std::string output;
    for (std::size_t i = 0; i < text.size();) {
        const auto tail = text.substr(i);
        if (auto tag = html_tag(tail); tag && tag->size == tag->name.size() + (tag->closing ? 3 : 2) &&
            std::ranges::find(inline_tags, tag->name) != std::end(inline_tags)) {
            const auto markup = tail.substr(0, tag->size);
            output.append(markup.begin(), markup.end());
            i += tag->size;
            continue;
        }
        const auto token = std::ranges::find_if(tokens, [&](View candidate) {
            return tail.starts_with(candidate);
        });
        if (token != std::end(tokens)) {
            if (token->starts_with(u"<br")) output += "<br/>";
            else if (*token == u"&nbsp;") output += "&#160;";
            else output.append(token->begin(), token->end());
            i += token->size();
        } else {
            auto end = text.find_first_of(u"<&", i + 1);
            if (end == View::npos) end = text.size();
            output += escape(text.substr(i, end - i));
            i = end;
        }
    }
    return output;
}

SvgTextMetrics measure_text(const EventmodelingTextMeasure& measure, View text, View font) {
    auto result = measure(text, font, text_size, 700);
    if (!result) throw std::runtime_error(result.error());
    return *result;
}

struct Dimensions {
    double width{}, height{}, line_height{};
};

Dimensions dimensions(View text, const EventmodelingTextMeasure& measure) {
    if (text.empty()) return {};
    std::vector<View> lines;
    auto start = std::size_t{0};
    for (auto pos = text.find(u'<'); pos != View::npos; pos = text.find(u'<', pos + 1)) {
        if (const auto size = break_size(text.substr(pos))) {
            lines.push_back(text.substr(start, pos - start));
            start = pos + size;
            pos = start - 1;
        }
    }
    lines.push_back(text.substr(start));

    auto measure_font = [&](View font) {
        Dimensions result;
        for (const auto line : lines) {
            const auto m = measure_text(measure, line.empty() ? u"\u200b" : line, font);
            result.width = std::max(result.width, std::round(m.width));
            result.line_height = std::max(result.line_height, std::round(m.height));
            result.height += std::round(m.height);
        }
        return result;
    };
    const auto generic = measure_font(u"sans-serif");
    const auto fixed = measure_font(fixed_font);
    if (generic.width > fixed.width && generic.height > fixed.height &&
        generic.line_height > fixed.line_height) return generic;
    return fixed;
}

std::u16string wrap_label(View text, const EventmodelingTextMeasure& measure) {
    if (text.find(u"<br>") != View::npos) return std::u16string(text);
    std::u16string result, line;
    auto append_line = [&] {
        if (line.empty()) return;
        if (!result.empty()) result += u"<br/>";
        result += line;
        line.clear();
    };
    for (std::size_t i = 0; i < text.size();) {
        const auto end = text.find(u' ', i);
        const auto word = text.substr(i, end == View::npos ? end : end - i);
        i = end == View::npos ? text.size() : end + 1;
        if (word.empty()) continue;

        auto spaced_word = std::u16string(word) + u' ';
        const auto width = dimensions(spaced_word, measure).width;
        if (width > text_width) {
            append_line();
            for (std::size_t j = 0; j < word.size();) {
                const auto start = j++;
                if (word[start] >= 0xd800 && word[start] <= 0xdbff && j < word.size()) ++j;
                line += word.substr(start, j - start);
                if (dimensions(line, measure).width >= text_width) {
                    if (j < word.size()) line += u'-';
                    append_line();
                }
            }
        } else {
            if (dimensions(line, measure).width + width >= text_width) append_line();
            if (!line.empty()) line += u' ';
            line += word;
        }
    }
    append_line();
    return result;
}

std::pair<View, View> entity_parts(View identifier) {
    const auto dot = identifier.find(u'.');
    if (dot == View::npos || identifier.find(u'.', dot + 1) != View::npos) return {{}, identifier};
    return {identifier.substr(0, dot), identifier.substr(dot + 1)};
}

enum class EntityKind { ui, processor, read_model, command, event };

EntityKind entity_kind(View kind) {
    if (kind == u"ui") return EntityKind::ui;
    if (kind == u"pcr" || kind == u"processor") return EntityKind::processor;
    if (kind == u"rmo" || kind == u"readmodel") return EntityKind::read_model;
    if (kind == u"cmd" || kind == u"command") return EntityKind::command;
    return EntityKind::event;
}

struct LaneCategory {
    int index;
    View label, prefix;
};

LaneCategory lane_category(EntityKind kind) {
    switch (kind) {
    case EntityKind::ui:
    case EntityKind::processor: return {0, u"UI/Automation", u"UI/A: "};
    case EntityKind::read_model:
    case EntityKind::command: return {100, u"Command/Read Model", u"C/RM: "};
    default: return {200, u"Events", u"Stream: "};
    }
}

struct Lane {
    std::u16string label;
    double right{}, y{}, max_box_height = 70;
};

struct Box {
    std::u16string content;
    EntityKind kind;
    int lane_index{};
    double x{}, width{}, height{};
};

struct Layout {
    std::map<int, Lane> lanes;
    std::vector<Box> boxes;
    std::vector<std::pair<std::size_t, std::size_t>> relations;
    double max_right{};
};

Layout layout(const EventmodelingDiagram& document, const EventmodelingTextMeasure& measure) {
    Layout result;
    for (std::size_t index = 0; index < document.frames.size(); ++index) {
        const auto& frame = document.frames[index];
        const auto kind = entity_kind(frame.kind);
        const auto [name_space, name] = entity_parts(frame.entity);
        const auto category = lane_category(kind);
        auto lane_index = category.index;
        auto label = std::u16string(category.label);
        if (!name_space.empty()) {
            // Upstream creates namespaced lanes without storing a namespace on
            // the lane object, so subsequent qualified frames allocate again.
            for (auto it = result.lanes.upper_bound(category.index);
                 it != result.lanes.end() && it->first < category.index + 100; ++it) {
                lane_index = it->first;
            }
            ++lane_index;
            label = std::u16string(category.prefix) + std::u16string(name_space);
        }
        auto& lane = result.lanes.try_emplace(lane_index, Lane{std::move(label)}).first->second;

        auto content = u"<b>" + wrap_label(safe_html_text(name), measure) + u"</b>";
        if (frame.has_data || !frame.data.empty()) {
            const auto data = wrap_label(safe_html_text(frame.data), measure);
            content += u"<br/><br/>";
            content += code_tag;
            for (const auto ch : data) {
                if (ch == u' ') content += u"&nbsp;";
                else content += ch;
            }
            if (frame.data_from_reference) content += u"<br/>";
            content += u"</code>";
        }
        auto metrics = dimensions(content, measure);
        // The pinned renderer measures literal label markup with SVG getBBox.
        // Its data path then scales that measured width before box padding.
        if (frame.has_data || !frame.data.empty()) metrics.width /= 3;
        const auto width = std::clamp(metrics.width + 20, 80.0, 450.0) + 2 * box_padding;
        const auto height = std::clamp(metrics.height + 20, 80.0, 750.0) + 2 * box_padding;

        auto x = start_x;
        if (!result.boxes.empty()) {
            const auto& previous = result.boxes.back();
            if (previous.lane_index == lane_index && lane.right != 0) {
                x = lane.right + box_padding;
            } else {
                x = previous.x + previous.width + 2 * box_padding - box_overlap;
            }
        }
        result.max_right = x + width + box_padding;
        for (const auto& [_, item] : result.lanes) result.max_right = std::max(result.max_right, item.right);
        lane.right = x + width;
        lane.max_box_height = std::max(lane.max_box_height, height);
        result.boxes.push_back({std::move(content), kind, lane_index, x, width, height});

        if (!frame.sources.empty()) {
            // Upstream filters the document in frame order and only draws
            // explicit references whose boxes have already been positioned.
            for (std::size_t source = 0; source <= index; ++source) {
                if (std::ranges::find(frame.sources, document.frames[source].id) != frame.sources.end()) {
                    result.relations.emplace_back(source, index);
                }
            }
        } else if (!frame.reset) {
            for (auto source = index; source > 0;) {
                if (result.boxes[--source].lane_index != lane_index) {
                    result.relations.emplace_back(source, index);
                    break;
                }
            }
        }
    }
    double y = 0;
    for (auto& [_, lane] : result.lanes) {
        lane.y = y;
        y += lane.max_box_height + 2 * lane_padding + lane_gap;
    }
    return result;
}

struct Bounds {
    double right{}, bottom{};
};

std::string write_lanes(const Layout& scene, const EventmodelingTheme& theme,
                        const EventmodelingTextMeasure& measure, Bounds& bounds) {
    std::string svg;
    for (const auto& [_, lane] : scene.lanes) {
        const auto width = scene.max_right + lane_padding;
        const auto height = lane.max_box_height + 2 * lane_padding;
        const auto label = measure_text(measure, lane.label, theme.font_family);
        bounds.right = std::max({bounds.right, width, 30 + label.width});
        bounds.bottom = std::max(bounds.bottom, lane.y + height);
        svg += "<g class=\"em-swimlane\"><rect x=\"0\" rx=\"3\"" + attribute("y", lane.y) +
            attribute("width", width) + attribute("height", height) +
            attribute("fill", theme.lane_fill) + attribute("stroke", theme.lane_stroke) +
            "/><text x=\"30\" font-weight=\"bold\"" + attribute("y", lane.y + 30) + ">" +
            escape(lane.label) + "</text></g>";
    }
    return svg;
}

std::string write_boxes(const Layout& scene, const EventmodelingTheme& theme, Bounds& bounds) {
    std::string svg;
    for (const auto& box : scene.boxes) {
        const auto y = scene.lanes.at(box.lane_index).y + lane_padding;
        const auto kind = static_cast<std::size_t>(box.kind);
        bounds.right = std::max(bounds.right, box.x + box.width);
        bounds.bottom = std::max(bounds.bottom, y + box.height);
        svg += "<g class=\"em-box\"><rect rx=\"3\"" + attribute("x", box.x) + attribute("y", y) +
            attribute("width", box.width) + attribute("height", box.height) +
            attribute("stroke", theme.strokes[kind]) + attribute("fill", theme.fills[kind]) +
            "/><foreignObject" + attribute("x", box.x + box_padding) + attribute("y", y + 10) +
            attribute("width", box.width - 2 * box_padding) +
            attribute("height", box.height - 2 * box_padding) +
            "><div xmlns=\"http://www.w3.org/1999/xhtml\" style=\"display:table;height:100%;width:100%\">"
            "<span style=\"display:table-cell;text-align:center;vertical-align:middle\">" +
            label_markup(box.content) + "</span></div></foreignObject></g>";
    }
    return svg;
}

std::string write_relations(const Layout& scene, const EventmodelingTheme& theme) {
    std::string svg;
    for (const auto [source_index, target_index] : scene.relations) {
        const auto& source = scene.boxes[source_index];
        const auto& target = scene.boxes[target_index];
        auto source_y = scene.lanes.at(source.lane_index).y + lane_padding;
        auto target_y = scene.lanes.at(target.lane_index).y + lane_padding;
        if (source_y > target_y) target_y += target.height;
        else source_y += source.height;
        const auto path = "M" + number(source.x + source.width * 2 / 3) + " " + number(source_y) +
            " L" + number(target.x + target.width / 3) + " " + number(target_y);
        svg += "<path class=\"em-relation\" fill=\"none\" stroke-width=\"1\""
            " marker-end=\"url(#em-arrowhead)\"" + attribute("stroke", theme.relation_stroke) +
            attribute("d", path) + "/>";
    }
    return svg;
}

}

std::expected<EventmodelingRenderInput, std::string> prepare_eventmodeling_render(const PreparedDiagram& prepared) {
    auto document = parse_eventmodeling(prepared.code.with_comments);
    if (!document) return std::unexpected(document.error());
    EventmodelingRenderInput input;
    input.document = std::move(*document);
    if (const auto font = prepared.effective_config.find(u"fontFamily"); font && font->kind == ConfigKind::string) {
        input.theme.font_family = font->string;
    }
    if (const auto vars = prepared.effective_config.find(u"themeVariables")) {
        auto set = [&](View key, std::u16string& target) {
            if (const auto value = vars->find(key); value && value->kind == ConfigKind::string) target = value->string;
        };
        set(u"textColor", input.theme.text);
        set(u"emSwimlaneBackgroundOdd", input.theme.lane_fill);
        set(u"emSwimlaneBackgroundStroke", input.theme.lane_stroke);
        set(u"emRelationStroke", input.theme.relation_stroke);
        set(u"emArrowhead", input.theme.arrowhead);
        constexpr View fills[] = {u"emUiFill", u"emProcessorFill", u"emReadModelFill", u"emCommandFill", u"emEventFill"};
        constexpr View strokes[] = {u"emUiStroke", u"emProcessorStroke", u"emReadModelStroke", u"emCommandStroke", u"emEventStroke"};
        for (std::size_t i = 0; i < std::size(fills); ++i) {
            set(fills[i], input.theme.fills[i]);
            set(strokes[i], input.theme.strokes[i]);
        }
    }
    if (const auto config = prepared.effective_config.find(u"eventmodeling")) {
        if (const auto value = config->find(u"padding"); value && value->kind == ConfigKind::number) input.padding = value->number;
        if (const auto value = config->find(u"useMaxWidth"); value && value->kind == ConfigKind::boolean) input.use_max_width = value->boolean;
    }
    return input;
}

std::expected<EventmodelingSvg, std::string> render_eventmodeling_svg(
    const EventmodelingRenderInput& input, const EventmodelingTextMeasure& measure) {
    if (!measure) return std::unexpected("Event Modeling rendering requires real text measurements");
    try {
        const auto scene = layout(input.document, measure);
        Bounds bounds;
        auto children = write_lanes(scene, input.theme, measure, bounds);
        children += write_boxes(scene, input.theme, bounds);
        children += write_relations(scene, input.theme);
        const auto width = bounds.right + 2 * input.padding;
        const auto height = bounds.bottom + 2 * input.padding;
        const auto origin = number(-input.padding);
        std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" role=\"graphics-document document\""
            " aria-roledescription=\"eventmodeling\" font-size=\"16\"" +
            attribute("viewBox", origin + " " + origin + " " + number(width) + " " + number(height)) +
            attribute("font-family", input.theme.font_family) + attribute("fill", input.theme.text);
        if (input.use_max_width) svg += " width=\"100%\"" + attribute("style", "max-width: " + number(width) + "px;");
        else svg += attribute("width", width) + attribute("height", height);
        const auto& document = input.document;
        if (!document.acc_title.empty()) svg += " aria-labelledby=\"em-accessible-title\"";
        if (!document.acc_description.empty()) svg += " aria-describedby=\"em-accessible-description\"";
        svg += ">";
        if (!document.acc_title.empty()) svg += "<title id=\"em-accessible-title\">" + escape(document.acc_title) + "</title>";
        if (!document.acc_description.empty()) svg += "<desc id=\"em-accessible-description\">" + escape(document.acc_description) + "</desc>";
        svg += children + "<defs><marker id=\"em-arrowhead\" markerWidth=\"10\" markerHeight=\"7\""
            " refX=\"10\" refY=\"3.5\" orient=\"auto\"><polygon points=\"0 0, 10 3.5, 0 7\"" +
            attribute("fill", input.theme.arrowhead) + "/></marker></defs></svg>";
        return EventmodelingSvg{std::move(svg), width, height};
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}

}
