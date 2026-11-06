module folia.mermaid.agentflow;
import std;
namespace folia::mermaid {
namespace {
std::u16string trim(std::u16string_view s) {
    auto a = s.find_first_not_of(u" \t\r");
    if (a == s.npos)
        return {};
    auto b = s.find_last_not_of(u" \t\r");
    return std::u16string(s.substr(a, b - a + 1));
}
struct Vertex {
    std::u16string id, label, shape;
};
Vertex vertex(std::u16string_view v) {
    auto meta = v.find(u"@{");
    auto core = trim(meta == v.npos ? v : v.substr(0, meta));
    std::u16string shape;
    if (meta != v.npos) {
        auto s = v.find(u"shape", meta), colon = s == v.npos ? s : v.find(u':', s),
             close = v.find(u'}', colon);
        if (s != v.npos && colon != v.npos && close != v.npos) {
            shape = trim(v.substr(colon + 1, close - colon - 1));
            if (shape.starts_with(u'"') && shape.ends_with(u'"'))
                shape = shape.substr(1, shape.size() - 2);
        }
    }
    auto open = core.find_first_of(u"[(");
    if (open == core.npos)
        return {core, {}, std::move(shape)};
    auto close = core.find_last_of(u")]");
    auto label = close > open ? core.substr(open + 1, close - open - 1) : std::u16string{};
    if (label.size() > 1 && label.front() == u'"' && label.back() == u'"')
        label = label.substr(1, label.size() - 2);
    return {trim(std::u16string_view(core).substr(0, open)), std::move(label), std::move(shape)};
}
} // namespace
std::expected<AgentflowDocument, std::string> parse_agentflow(std::u16string_view source) {
    try {
        AgentflowDocument d;
        std::unordered_map<std::u16string, std::size_t> indices;
        bool header = false;
        std::size_t p = 0;
        std::vector<std::u16string> container_stack;
        auto add = [&](Vertex v, bool global = false) -> std::u16string {
            if (v.id.empty())
                throw std::runtime_error("Agentflow node id is empty");
            auto [i, created] = indices.emplace(v.id, d.nodes.size());
            if (created)
                d.nodes.push_back(
                    {v.id, v.label.empty() ? v.id : std::move(v.label),
                     v.shape.empty() ? u"task" : std::move(v.shape),
                     global || container_stack.empty() ? std::u16string{} : container_stack.back()});
            else {
                auto &n = d.nodes[i->second];
                if (!v.label.empty())
                    n.label = std::move(v.label);
                if (!v.shape.empty())
                    n.shape = std::move(v.shape);
            }
            return v.id;
        };
        while (p <= source.size()) {
            auto end = source.find(u'\n', p);
            if (end == source.npos)
                end = source.size();
            auto line = trim(source.substr(p, end - p));
            p = end + 1;
            if (line.empty() || line.starts_with(u"%%")) {
                if (end == source.size())
                    break;
                continue;
            }
            if (!header) {
                if (!line.starts_with(u"agentflow-beta"))
                    return std::unexpected("Expected agentflow-beta diagram header");
                header = true;
                auto q = line.find(u' ');
                if (q != line.npos) {
                    d.direction = trim(std::u16string_view(line).substr(q + 1));
                    if (d.direction == u"TD")
                        d.direction = u"TB";
                    if (d.direction != u"TB" && d.direction != u"BT" && d.direction != u"LR" &&
                        d.direction != u"RL")
                        return std::unexpected("Invalid Agentflow direction");
                }
                if (end == source.size())
                    break;
                continue;
            }
            if (line == u"end") {
                if (container_stack.empty())
                    return std::unexpected("Unexpected Agentflow end");
                container_stack.pop_back();
                if (end == source.size())
                    break;
                continue;
            }
            if (line.starts_with(u"flow ") || line.starts_with(u"subgraph ")) {
                auto body = trim(std::u16string_view(line).substr(line.find(u' ') + 1));
                auto group = vertex(body);
                if (group.id.empty())
                    return std::unexpected("Agentflow flow id is empty");
                d.containers.push_back({group.id, group.label.empty() ? group.id : group.label,
                                        container_stack.empty() ? std::u16string{} : container_stack.back()});
                container_stack.push_back(group.id);
                if (end == source.size())
                    break;
                continue;
            }
            if (line.starts_with(u"global ")) {
                add(vertex(trim(std::u16string_view(line).substr(7))), true);
                if (end == source.size())
                    break;
                continue;
            }
            if (line.starts_with(u"connector ")) {
                auto v = vertex(trim(std::u16string_view(line).substr(10)));
                v.shape = u"connector";
                add(std::move(v), true);
                if (end == source.size())
                    break;
                continue;
            }
            const auto find_edge = [](std::u16string_view text, std::size_t from) {
                const auto solid = text.find(u"--", from), dotted = text.find(u"-.-", from);
                if (solid == text.npos)
                    return dotted;
                if (dotted == text.npos)
                    return solid;
                return std::min(solid, dotted);
            };
            const auto first = find_edge(line, 0);
            if (first == line.npos) {
                add(vertex(line));
            } else {
                auto a = add(vertex(std::u16string_view(line).substr(0, first)));
                std::size_t pos = first;
                while (pos < line.size()) {
                    const bool dashed = line.substr(pos).starts_with(u"-.-");
                    std::u16string label;
                    std::size_t after = pos;
                    if (dashed)
                        after += 3;
                    else if (line.substr(pos).starts_with(u"-->"))
                        after += 3;
                    else {
                        const auto close = line.find(u"-->", pos + 2);
                        if (close == line.npos)
                            return std::unexpected("Unclosed Agentflow edge label");
                        label = trim(std::u16string_view(line).substr(pos + 2, close - pos - 2));
                        after = close + 3;
                    }
                    const auto next = find_edge(line, after);
                    auto right = trim(std::u16string_view(line).substr(
                        after, next == line.npos ? line.size() - after : next - after));
                    const auto ls = right.find(u'|'),
                               le = ls == right.npos ? right.npos : right.find(u'|', ls + 1);
                    if (le != right.npos) {
                        label = right.substr(ls + 1, le - ls - 1);
                        right = trim(std::u16string_view(right).substr(le + 1));
                    }
                    auto b = add(vertex(right));
                    d.edges.push_back({a, b, std::move(label), dashed});
                    if (next == line.npos)
                        break;
                    pos = next;
                    a = std::move(b);
                }
            }
            if (end == source.size())
                break;
        }
        if (!header)
            return std::unexpected("Expected agentflow-beta diagram header");
        if (!container_stack.empty())
            return std::unexpected("Unclosed Agentflow flow container");
        if (d.nodes.empty())
            return std::unexpected("Agentflow diagram has no nodes");
        return d;
    } catch (const std::exception &e) {
        return std::unexpected(e.what());
    }
}
} // namespace folia::mermaid
