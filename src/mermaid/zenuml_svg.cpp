module folia.mermaid.zenuml_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
import folia.mermaid.flowchart_label;
namespace folia::mermaid {
namespace {
#include "zenuml_assets.inc"
#include "zen_fragment_icons.inc"
#include "zenuml_comments.inc"
std::string num(double v) {
    auto n = format_number(v);
    if (!n)
        throw std::runtime_error("Invalid ZenUML geometry");
    return *n;
}
std::string esc(std::u16string_view s) {
    auto e = escape_svg_text(s, u"ZenUML text");
    if (!e)
        throw std::runtime_error("Invalid ZenUML SVG text");
    return *e;
}
std::string attr(std::string_view key, double n) {
    return " " + std::string(key) + "=\"" + num(n) + "\"";
}
std::string rect(double x, double y, double w, double h, std::string_view cls, double radius = 0) {
    return "<rect" + attr("x", x) + attr("y", y) + attr("width", w) + attr("height", h) + attr("rx", radius) +
           " class=\"" + std::string(cls) + "\"/>";
}
std::string text(double x, double y, std::u16string_view label, std::string_view cls,
                 std::string_view anchor = "start", bool central = false, double opacity = 1) {
    return "<text" + (opacity == 1 ? std::string{} : attr("opacity", opacity)) + attr("x", x) + attr("y", y) +
           " text-anchor=\"" + std::string(anchor) + "\" class=\"" + std::string(cls) + "\"" +
           (central ? " dominant-baseline=\"central\"" : "") + ">" + esc(label) + "</text>";
}
std::string line(double x, double y, double x2, double y2, std::string_view cls,
                 std::string_view extra = "") {
    return "<line" + attr("x1", x) + attr("y1", y) + attr("x2", x2) + attr("y2", y2) + " class=\"" +
           std::string(cls) + "\"" + std::string(extra) + "/>";
}
std::string icon(double x, double y, std::string_view value) {
    return "<g class=\"participant-icon\" transform=\"translate(" + num(x) + "," + num(y) +
           ")\" fill=\"none\">" + std::string(value) + "</g>";
}
std::u16string number(std::size_t n) {
    auto s = std::to_string(n);
    return {s.begin(), s.end()};
}
std::string arrow(double x, double y, bool left, bool open) {
    return "<svg" + attr("x", left ? x : x - 7) + attr("y", y - 5) +
           " width=\"7\" height=\"10\" viewBox=\"0 0 7 9\" overflow=\"visible\" class=\"arrow-head" +
           (open ? " arrow-open" : "") + "\"><g" +
           (left ? " transform=\"scale(-1, 1) translate(-7, 0)\"" : "") +
           "><path d=\"M1 1.25 L6.15 4.5 L1 7.75" + (open ? "" : " Z") +
           "\" stroke=\"#000\" stroke-linecap=\"round\" stroke-width=\"2\" fill=\"" +
           (open ? "none" : "#000") + "\"/></g></svg>";
}
std::string open_arrow(double x, double y, bool left, std::string_view cls) {
    double a = x + (left ? 5.15 : -5.15);
    return "<polyline points=\"" + num(a) + "," + num(y - 3.25) + " " + num(x) + "," + num(y) + " " + num(a) +
           "," + num(y + 3.25) + "\" fill=\"none\" stroke-linecap=\"round\" class=\"" + std::string(cls) +
           "\"/>";
}
struct Painter {
    const ZenDocument &doc;
    const ZenLayout &layout;
    const ZenSvgMeasure &measure;
    std::vector<int> depth;
    std::vector<double> py, pw;
    double extra{}, borderLeft{}, borderRight{}, maxReturn{};
    struct FragmentRegion {
        const ZenStatement *statement{};
        ZenCoordinate coordinate;
        std::u16string sequence;
        double x{}, width{}, inset{};
        std::size_t nesting{};
    };
    std::vector<FragmentRegion> regions;
    struct ReturnAdjustment {
        double shift{}, inner{};
    };
    std::map<const ZenStatement *, ReturnAdjustment> adjustments;
    void prepare_return_adjustments() {
        struct StatementInfo {
            const ZenStatement *statement{};
            std::size_t depth{};
            bool syncParent{}, sectionReset{};
        };
        std::vector<StatementInfo> statements;
        std::function<void(const std::vector<ZenStatement> &, std::size_t, bool, bool)> collect =
            [&](const auto &list, std::size_t level, bool syncParent, bool reset) {
                for (std::size_t i = 0; i < list.size(); ++i) {
                    const auto &statement = list[i];
                    statements.push_back({&statement, level, syncParent, reset && i == 0});
                    if (zen_fragment(statement.kind)) {
                        for (std::size_t section = 0; section < statement.sections.size(); ++section)
                            collect(statement.sections[section].statements, level + 1, false, section > 0);
                    } else if (statement.body)
                        collect(statement.children, level + 1, statement.kind == ZenKind::sync, false);
                }
            };
        collect(doc.statements, 0, false, false);
        struct Scope {
            double debt{}, direct{}, localShift{};
            const ZenStatement *owner{};
            bool assignment{};
        };
        std::vector<Scope> scopes(1);
        const auto close = [&] {
            const auto scope = scopes.back();
            scopes.pop_back();
            const auto inner = scope.debt - scope.direct + std::max(scope.direct - 16, 0.0);
            if (scope.owner) {
                adjustments[scope.owner].inner = inner;
                if (scope.owner->kind == ZenKind::sync)
                    scopes.back().debt += inner + (scope.assignment ? 4 : 0);
            }
        };
        for (const auto &info : statements) {
            while (scopes.size() > info.depth + 1)
                close();
            while (scopes.size() < info.depth + 1)
                scopes.emplace_back();
            auto &scope = scopes.back();
            if (info.sectionReset) {
                scope.debt = 0;
                scope.direct = 0;
            }
            double shift = 0;
            for (const auto &current : scopes)
                shift += current.debt + current.localShift;
            adjustments[info.statement].shift = shift;
            const auto &statement = *info.statement;
            if (statement.kind == ZenKind::returned && statement.from != statement.to &&
                coordinate(statement).height <= 0) {
                const bool first = scope.direct == 0;
                scope.debt += 16;
                scope.direct += 16;
                if (first && shift != 0 && info.syncParent)
                    ++scope.localShift;
            }
            if ((statement.kind == ZenKind::sync || statement.kind == ZenKind::creation) && statement.body) {
                scopes.push_back({0, 0, 0, &statement,
                                  statement.kind == ZenKind::sync && !statement.assignment.empty() &&
                                      statement.from != statement.to});
            }
        }
        while (scopes.size() > 1)
            close();
    }
    std::string occurrences, messages, selfs, creations, returns, fragments, comments;
    std::size_t index(std::u16string_view s) const {
        for (std::size_t i = 0; i < doc.participants.size(); ++i)
            if (doc.participants[i].name == s)
                return i;
        throw std::runtime_error("Unknown ZenUML participant");
    }
    const ZenCoordinate &coordinate(const ZenStatement &s) const {
        for (const auto &c : layout.vertical)
            if (c.begin == s.begin)
                return c;
        throw std::runtime_error("Missing ZenUML coordinate");
    }
    void ret(double from, double to, double y, const std::u16string &label, const std::u16string &seq,
             bool self) {
        if (self) {
            returns += "<g class=\"return return-self\"><g transform=\"translate(" + num(from + 4) + "," +
                       num(y - 12) + ") scale(" + num(12.0 / 512) + ")\">" + std::string(zen_return) +
                       "</g>" + text(from + 20, y - 1, label, "return-label") + "</g>";
            return;
        }
        y = std::floor(y);
        returns += "<g class=\"return\">" + line(from, y, to, y, "return-line") +
                   open_arrow(to, y, to < from, "return-arrow") +
                   text((from + to) / 2 + (to < from ? 3.5 : -3.5), y - 3, label, "return-label", "middle") +
                   text(std::min(from, to) - 4, y - 3, seq, "seq-number", "end") + "</g>";
    }
    void draw_comment(const ZenStatement &statement, double x, double y) {
        const auto lines = zen_comment_runs(statement.comment);
        comments += "<text class=\"comment-text\">";
        for (std::size_t i = 0; i < lines.size(); ++i) {
            comments += "<tspan" + attr("x", x) + (i ? attr("dy", 20) : attr("y", y)) + ">";
            for (const auto &run : lines[i]) {
                if (run.strong || run.emphasis) {
                    comments += "<tspan" + std::string(run.strong ? " font-weight=\"bold\"" : "") +
                                (run.emphasis ? " font-style=\"italic\"" : "") + ">" + esc(run.text) +
                                "</tspan>";
                } else
                    comments += esc(run.text);
            }
            comments += "</tspan>";
        }
        comments += "</text>";
    }
    std::string bracketed(double x, double y, std::u16string_view value, std::string_view cls) {
        return text(x, y, u"[", cls) + text(x + 7.89, y, value, cls, "start", false, .65) +
               text(x + 11.89 + measure(value, 14, false), y, u"]", cls);
    }
    std::string fragment_icon(ZenKind kind, double x, double y) {
        if (kind == ZenKind::loop)
            return "<svg" + attr("x", x) + attr("y", y) +
                   " width=\"20\" height=\"24\" viewBox=\"0 0 1024 1024\" fill=\"#000\" stroke=\"none\">" +
                   std::string(zen_loop) + "</svg>";
        std::string_view glyph;
        switch (kind) {
        case ZenKind::optional:
            glyph = zen_fragment_opt;
            break;
        case ZenKind::parallel:
            glyph = zen_fragment_par;
            break;
        case ZenKind::critical:
            glyph = zen_fragment_critical;
            break;
        case ZenKind::section:
            glyph = zen_fragment_section;
            break;
        case ZenKind::try_catch:
            glyph = zen_fragment_tcf;
            break;
        default:
            glyph = "<svg viewBox=\"0 0 24 24\" fill=\"none\"><path d=\"M12 8L20 12L12 16L4 12L12 8Z\" "
                    "stroke=\"#000\" stroke-width=\"1.5\" stroke-linecap=\"round\" "
                    "stroke-linejoin=\"round\"/></svg>";
            break;
        }
        return "<svg" + attr("x", x) + attr("y", y) + " width=\"20\" height=\"24\"" +
               std::string(glyph.substr(4));
    }
    std::pair<std::size_t, std::size_t> members(const ZenStatement &statement) const {
        auto first = doc.participants.size();
        std::size_t last = 0;
        const auto include = [&](std::u16string_view name) {
            for (std::size_t i = 0; i < doc.participants.size(); ++i)
                if (doc.participants[i].name == name) {
                    first = std::min(first, i);
                    last = std::max(last, i);
                    break;
                }
        };
        include(statement.from);
        std::function<void(const std::vector<ZenStatement> &)> visit = [&](const auto &list) {
            for (const auto &item : list) {
                if (!zen_fragment(item.kind)) {
                    include(item.from);
                    include(item.to);
                }
                visit(item.children);
                for (const auto &section : item.sections)
                    visit(section.statements);
            }
        };
        for (const auto &section : statement.sections)
            visit(section.statements);
        return {first, last};
    }
    std::pair<double, double> frame_border(const ZenStatement &statement) const {
        const auto [first, last] = members(statement);
        double left = 0, right = 0;
        std::function<void(const std::vector<ZenStatement> &)> visit = [&](const auto &list) {
            for (const auto &item : list) {
                if (zen_fragment(item.kind)) {
                    const auto [childFirst, childLast] = members(item);
                    const auto [childLeft, childRight] = frame_border(item);
                    if (first == childFirst)
                        left = std::max(left, childLeft);
                    if (last == childLast)
                        right = std::max(right, childRight);
                } else
                    visit(item.children);
            }
        };
        for (const auto &section : statement.sections)
            visit(section.statements);
        return {left + 10, right + 10};
    }
    void fragment(const ZenStatement &statement, const ZenCoordinate &coordinateValue,
                  const std::u16string &sequence, int nesting) {
        const auto [first, last] = members(statement);
        const auto [left, right] = frame_border(statement);
        // FrameBuilder follows the first top-level fragment tree, independently for
        // its left and right boundary participants. Sibling fragments do not add
        // padding to that frame merely because they are nested more deeply.
        if (regions.empty()) {
            borderLeft = left;
            borderRight = right;
        }
        double selfExtra = 0;
        std::function<void(const std::vector<ZenStatement> &)> widths = [&](const auto &list) {
            for (const auto &item : list) {
                if (!zen_fragment(item.kind) && item.from == item.to && first < doc.participants.size()) {
                    const auto i = index(item.to);
                    selfExtra = std::max(selfExtra, std::floor(measure(item.text, 16, false) + .5) -
                                                        (layout.positions[last] - layout.positions[i]) -
                                                        layout.widths[last] / 2);
                }
                widths(item.children);
                for (const auto &section : item.sections)
                    widths(section.statements);
            }
        };
        for (const auto &section : statement.sections)
            widths(section.statements);
        const auto x =
            first == doc.participants.size() ? 0 : layout.positions[first] - layout.widths[first] / 2;
        const auto width = first == doc.participants.size()
                               ? std::max(100.0, layout.width)
                               : std::max(100.0, layout.positions[last] - layout.positions[first] +
                                                     (layout.widths[first] + layout.widths[last]) / 2) +
                                     left + right + selfExtra;
        regions.push_back({&statement, coordinateValue, sequence, x, width,
                           first == doc.participants.size() ? 0 : left + layout.widths[first] / 2});
        std::size_t ordinal = 0;
        for (const auto &section : statement.sections) {
            walk(section.statements, sequence + u".", ordinal, nesting + 1);
            ordinal += section.statements.size();
        }
    }
    void paint_fragment(const FragmentRegion &region) {
        const auto &statement = *region.statement;
        const auto &coordinateValue = region.coordinate;
        const auto &sequence = region.sequence;
        const auto x = region.x, width = region.width;
        const auto top = coordinateValue.top;
        const auto commentHeight =
            statement.comment.empty() ? 0 : (1 + std::ranges::count(statement.comment, u'\n')) * 20;
        const auto headerTop = top + commentHeight;
        std::u16string_view label = u"Alt";
        switch (statement.kind) {
        case ZenKind::loop:
            label = u"Loop";
            break;
        case ZenKind::optional:
            label = u"Opt";
            break;
        case ZenKind::parallel:
            label = u"Par";
            break;
        case ZenKind::critical:
            label = u"Critical";
            break;
        case ZenKind::section:
            label = u"Section";
            break;
        case ZenKind::try_catch:
            label = u"Try";
            break;
        default:
            break;
        }
        fragments += "<g class=\"fragment\">" +
                     rect(x + .5, top + .5, width - 1, coordinateValue.height - 1, "fragment-border", 4) +
                     rect(x + 1, headerTop + 1, width - 2, 25, "fragment-header") +
                     fragment_icon(statement.kind, x + 5, headerTop + 1) +
                     text(x + 27, headerTop + 13, label, "fragment-label", "start", true) +
                     text(x - 3, headerTop + 9, sequence, "seq-number", "end", true);
        if (!statement.condition.empty())
            fragments += bracketed(x + 1, headerTop + 41, statement.condition, "fragment-condition");
        if (!statement.comment.empty())
            draw_comment(statement, x + 1, top + 15);
        for (std::size_t i = 1; i < statement.sections.size(); ++i) {
            const auto &section = statement.sections[i];
            if (section.statements.empty())
                continue;
            const auto y = coordinate(section.statements.front()).top - 37;
            fragments += line(x + 1, y + .5, x + width - 1, y + .5, "fragment-separator");
            if (section.label.size() > 2 && section.label.front() == u'[' && section.label != u"[else]") {
                fragments += bracketed(x + 1, y + 16,
                                       std::u16string_view(section.label).substr(1, section.label.size() - 2),
                                       "fragment-section-label");
            } else if (section.label == u"[else]")
                fragments += text(x + 5, y + 16, section.label, "fragment-section-label");
            else {
                const auto split = section.label.find(u' ');
                const auto firstPart = std::u16string_view(section.label).substr(0, split);
                const auto firstWidth = measure(firstPart, 14, false);
                const auto detail = split == section.label.npos
                                        ? std::u16string_view{}
                                        : std::u16string_view(section.label).substr(split + 1);
                const auto backgroundWidth =
                    firstWidth + (detail.empty() ? 8 : measure(detail, 14, false) + 16);
                fragments += "<g opacity=\"0.65\"><rect" + attr("x", x + 1) + attr("y", y + 1) +
                             attr("width", backgroundWidth) + " height=\"20\" fill=\"#fff\"/>" +
                             text(x + 5, y + 16, firstPart, "fragment-section-label");
                if (!detail.empty())
                    fragments += text(x + 13 + firstWidth, y + 16, detail, "fragment-section-label");
                fragments += "</g>";
            }
        }
        if (statement.kind == ZenKind::parallel && !statement.sections.empty()) {
            const auto &children = statement.sections.front().statements;
            for (std::size_t i = 1; i < children.size(); ++i) {
                const auto y = coordinate(children[i]).top - .5;
                const auto inset = region.inset;
                fragments += line(x + 1 + inset, y, x + width - 1, y, "fragment-separator");
            }
        }
        fragments += "</g>";
    }
    void paint_fragments(double diagramWidth) {
        for (auto &region : regions)
            region.x -= borderLeft;
        // Preserve upstream's iteration order: each outer region's left edge is
        // adjusted before testing containment for the next region.
        for (auto &inner : regions) {
            for (const auto &outer : regions) {
                if (&inner == &outer)
                    continue;
                if (outer.x <= inner.x && outer.coordinate.top <= inner.coordinate.top &&
                    outer.x + outer.width >= inner.x + inner.width &&
                    outer.coordinate.top + outer.coordinate.height >=
                        inner.coordinate.top + inner.coordinate.height)
                    ++inner.nesting;
            }
            inner.x += inner.nesting * 10;
        }
        for (auto &region : regions) {
            const auto target = diagramWidth + borderRight - region.nesting * 10;
            const auto current = region.x + region.width;
            if (current >= target - 20 && current < target)
                region.width = target - region.x;
            paint_fragment(region);
        }
    }
    void walk(const std::vector<ZenStatement> &list, std::u16string prefix = {}, std::size_t start = 0,
              int nesting = 0) {
        for (std::size_t ordinal = 0; ordinal < list.size(); ++ordinal) {
            const auto &s = list[ordinal];
            auto c = coordinate(s);
            c.top += adjustments[&s].shift;
            auto seq = prefix + number(start + ordinal + 1);
            if (zen_fragment(s.kind)) {
                fragment(s, c, seq, nesting);
                continue;
            }
            if (!s.comment.empty()) {
                const auto sender = index(s.from);
                draw_comment(s, layout.positions[sender] + (depth[sender] ? 8 : 1), c.top + 15);
                const auto commentHeight = (1 + std::ranges::count(s.comment, u'\n')) * 20;
                if (s.kind != ZenKind::returned) {
                    c.top += commentHeight;
                    c.height -= commentHeight;
                }
            }
            auto a = index(s.from), b = index(s.to);
            double from = layout.positions[a], to = layout.positions[b];
            bool self = a == b;
            auto label = s.label.empty() ? s.text : s.label;
            if (s.kind == ZenKind::returned) {
                const bool reverse = to < from;
                const auto returnFrom = reverse ? (depth[a] ? from + std::max(depth[a] - 1, 0) * 7 - 7 : from)
                                                : from + depth[a] * 7 + 1;
                const auto returnTo = reverse ? to + depth[b] * 7 + 1
                                              : (depth[b] ? to + std::max(depth[b] - 1, 0) * 7 - 7 : to);
                const auto y = c.top + (c.height == 0 ? 16.5 : 15.5);
                ret(returnFrom, returnTo, y, label, seq, self);
                maxReturn = std::max(maxReturn, y + (c.height == 0 ? 45.5 : 44.5));
                continue;
            }
            double sender = from;
            if (depth[a] && !self)
                sender += from < to ? depth[a] * 7 : (depth[a] - 2) * 7;
            if (s.kind == ZenKind::creation) {
                py[b] = c.top;
                double y = c.top + 20, edge = to + (to < sender ? pw[b] / 2 : -pw[b] / 2),
                       begin = sender + (sender < to ? 1 : 0);
                std::string labelContent = esc(label);
                if (label.size() > 2 && label.front() == u'\u00ab' && label != u"\u00abcreate\u00bb")
                    labelContent = "<tspan>\xc2\xab</tspan><tspan dx=\"4\">" +
                                   esc(std::u16string_view(label).substr(1, label.size() - 2)) +
                                   "</tspan><tspan dx=\"4\">\xc2\xbb</tspan>";
                creations += "<g class=\"creation\">" +
                             line(begin, y, edge, y, "message-line", " stroke-dasharray=\"6,4\"") +
                             open_arrow(edge, y, to < sender, "arrow-head arrow-open") + "<text" +
                             attr("x", (begin + edge) / 2 + (to < sender ? 3.5 : -3)) + attr("y", y - 3) +
                             " text-anchor=\"middle\" class=\"message-label\">" + labelContent + "</text>" +
                             text(std::min(begin, edge) - 4, y - 3, seq, "seq-number", "end") + "</g>";
                occurrences +=
                    rect(to - 6, c.top + 39, 13, std::max(c.height - 38, 24.0) - 2, "occurrence", 1);
            } else {
                double y = c.top + (self ? 30 : 16) - .5, offset = depth[b] * 7;
                if (self) {
                    const auto x = from + (depth[a] ? depth[a] * 7 : 0);
                    const bool asynchronous = s.kind == ZenKind::async;
                    selfs += "<g class=\"message self-call\"><svg" + attr("x", x + 1) +
                             attr("y", c.top + (asynchronous ? 20 : 14)) + " width=\"30\" height=\"24\">" +
                             "<path d=\"M0,2 L26,2 Q28,2 28,4 L28,13 Q28,15 26,15 " +
                             (asynchronous ? "L1,15" : "L14,15") +
                             "\" fill=\"none\" stroke=\"#000\" stroke-width=\"2\"/>" +
                             "<g transform=\"translate(" + (asynchronous ? "0" : "7") +
                             ", 10)\"><svg height=\"10\" width=\"7\" viewBox=\"0 0 7 9\"><g "
                             "transform=\"scale(-1, 1) translate(-7, 0)\">" +
                             "<path d=\"M1 1.25 L6.15 4.5 L1 7.75" + (asynchronous ? "" : " Z") +
                             "\" stroke=\"#000\" stroke-linecap=\"round\" fill=\"" +
                             (asynchronous ? "none" : "#000") +
                             "\" stroke-width=\"2\"/></g></svg></g></svg>" +
                             text(x + 6, c.top + (asynchronous ? 15 : 12), label, "message-label") +
                             text(x - 3, c.top + 12, seq, "seq-number", "end") + "</g>";
                    extra = std::max(extra, std::floor(measure(label, 16, false) + .5) -
                                                (layout.positions.back() - to) - layout.widths.back() / 2);
                } else {
                    double end = to;
                    if (s.kind == ZenKind::sync)
                        end += sender < to ? -7 + offset : 7 + offset;
                    bool right = sender < end;
                    double x1 = sender + (right ? 1 : 0), x2 = end + (right ? 0 : 1);
                    messages += "<g class=\"message\">" + line(x1, y - .5, x2, y - .5, "message-line") +
                                arrow(x2, y - .5, !right, s.kind == ZenKind::async) +
                                text((sender + end) / 2 + (right ? -3.5 : 3.5) + .5, y - 3.5, label,
                                     "message-label", "middle") +
                                text(std::min(x1, x2) - 4, y - 3.5, seq, "seq-number", "end") + "</g>";
                }
                if (s.kind == ZenKind::sync) {
                    double h = c.height - (self ? 30 : 16) + 2 + adjustments[&s].inner +
                               (!s.assignment.empty() && !self && s.body ? 4 : 0),
                           x = to - 7 + offset;
                    occurrences += rect(x + 1, y - .5, 13, h - 2, "occurrence", 1);
                    if (!s.assignment.empty() && !self) {
                        const auto bottom = y - 1.5 + h - 2;
                        ret(to - 6 + offset, sender + 2, bottom, s.assignment,
                            seq + u"." + number(s.children.size() + 1), false);
                        maxReturn = std::max(maxReturn, bottom + 46);
                    }
                }
            }
            if (s.kind == ZenKind::sync || s.kind == ZenKind::creation) {
                ++depth[b];
                walk(s.children, seq + u".", 0, nesting);
                --depth[b];
            }
        }
    }
};
} // namespace
std::expected<ZenSvg, std::string> render_zenuml_svg(const ZenDocument &doc, const ZenSvgMeasure &measure) {
    if (!measure)
        return std::unexpected("ZenUML requires real text measurements");
    try {
        auto layout = layout_zenuml(doc, [&](auto t) { return measure(t, 16, false); });
        if (!layout)
            return std::unexpected(layout.error());
        if (doc.participants.empty())
            return ZenSvg{"<svg></svg>", 0, 0};
        Painter p{doc, *layout, measure};
        p.depth.resize(doc.participants.size());
        p.py.assign(doc.participants.size(), 28);
        for (const auto &v : doc.participants) {
            auto label = v.label.empty() ? v.name : v.label;
            p.pw.push_back(
                v.name == u"_STARTER_"
                    ? 80
                    : std::clamp(measure(label, 16, false) + 16 + (v.type.empty() ? 0 : 28), 80.0, 250.0));
        }
        p.prepare_return_adjustments();
        p.walk(doc.statements);
        double height = std::max(layout->height + 28, p.maxReturn), width = layout->width + p.extra,
               viewWidth = width + 22 + p.borderLeft + p.borderRight, viewHeight = height + 47;
        p.paint_fragments(width);
        std::string life, parts, createdParts;
        for (std::size_t i = 0; i < doc.participants.size(); ++i) {
            auto &v = doc.participants[i];
            double x = layout->positions[i], y = p.py[i], w = p.pw[i];
            life += "<g class=\"lifeline\">" +
                    line(x + .5, y + 40, x + .5, height + 12, "lifeline",
                         " stroke-dasharray=\"5,5\" stroke-dashoffset=\"5\" shape-rendering=\"crispEdges\"") +
                    "</g>";
            auto &participantParts = y == 28 ? parts : createdParts;
            participantParts +=
                "<g class=\"participant\">" + rect(x - w / 2 + 1, y + 1, w - 2, 38, "participant-box", 3);
            if (v.name == u"_STARTER_")
                participantParts += icon(x - 14, y + 8, zen_actor);
            else {
                auto label = v.label.empty() ? v.name : v.label;
                if (!v.type.empty()) {
                    double group = x - (44 + measure(label, 16, false)) / 2;
                    participantParts +=
                        icon(group + 4, y + 8, v.type == u"Actor" ? zen_actor : zen_database) +
                        text(group + 36, y + 19.75, label, "participant-label", "start", true);
                } else
                    participantParts += text(x, y + 19.75, label, "participant-label", "middle", true);
            }
            participantParts += "</g>";
        }
        std::string frame = "<rect x=\"0\" y=\"0\"" + attr("width", viewWidth) + attr("height", viewHeight) +
                            " rx=\"4\" fill=\"#666\"/><rect x=\"1\" y=\"1\"" + attr("width", viewWidth - 2) +
                            attr("height", viewHeight - 2) + " rx=\"3\" fill=\"#fff\"/>" +
                            line(1, 33.5, viewWidth - 1, 33.5, "frame-header-line");
        if (!doc.title.empty())
            frame += text(5, 16.75, doc.title, "frame-title", "start", true);
        auto svg = "<svg xmlns=\"http://www.w3.org/2000/svg\"" + attr("width", viewWidth) +
                   attr("height", viewHeight) + " viewBox=\"0 0 " + num(viewWidth) + " " + num(viewHeight) +
                   "\"><defs><style>" + std::string(zen_styles) + "</style></defs>" + frame +
                   "<g transform=\"translate(" + num(11 + p.borderLeft) + ",34)\">" + life + parts +
                   p.occurrences + createdParts + p.messages + p.selfs + p.creations + p.returns +
                   p.fragments + p.comments + "</g></svg>";
        return ZenSvg{std::move(svg), viewWidth, viewHeight};
    } catch (const std::exception &e) {
        return std::unexpected(e.what());
    }
}
} // namespace folia::mermaid
