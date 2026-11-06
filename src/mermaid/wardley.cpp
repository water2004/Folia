module folia.mermaid.wardley;
import std;

namespace folia::mermaid {
namespace {

using View = std::u16string_view;

View trim(View value) {
  while (!value.empty() && (value.front() == u' ' || value.front() == u'\t' ||
                            value.front() == u'\r')) {
    value.remove_prefix(1);
  }
  while (!value.empty() && (value.back() == u' ' || value.back() == u'\t' ||
                            value.back() == u'\r')) {
    value.remove_suffix(1);
  }
  return value;
}

bool starts_with(View value, View prefix) { return value.starts_with(prefix); }

std::u16string parse_name(View &rest) {
  rest = trim(rest);
  if (rest.empty())
    return {};

  if (rest.front() == u'"') {
    const auto end = rest.find(u'"', 1);
    if (end == View::npos)
      throw std::runtime_error("unterminated quoted name");
    std::u16string value(rest.substr(1, end - 1));
    rest.remove_prefix(end + 1);
    return value;
  }

  std::size_t length = 0;
  while (length < rest.size()) {
    const auto current = rest[length];
    const bool arrow_start =
        current == u'-' && length + 1 < rest.size() &&
        (rest[length + 1] == u'>' || rest[length + 1] == u'-' ||
         rest[length + 1] == u'.');
    if (current == u'[' || current == u';' || current == u'+' ||
        current == u'>' || current == u'{' || current == u'}' || arrow_start) {
      break;
    }
    ++length;
  }

  const auto result = trim(rest.substr(0, length));
  rest.remove_prefix(length);
  return std::u16string(result);
}

double parse_number(View value) {
  std::string ascii;
  ascii.reserve(value.size());
  for (const auto ch : value)
    ascii.push_back(static_cast<char>(ch));

  double result{};
  const auto conversion =
      std::from_chars(ascii.data(), ascii.data() + ascii.size(), result);
  if (conversion.ec != std::errc{} ||
      conversion.ptr != ascii.data() + ascii.size() || !std::isfinite(result)) {
    throw std::runtime_error("invalid number");
  }
  return result;
}

double parse_decimal(View value) {
  if (value.find(u'.') == View::npos) {
    throw std::runtime_error("expected decimal coordinate");
  }
  return parse_number(value);
}

double to_percent(double value) {
  if (value < 0 || value > 100) {
    throw std::runtime_error("coordinate must be in 0-1 or 0-100 range");
  }
  return value <= 1 ? value * 100 : value;
}

std::pair<double, double> parse_pair(View value, bool decimal_values = true) {
  const auto comma = value.find(u',');
  if (comma == View::npos) {
    throw std::runtime_error("expected coordinate pair");
  }

  const auto parse_value = decimal_values ? parse_decimal : parse_number;
  return {
      to_percent(parse_value(trim(value.substr(comma + 1)))),
      to_percent(parse_value(trim(value.substr(0, comma)))),
  };
}

std::pair<double, double> parse_offsets(View value) {
  const auto comma = value.find(u',');
  if (comma == View::npos) {
    throw std::runtime_error("expected label offset pair");
  }
  return {
      parse_number(trim(value.substr(0, comma))),
      parse_number(trim(value.substr(comma + 1))),
  };
}

std::pair<double, double> parse_coordinates(View &rest) {
  const auto open = rest.find(u'[');
  const auto close = rest.find(u']');
  if (open == View::npos || close == View::npos || close < open) {
    throw std::runtime_error("expected [visibility,evolution]");
  }
  const auto result = parse_pair(rest.substr(open + 1, close - open - 1));
  rest.remove_prefix(close + 1);
  return result;
}

} // namespace

std::expected<WardleyDiagram, std::string> parse_wardley(View source) {
  WardleyDiagram diagram;
  diagram.stages = {u"Genesis", u"Custom Built", u"Product", u"Commodity"};
  bool header_seen = false;
  std::u16string pipeline_parent;
  std::size_t line_number = 0;

  try {
    while (!source.empty()) {
      const auto newline = source.find(u'\n');
      const auto raw_line = source.substr(0, newline);
      source = newline == View::npos ? View{} : source.substr(newline + 1);
      ++line_number;

      auto rest = trim(raw_line);
      if (rest.empty() || rest.starts_with(u"%%"))
        continue;

      if (!header_seen) {
        if (!starts_with(rest, u"wardley-beta")) {
          throw std::runtime_error("expected wardley-beta header");
        }
        header_seen = true;
        rest = trim(rest.substr(12));
        if (rest.empty())
          continue;
      }

      if (!pipeline_parent.empty()) {
        if (rest == u"}") {
          pipeline_parent.clear();
          continue;
        }
        if (!starts_with(rest, u"component ")) {
          throw std::runtime_error(
              "expected pipeline component or closing brace");
        }

        rest = trim(rest.substr(10));
        const auto label = parse_name(rest);
        const auto open = rest.find(u'[');
        const auto close = rest.find(u']');
        if (open == View::npos || close == View::npos) {
          throw std::runtime_error("expected pipeline evolution coordinate");
        }
        const auto evolution = to_percent(
            parse_decimal(trim(rest.substr(open + 1, close - open - 1))));
        const auto parent =
            std::ranges::find(diagram.nodes, pipeline_parent, &WardleyNode::id);
        if (parent == diagram.nodes.end()) {
          throw std::runtime_error("pipeline parent must be an existing node");
        }

        WardleyNode node;
        node.id = pipeline_parent + u"_" + label;
        node.label = label;
        node.class_name = u"pipeline-component";
        node.x = evolution;
        node.y = parent->y;
        node.in_pipeline = true;
        diagram.nodes.push_back(node);
        diagram.pipelines.back().components.push_back(node.id);
        continue;
      }

      if (starts_with(rest, u"title ")) {
        diagram.title = std::u16string(trim(rest.substr(6)));
        continue;
      }

      if (starts_with(rest, u"size ")) {
        const auto open = rest.find(u'[');
        const auto close = rest.find(u']');
        if (open == View::npos || close == View::npos) {
          throw std::runtime_error("invalid size");
        }
        const auto comma = rest.find(u',', open);
        if (comma == View::npos || comma > close) {
          throw std::runtime_error("invalid size");
        }
        diagram.width =
            parse_number(trim(rest.substr(open + 1, comma - open - 1)));
        diagram.height =
            parse_number(trim(rest.substr(comma + 1, close - comma - 1)));
        if (diagram.width <= 0 || diagram.height <= 0) {
          throw std::runtime_error("size must be positive");
        }
        diagram.explicit_size = true;
        continue;
      }

      if (starts_with(rest, u"evolution ")) {
        rest = trim(rest.substr(10));
        diagram.stages.clear();
        diagram.boundaries.clear();
        while (!rest.empty()) {
          const auto arrow = rest.find(u"->");
          auto stage = trim(rest.substr(0, arrow));
          const auto boundary_at = stage.find(u'@');
          if (boundary_at != View::npos) {
            const auto boundary =
                parse_decimal(trim(stage.substr(boundary_at + 1)));
            if (boundary < 0 || boundary > 1) {
              throw std::runtime_error(
                  "stage boundary must be between 0 and 1");
            }
            diagram.boundaries.push_back(boundary);
            stage = trim(stage.substr(0, boundary_at));
          }
          const auto slash = stage.find(u'/');
          if (slash != View::npos) {
            const auto first = trim(stage.substr(0, slash));
            const auto second = trim(stage.substr(slash + 1));
            stage = std::u16string(first) + u" / " + std::u16string(second);
          }
          diagram.stages.emplace_back(stage);
          if (arrow == View::npos)
            break;
          rest = trim(rest.substr(arrow + 2));
        }
        continue;
      }

      if (starts_with(rest, u"anchor ") || starts_with(rest, u"component ")) {
        const bool anchor = starts_with(rest, u"anchor ");
        rest = trim(rest.substr(anchor ? 7 : 10));
        auto id = parse_name(rest);
        const auto [x, y] = parse_coordinates(rest);

        WardleyNode node;
        node.id = id;
        node.label = std::move(id);
        node.x = x;
        node.y = y;
        node.class_name = anchor ? u"anchor" : u"component";

        if (!anchor) {
          auto remainder = trim(rest);
          if (starts_with(remainder, u"label")) {
            const auto open = remainder.find(u'[');
            const auto close = remainder.find(u']');
            if (open != View::npos && close != View::npos) {
              const auto [offset_x, offset_y] =
                  parse_offsets(remainder.substr(open + 1, close - open - 1));
              node.label_x = offset_x;
              node.label_y = offset_y;
              node.has_label_offset = true;
              rest = remainder.substr(close + 1);
            }
          }
          if (rest.find(u"inertia") != View::npos)
            node.inertia = true;
          for (const auto strategy :
               {u"build", u"buy", u"outsource", u"market"}) {
            if (rest.find(strategy) != View::npos) {
              node.strategy = strategy;
            }
          }
        }
        diagram.nodes.push_back(std::move(node));
        continue;
      }

      if (starts_with(rest, u"pipeline ")) {
        rest = trim(rest.substr(9));
        auto parent_id = parse_name(rest);
        const auto parent =
            std::ranges::find(diagram.nodes, parent_id, &WardleyNode::id);
        if (parent == diagram.nodes.end()) {
          throw std::runtime_error(
              "pipeline parent must reference an existing component");
        }
        parent->pipeline_parent = true;
        pipeline_parent = parent_id;
        diagram.pipelines.push_back({parent_id, {}});
        continue;
      }

      if (starts_with(rest, u"note ")) {
        rest = trim(rest.substr(5));
        WardleyNote note;
        note.text = parse_name(rest);
        const auto [x, y] = parse_coordinates(rest);
        note.x = x;
        note.y = y;
        diagram.notes.push_back(std::move(note));
        continue;
      }

      if (starts_with(rest, u"evolve ")) {
        rest = trim(rest.substr(7));
        const auto target_at = rest.find_last_of(u" \t");
        if (target_at == View::npos) {
          throw std::runtime_error("evolve requires a component and target");
        }
        auto id_rest = trim(rest.substr(0, target_at));
        const auto id = parse_name(id_rest);
        if (!trim(id_rest).empty()) {
          throw std::runtime_error("invalid evolve component");
        }
        const auto target =
            to_percent(parse_decimal(trim(rest.substr(target_at + 1))));
        const auto node =
            std::ranges::find(diagram.nodes, id, &WardleyNode::id);
        if (node == diagram.nodes.end()) {
          throw std::runtime_error("evolve refers to an unknown component");
        }
        diagram.trends.push_back({id, target});
        continue;
      }

      if (starts_with(rest, u"annotations ")) {
        rest = trim(rest.substr(12));
        const auto open = rest.find(u'[');
        const auto close = rest.find(u']');
        if (open == View::npos || close == View::npos) {
          throw std::runtime_error("invalid annotations box");
        }
        const auto [x, y] =
            parse_pair(rest.substr(open + 1, close - open - 1), false);
        diagram.annotations_box = std::pair{x, y};
        continue;
      }

      if (starts_with(rest, u"annotation ")) {
        rest = trim(rest.substr(11));
        const auto comma = rest.find(u',');
        if (comma == View::npos) {
          throw std::runtime_error("invalid annotation");
        }
        const auto number =
            static_cast<int>(parse_number(trim(rest.substr(0, comma))));
        rest.remove_prefix(comma + 1);
        const auto open = rest.find(u'[');
        const auto close = rest.find(u']');
        if (open == View::npos || close == View::npos) {
          throw std::runtime_error("invalid annotation coordinate");
        }
        const auto [x, y] =
            parse_pair(rest.substr(open + 1, close - open - 1), false);
        rest.remove_prefix(close + 1);
        auto text = parse_name(rest);
        diagram.annotations.push_back({number, x, y, std::move(text)});
        continue;
      }

      if (starts_with(rest, u"accelerator ") ||
          starts_with(rest, u"deaccelerator ")) {
        const bool deaccelerator = starts_with(rest, u"deaccelerator ");
        rest = trim(rest.substr(deaccelerator ? 14 : 12));
        auto label = parse_name(rest);
        const auto [x, y] = parse_coordinates(rest);
        auto &markers =
            deaccelerator ? diagram.deaccelerators : diagram.accelerators;
        markers.push_back({std::move(label), x, y});
        continue;
      }

      auto source_id = parse_name(rest);
      const auto arrow_at = rest.find_first_of(u"-+>");
      if (arrow_at == View::npos) {
        throw std::runtime_error("unknown Wardley statement");
      }
      rest.remove_prefix(arrow_at);

      WardleyLink link;
      link.source = std::move(source_id);
      if (starts_with(rest, u"-.->")) {
        link.dashed = true;
        rest.remove_prefix(4);
      } else if (starts_with(rest, u"-->")) {
        rest.remove_prefix(3);
      } else if (starts_with(rest, u"->")) {
        rest.remove_prefix(2);
      } else if (starts_with(rest, u"+<>")) {
        link.flow = u"bidirectional";
        rest.remove_prefix(3);
      } else if (starts_with(rest, u"+<")) {
        link.flow = u"backward";
        rest.remove_prefix(2);
      } else if (starts_with(rest, u"+>")) {
        link.flow = u"forward";
        rest.remove_prefix(2);
      } else if (starts_with(rest, u">")) {
        rest.remove_prefix(1);
      } else if (starts_with(rest, u"+'")) {
        const auto label_end = rest.find(u'\'', 2);
        if (label_end == View::npos) {
          throw std::runtime_error("unterminated flow label");
        }
        link.label = std::u16string(rest.substr(2, label_end - 2));
        rest.remove_prefix(label_end + 1);
        if (starts_with(rest, u"<>")) {
          link.flow = u"bidirectional";
          rest.remove_prefix(2);
        } else if (starts_with(rest, u"<")) {
          link.flow = u"backward";
          rest.remove_prefix(1);
        } else if (starts_with(rest, u">")) {
          link.flow = u"forward";
          rest.remove_prefix(1);
        }
      } else {
        throw std::runtime_error("invalid link arrow");
      }

      if (!link.flow.empty()) {
        if (starts_with(rest, u"-.->")) {
          link.dashed = true;
          rest.remove_prefix(4);
        } else if (starts_with(rest, u"-->")) {
          rest.remove_prefix(3);
        } else if (starts_with(rest, u"->")) {
          rest.remove_prefix(2);
        } else if (starts_with(rest, u">")) {
          rest.remove_prefix(1);
        }
      }
      link.target = parse_name(rest);
      const auto semicolon = rest.find(u';');
      if (semicolon != View::npos) {
        link.label = std::u16string(trim(rest.substr(semicolon + 1)));
      }
      diagram.links.push_back(std::move(link));
    }

    if (!header_seen) {
      throw std::runtime_error("expected wardley-beta header");
    }
    if (!pipeline_parent.empty()) {
      throw std::runtime_error("unterminated pipeline block");
    }

    for (auto &link : diagram.links) {
      auto resolve = [&](std::u16string &id) {
        const auto exact =
            std::ranges::find(diagram.nodes, id, &WardleyNode::id);
        if (exact != diagram.nodes.end())
          return;
        const auto label =
            std::ranges::find(diagram.nodes, id, &WardleyNode::label);
        if (label != diagram.nodes.end())
          id = label->id;
      };
      resolve(link.source);
      resolve(link.target);
    }
    return diagram;
  } catch (const std::exception &error) {
    return std::unexpected("Wardley line " + std::to_string(line_number) +
                           ": " + error.what());
  }
}

} // namespace folia::mermaid
