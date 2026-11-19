module folia.mermaid.wardley_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
namespace folia::mermaid {
namespace {
using V = std::u16string_view;
const ConfigValue *get(const ConfigValue &c, V k) { return c.find(k); }
std::u16string str(const ConfigValue &c, V k, std::u16string fallback = {}) {
  auto p = get(c, k);
  return p && p->kind == ConfigKind::string ? p->string : std::move(fallback);
}
double num(const ConfigValue &c, V k, double f) {
  auto p = get(c, k);
  return p && p->kind == ConfigKind::number ? p->number : f;
}
bool boolean(const ConfigValue &c, V k, bool f) {
  auto p = get(c, k);
  return p && p->kind == ConfigKind::boolean ? p->boolean : f;
}
std::string n(double x) {
  auto r = format_number(x);
  if (!r)
    throw std::runtime_error("nonfinite Wardley geometry");
  return *r;
}
std::string esc(V s) {
  auto r = escape_svg_text(s, u"Wardley text");
  if (!r)
    throw std::runtime_error("invalid Wardley text");
  return *r;
}
std::string q(V s) {
  auto r = esc(s);
  std::string o;
  for (char c : r) {
    if (c == '"')
      o += "&quot;";
    else
      o += c;
  }
  return o;
}
std::string line(double x1, double y1, double x2, double y2,
                 std::string_view cls, std::string_view color,
                 std::string_view extra = {}) {
  return "<line class=\"" + std::string(cls) + "\" x1=\"" + n(x1) + "\" y1=\"" +
         n(y1) + "\" x2=\"" + n(x2) + "\" y2=\"" + n(y2) + "\" stroke=\"" +
         std::string(color) + "\"" + std::string(extra) + "/>";
}
} // namespace
std::expected<WardleyRenderInput, std::string>
prepare_wardley_render(const PreparedDiagram &p) {
  auto parsed = parse_wardley(encode_diagram_source(p.code.with_comments));
  if (!parsed)
    return std::unexpected(parsed.error());
  WardleyRenderInput r;
  r.document = std::move(*parsed);
  if (r.document.title.empty() && p.title)
    r.document.title = *p.title;
  const auto &c = p.effective_config;
  const auto *wc = c.find(u"wardley-beta");
  if (wc) {
    r.document.width = num(*wc, u"width", r.document.width);
    r.document.height = num(*wc, u"height", r.document.height);
    r.padding = num(*wc, u"padding", r.padding);
    r.node_radius = num(*wc, u"nodeRadius", r.node_radius);
    r.label_offset = num(*wc, u"nodeLabelOffset", r.label_offset);
    r.axis_font = num(*wc, u"axisFontSize", r.axis_font);
    r.label_font = num(*wc, u"labelFontSize", r.label_font);
    r.show_grid = boolean(*wc, u"showGrid", false);
    r.use_max_width = boolean(*wc, u"useMaxWidth", true);
  }
  const auto *vars = c.find(u"themeVariables");
  if (vars) {
    r.theme.font_family = str(*vars, u"fontFamily", u"Arial");
    auto color = [&](const ConfigValue &obj, V k, std::string &dst,
                     V fallback) {
      auto v = str(obj, k, std::u16string(fallback.begin(), fallback.end()));
      dst = std::string(v.begin(), v.end());
    };
    color(*vars, u"background", r.theme.background, u"#ffffff");
    color(*vars, u"primaryTextColor", r.theme.text, u"#222222");
    if (auto wardley = vars->find(u"wardley")) {
      color(*wardley, u"backgroundColor", r.theme.background, u"#ffffff");
      color(*wardley, u"axisColor", r.theme.axis, u"#000000");
      color(*wardley, u"axisTextColor", r.theme.text, u"#222222");
      color(*wardley, u"gridColor", r.theme.grid, u"rgba(100, 100, 100, 0.2)");
      color(*wardley, u"componentFill", r.theme.component_fill, u"#ffffff");
      color(*wardley, u"componentStroke", r.theme.component_stroke, u"#000000");
      color(*wardley, u"linkStroke", r.theme.link, u"#000000");
      color(*wardley, u"evolutionStroke", r.theme.evolution, u"#dc3545");
      color(*wardley, u"annotationStroke", r.theme.annotation_stroke,
            u"#000000");
      color(*wardley, u"annotationTextColor", r.theme.annotation_text,
            u"#222222");
      color(*wardley, u"annotationFill", r.theme.annotation_fill, u"#ffffff");
    }
  }
  return r;
}
std::expected<WardleySvg, std::string>
render_wardley_svg(const WardleyRenderInput &in,
                   const SvgTextMeasure &measure) {
  try {
    const auto &d = in.document;
    const double w = d.width, h = d.height, p = in.padding, cw = w - 2 * p,
                 ch = h - 2 * p;
    if (!(w > 2 * p && h > 2 * p))
      return std::unexpected("Wardley canvas is smaller than its padding");
    auto px = [&](double x) { return p + x * cw / 100; };
    auto py = [&](double y) { return h - p - y * ch / 100; };
    auto position = [&](const WardleyNode &node) {
      double x = px(node.x), y = py(node.y);
      if (node.pipeline_parent) {
        for (const auto &pipeline : d.pipelines)
          if (pipeline.parent == node.id && !pipeline.components.empty()) {
            double left = std::numeric_limits<double>::infinity(),
                   right = -left;
            for (const auto &id : pipeline.components) {
              auto child = std::ranges::find(d.nodes, id, &WardleyNode::id);
              if (child != d.nodes.end()) {
                left = std::min(left, px(child->x));
                right = std::max(right, px(child->x));
                y = py(child->y) - in.node_radius * 2;
              }
            }
            if (std::isfinite(left))
              x = (left + right) / 2;
            y -= in.node_radius * 1.6 / 6;
            break;
          }
      }
      return std::pair{x, y};
    };
    std::string s = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " +
                    n(w) + " " + n(h) + "\" font-family=\"" +
                    q(in.theme.font_family) + "\"";
    s += in.use_max_width ? " width=\"100%\" style=\"max-width:" + n(w) + "px\""
                          : " width=\"" + n(w) + "\" height=\"" + n(h) + "\"";
    s += "><g class=\"wardley-map\"><rect class=\"wardley-background\" "
         "width=\"100%\" height=\"100%\" fill=\"" +
         in.theme.background + "\"/>";
    auto text =
        [&](V value, double x, double y, double size, std::string cls,
            std::string anchor = "middle", bool bold = false,
            std::string baseline = {}, std::string transform = {},
            std::string color = {}) -> std::expected<void, std::string> {
      auto m = measure(value, size, bold ? 700 : 400);
      if (!m)
        return std::unexpected(m.error());
      s += "<text class=\"" + cls + "\" x=\"" + n(x) + "\" y=\"" + n(y) +
           "\" font-size=\"" + n(size) + "\" text-anchor=\"" + anchor +
           "\" fill=\"" + (color.empty() ? in.theme.text : color) + "\"" +
           (baseline.empty() ? "" : " dominant-baseline=\"" + baseline + "\"") +
           (transform.empty() ? "" : " transform=\"" + transform + "\"") +
           (bold ? " font-weight=\"bold\"" : "") + ">" + esc(value) + "</text>";
      return {};
    };
    if (!d.title.empty()) {
      auto x = text(d.title, w / 2, p / 2, in.axis_font * 1.05, "wardley-title",
                    "middle", true, "middle");
      if (!x)
        return std::unexpected(x.error());
    }
    s += "<defs><marker id=\"link-arrow-end-wardley\" viewBox=\"0 0 10 10\" "
         "refX=\"9\" refY=\"5\" markerWidth=\"5\" markerHeight=\"5\" "
         "orient=\"auto\"><path d=\"M 0 0 L 10 5 L 0 10 z\" fill=\"" +
         in.theme.link +
         "\"/></marker><marker id=\"link-arrow-start-wardley\" viewBox=\"0 0 "
         "10 10\" refX=\"1\" refY=\"5\" markerWidth=\"5\" markerHeight=\"5\" "
         "orient=\"auto\"><path d=\"M 10 0 L 0 5 L 10 10 z\" fill=\"" +
         in.theme.link +
         "\"/></marker><marker id=\"wardley-trend-arrow\" viewBox=\"0 0 10 "
         "10\" refX=\"9\" refY=\"5\" markerWidth=\"6\" markerHeight=\"6\" "
         "orient=\"auto-start-reverse\"><path d=\"M 0 0 L 10 5 L 0 10 z\" "
         "fill=\"" +
         in.theme.evolution + "\"/></marker></defs><g class=\"wardley-axes\">";
    s += line(p, h - p, w - p, h - p, "", in.theme.axis) +
         line(p, p, p, h - p, "", in.theme.axis);
    auto a = text(d.x_label, w / 2, h - p / 4, in.axis_font,
                  "wardley-axis-label wardley-axis-label-x", "middle", true);
    if (!a)
      return std::unexpected(a.error());
    s += "<text class=\"wardley-axis-label wardley-axis-label-y\" x=\"" +
         n(p / 3) + "\" y=\"" + n(p + (ch / 2)) + "\" transform=\"rotate(-90 " +
         n(p / 3) + " " + n(p + ch / 2) + ")\" fill=\"" + in.theme.text +
         "\" font-size=\"" + n(in.axis_font) + "\" text-anchor=\"middle\">" +
         esc(d.y_label) + "</text></g><g class=\"wardley-stages\">";
    const std::vector<std::u16string> defaults = {u"Genesis", u"Custom Built",
                                                  u"Product", u"Commodity"};
    const auto &stages = d.stages.empty() ? defaults : d.stages;
    for (std::size_t i = 0; i < stages.size(); ++i) {
      double start, end;
      if (d.boundaries.size() == stages.size()) {
        start = i ? d.boundaries[i - 1] : 0;
        end = d.boundaries[i];
      } else {
        start = double(i) / stages.size();
        end = double(i + 1) / stages.size();
      }
      if (i)
        s += line(
            p + start * cw, p, p + start * cw, h - p, "", "#000",
            " stroke-width=\"1\" stroke-dasharray=\"5 5\" opacity=\"0.8\"");
      auto st = text(stages[i], p + (start + end) * cw / 2, h - p / 1.5,
                     in.axis_font - 2, "wardley-stage-label");
      if (!st)
        return std::unexpected(st.error());
    }
    s += "</g>";
    if (in.show_grid)
      for (int i = 1; i < 4; ++i) {
        double x = p + cw * i / 4, y = h - p - ch * i / 4;
        s += line(x, p, x, h - p, "wardley-grid", in.theme.grid,
                  " stroke-dasharray=\"2 6\"") +
             line(p, y, w - p, y, "wardley-grid", in.theme.grid,
                  " stroke-dasharray=\"2 6\"");
      }
    for (const auto &pipeline : d.pipelines) {
      std::vector<const WardleyNode *> nodes;
      for (const auto &id : pipeline.components) {
        auto it = std::ranges::find(d.nodes, id, &WardleyNode::id);
        if (it != d.nodes.end())
          nodes.push_back(&*it);
      }
      std::ranges::sort(nodes, {}, &WardleyNode::x);
      if (nodes.empty())
        continue;
      for (std::size_t i = 1; i < nodes.size(); ++i)
        s += line(px(nodes[i - 1]->x), py(nodes[i - 1]->y), px(nodes[i]->x),
                  py(nodes[i]->y), "wardley-pipeline-evolution-link",
                  in.theme.link, " stroke-dasharray=\"4 4\"");
      double left = px(nodes.front()->x), right = px(nodes.back()->x),
             y = py(nodes.front()->y), box_h = in.node_radius * 4;
      s += "<rect class=\"wardley-pipeline-box\" x=\"" + n(left - 15) +
           "\" y=\"" + n(y - box_h / 2) + "\" width=\"" + n(right - left + 30) +
           "\" height=\"" + n(box_h) + "\" fill=\"none\" stroke=\"" +
           in.theme.component_stroke +
           "\" stroke-width=\"1.5\" rx=\"4\" ry=\"4\"/>";
    }
    for (const auto &l : d.links) {
      auto a = std::ranges::find(d.nodes, l.source, &WardleyNode::id),
           b = std::ranges::find(d.nodes, l.target, &WardleyNode::id);
      if (a == d.nodes.end() || b == d.nodes.end())
        continue;
      auto [ax, ay] = position(*a);
      auto [bx, by] = position(*b);
      double x1 = ax, y1 = ay, x2 = bx, y2 = by, dx = x2 - x1, dy = y2 - y1,
             dist = std::hypot(dx, dy);
      if (dist == 0)
        continue;
      double radius = a->pipeline_parent ? in.node_radius * 1.6 / std::sqrt(2)
                                         : in.node_radius;
      double radius2 = b->pipeline_parent ? in.node_radius * 1.6 / std::sqrt(2)
                                          : in.node_radius;
      x1 += dx / dist * radius;
      y1 += dy / dist * radius;
      x2 -= dx / dist * radius2;
      y2 -= dy / dist * radius2;
      std::string extra = l.dashed ? " stroke-dasharray=\"6 6\"" : "";
      if (l.flow == u"forward" || l.flow == u"bidirectional")
        extra += " marker-end=\"url(#link-arrow-end-wardley)\"";
      if (l.flow == u"backward" || l.flow == u"bidirectional")
        extra += " marker-start=\"url(#link-arrow-start-wardley)\"";
      s += line(x1, y1, x2, y2, "wardley-link", in.theme.link, extra);
      if (!l.label.empty()) {
        double mid_x = (x1 + x2) / 2, mid_y = (y1 + y2) / 2, perp_x = dy / dist,
               perp_y = -dx / dist, tx = mid_x + perp_x * 8,
               ty = mid_y + perp_y * 8;
        double angle = std::atan2(dy, dx) * 180 / std::numbers::pi;
        if (angle > 90 || angle < -90)
          angle += 180;
        auto z = text(l.label, tx, ty, in.label_font, "wardley-link-label",
                      "middle", false, "middle",
                      "rotate(" + n(angle) + " " + n(tx) + " " + n(ty) + ")");
        if (!z)
          return std::unexpected(z.error());
      }
    }
    for (const auto &t : d.trends) {
      auto a = std::ranges::find(d.nodes, t.node, &WardleyNode::id);
      if (a == d.nodes.end())
        continue;
      double x1 = px(a->x), y1 = py(a->y), x2 = px(t.target), y2 = py(a->y),
             dx = x2 - x1, dy = y2 - y1, dist = std::hypot(dx, dy);
      if (dist > in.node_radius + 2) {
        x2 -= dx / dist * (in.node_radius + 2);
        y2 -= dy / dist * (in.node_radius + 2);
      }
      s += line(
          x1, y1, x2, y2, "wardley-trend", in.theme.evolution,
          " stroke-dasharray=\"4 4\" marker-end=\"url(#wardley-trend-arrow)\"");
    }
    for (const auto &node : d.nodes) {
      auto [x, y] = position(node);
      if (node.strategy == u"outsource" || node.strategy == u"buy" ||
          node.strategy == u"build")
        s += "<circle class=\"wardley-" + esc(node.strategy) +
             "-overlay\" cx=\"" + n(x) + "\" cy=\"" + n(y) + "\" r=\"" +
             n(in.node_radius * 2) + "\" fill=\"" +
             (node.strategy == u"outsource" ? "#666"
              : node.strategy == u"buy"     ? "#ccc"
                                            : "#eee") +
             "\" stroke=\"" +
             (node.strategy == u"build" ? std::string("#000")
                                        : in.theme.component_stroke) +
             "\" stroke-width=\"1\"/>";
      if (node.strategy == u"market") {
        double tr = in.node_radius * 1.2, sr = in.node_radius * .7,
               dx = tr * std::cos(std::numbers::pi / 6),
               dy = tr * std::sin(std::numbers::pi / 6);
        s += "<circle class=\"wardley-market-overlay\" cx=\"" + n(x) +
             "\" cy=\"" + n(y) + "\" r=\"" + n(in.node_radius * 2) +
             "\" fill=\"white\" stroke=\"" + in.theme.component_stroke +
             "\" stroke-width=\"1\"/>";
        s += line(x, y - tr, x - dx, y + dy, "wardley-market-line",
                  in.theme.component_stroke) +
             line(x - dx, y + dy, x + dx, y + dy, "wardley-market-line",
                  in.theme.component_stroke) +
             line(x + dx, y + dy, x, y - tr, "wardley-market-line",
                  in.theme.component_stroke);
        for (auto [ox, oy] :
             {std::pair{0.0, -tr}, std::pair{-dx, dy}, std::pair{dx, dy}})
          s += "<circle class=\"wardley-market-dot\" cx=\"" + n(x + ox) +
               "\" cy=\"" + n(y + oy) + "\" r=\"" + n(sr) +
               "\" fill=\"white\" stroke=\"" + in.theme.component_stroke +
               "\" stroke-width=\"2\"/>";
      }
      if (node.pipeline_parent)
        s += "<rect class=\"wardley-pipeline-parent\" x=\"" +
             n(x - in.node_radius * .8) + "\" y=\"" +
             n(y - in.node_radius * .8) + "\" width=\"" +
             n(in.node_radius * 1.6) + "\" height=\"" +
             n(in.node_radius * 1.6) + "\" fill=\"" + in.theme.component_fill +
             "\" stroke=\"" + in.theme.component_stroke + "\"/>";
      else if (node.class_name != u"anchor" && node.strategy != u"market")
        s += "<circle class=\"wardley-node\" cx=\"" + n(x) + "\" cy=\"" + n(y) +
             "\" r=\"" + n(in.node_radius) + "\" fill=\"" +
             in.theme.component_fill + "\" stroke=\"" +
             in.theme.component_stroke + "\" stroke-width=\"1\"/>";
      if (node.inertia)
        s += "<line class=\"wardley-inertia\" x1=\"" +
             n(x + in.node_radius + 15 +
               (node.strategy.empty() ? 0 : in.node_radius + 10)) +
             "\" y1=\"" +
             n(y - (node.pipeline_parent ? in.node_radius * 1.6 / 2
                                         : in.node_radius)) +
             "\" x2=\"" +
             n(x + in.node_radius + 15 +
               (node.strategy.empty() ? 0 : in.node_radius + 10)) +
             "\" y2=\"" +
             n(y + (node.pipeline_parent ? in.node_radius * 1.6 / 2
                                         : in.node_radius)) +
             "\" stroke=\"" + in.theme.component_stroke +
             "\" stroke-width=\"6\"/>";
      double lx =
          node.class_name == u"anchor"
              ? (node.has_label_offset ? x + node.label_x : x)
              : (x +
                 (node.has_label_offset
                      ? node.label_x
                      : (in.label_offset + (node.strategy.empty() ? 0 : 10))));
      double ly =
          node.class_name == u"anchor"
              ? (node.has_label_offset ? y + node.label_y : y - 3)
              : (y +
                 (node.has_label_offset
                      ? node.label_y
                      : (-in.label_offset - (node.strategy.empty() ? 0 : 10))));
      auto z = text(node.label, lx, ly, in.label_font, "wardley-node-label",
                    node.class_name == u"anchor" ? "middle" : "start",
                    node.class_name == u"anchor",
                    node.class_name == u"anchor" ? "middle" : "", {},
                    node.class_name == u"anchor" ? "#000" : "");
      if (!z)
        return std::unexpected(z.error());
    }
    for (const auto &note : d.notes) {
      auto z = text(note.text, px(note.x), py(note.y), 11, "wardley-note",
                    "start", true);
      if (!z)
        return std::unexpected(z.error());
    }
    for (const auto &an : d.annotations) {
      double x = px(an.x), y = py(an.y);
      s += "<circle class=\"wardley-annotation\" cx=\"" + n(x) + "\" cy=\"" +
           n(y) + "\" r=\"10\" fill=\"white" + "\" stroke=\"" +
           in.theme.annotation_stroke + "\" stroke-width=\"1.5\"/>";
      auto digits = std::to_string(an.number);
      auto num_text = std::u16string(digits.begin(), digits.end());
      auto m = measure(num_text, 10, 700);
      if (!m)
        return std::unexpected(m.error());
      s += "<text class=\"wardley-annotation-number\" x=\"" + n(x) + "\" y=\"" +
           n(y) +
           "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
           "font-size=\"10\" font-weight=\"bold\" fill=\"" +
           in.theme.annotation_text + "\">" + esc(num_text) + "</text>";
    }
    if (d.annotations_box) {
      std::vector<const WardleyAnnotation *> sorted;
      for (const auto &item : d.annotations)
        if (!item.text.empty())
          sorted.push_back(&item);
      std::ranges::sort(sorted, {}, &WardleyAnnotation::number);
      if (!sorted.empty()) {
        double x = px(d.annotations_box->first),
               y = py(d.annotations_box->second), max_width = 0, max_height = 0;
        for (auto item : sorted) {
          auto label = std::to_string(item->number) + ". ";
          std::u16string text_value(label.begin(), label.end());
          text_value += item->text;
          auto m = measure(text_value, 11, 400);
          if (!m)
            return std::unexpected(m.error());
          max_width = std::max(max_width, m->width);
          max_height = std::max(max_height, m->height);
        }
        double box_w = max_width + 20 + 105,
               box_h = static_cast<double>(sorted.size()) * 16 + 20 +
                       max_height / 2;
        x = std::max(p, std::min(x, w - p - box_w));
        y = std::max(p, std::min(y, h - p - box_h));
        s += "<rect class=\"wardley-annotations-box\" x=\"" + n(x) + "\" y=\"" +
             n(y) + "\" width=\"" + n(box_w) + "\" height=\"" + n(box_h) +
             "\" rx=\"4\" ry=\"4\" fill=\"white" + "\" stroke=\"" +
             in.theme.annotation_stroke + "\" stroke-width=\"1.5\"/>";
        for (std::size_t i = 0; i < sorted.size(); ++i) {
          auto prefix = std::to_string(sorted[i]->number) + ". ";
          std::u16string label(prefix.begin(), prefix.end());
          label += sorted[i]->text;
          double yy = y + 10 + (i + 1) * 16;
          auto m = measure(label, 11, 400);
          if (!m)
            return std::unexpected(m.error());
          s += "<text class=\"wardley-annotation-box-label\" x=\"" + n(x + 10) +
               "\" y=\"" + n(yy) +
               "\" text-anchor=\"start\" dominant-baseline=\"middle\" "
               "font-size=\"11\" fill=\"" +
               in.theme.annotation_text + "\">" + esc(label) + "</text>";
        }
      }
    }
    auto draw_marker = [&](const WardleyMarker &m, bool right) {
      double x = px(m.x), y = py(m.y);
      std::string path;
      if (right) {
        path = "M " + n(x) + " " + n(y - 15) + " L " + n(x + 40) + " " +
               n(y - 15) + " L " + n(x + 40) + " " + n(y - 23) + " L " +
               n(x + 60) + " " + n(y) + " L " + n(x + 40) + " " + n(y + 23) +
               " L " + n(x + 40) + " " + n(y + 15) + " L " + n(x) + " " +
               n(y + 15) + " Z";
      } else {
        path = "M " + n(x + 60) + " " + n(y - 15) + " L " + n(x + 20) + " " +
               n(y - 15) + " L " + n(x + 20) + " " + n(y - 23) + " L " + n(x) +
               " " + n(y) + " L " + n(x + 20) + " " + n(y + 23) + " L " +
               n(x + 20) + " " + n(y + 15) + " L " + n(x + 60) + " " +
               n(y + 15) + " Z";
      }
      s += "<path class=\"wardley-accelerator\" d=\"" + path +
           "\" fill=\"white\" stroke=\"" + in.theme.component_stroke +
           "\" stroke-width=\"1\"/>";
      return text(m.name, x + 30, y + 30, 10, "wardley-accelerator-label",
                  "middle", true);
    };
    for (const auto &m : d.accelerators) {
      auto z = draw_marker(m, true);
      if (!z)
        return std::unexpected(z.error());
    }
    for (const auto &m : d.deaccelerators) {
      auto z = draw_marker(m, false);
      if (!z)
        return std::unexpected(z.error());
    }
    s += "</g></svg>";
    return WardleySvg{std::move(s), w, h};
  } catch (const std::exception &e) {
    return std::unexpected(e.what());
  }
}
} // namespace folia::mermaid
