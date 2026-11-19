// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_components;
import std;

namespace folia::mermaid {
std::expected<std::vector<ElkWeakComponent>, std::string>
elk_split_components(std::size_t node_count, std::span<const ElkFlatEdge> edges) {
    std::vector<std::vector<std::size_t>> adjacent(node_count);
    for (const auto &edge : edges) {
        if (edge.source >= node_count || edge.target >= node_count)
            return std::unexpected("ELK component edge references a missing node");
        // Each imported edge has its own port. The port list is in edge
        // insertion order, and DFS visits both incoming and outgoing ports.
        adjacent[edge.source].push_back(edge.target);
        adjacent[edge.target].push_back(edge.source);
    }
    const auto absent = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> owner(node_count, absent);
    std::vector<ElkWeakComponent> components;
    struct Visit {
        std::size_t node, next = 0;
    };
    std::vector<Visit> stack;
    for (std::size_t start = 0; start < node_count; ++start) {
        if (owner[start] != absent)
            continue;
        const auto index = components.size();
        auto &component = components.emplace_back();
        owner[start] = index;
        component.nodes.push_back(start);
        stack.push_back({start});
        // Iterative recursive-order DFS avoids a call-stack limit on chains.
        while (!stack.empty()) {
            auto &visit = stack.back();
            if (visit.next == adjacent[visit.node].size()) {
                stack.pop_back();
                continue;
            }
            const auto next = adjacent[visit.node][visit.next++];
            if (owner[next] != absent)
                continue;
            owner[next] = index;
            component.nodes.push_back(next);
            stack.push_back({next});
        }
    }
    for (std::size_t edge = 0; edge < edges.size(); ++edge)
        components[owner[edges[edge].source]].edges.push_back(edge);
    return components;
}

std::expected<ElkComponentPacking, std::string> elk_pack_components(std::span<const ElkComponentSize> sizes,
                                                                    double spacing, double aspect_ratio) {
    if (!std::isfinite(spacing) || spacing < 0 || !std::isfinite(aspect_ratio) || aspect_ratio <= 0)
        return std::unexpected("Invalid ELK component packing options");
    ElkComponentPacking result;
    result.offsets.resize(sizes.size());
    std::vector<double> areas;
    double maximum_width = 0, total_area = 0;
    for (std::size_t index = 0; index < sizes.size(); ++index) {
        const auto &size = sizes[index];
        if (!std::isfinite(size.width) || !std::isfinite(size.height) || size.width < 0 || size.height < 0)
            return std::unexpected("Invalid ELK component dimensions");
        result.order.push_back(index);
        areas.push_back(size.width * size.height);
        maximum_width = std::max(maximum_width, size.width);
    }
    if (sizes.empty())
        return result;
    if (sizes.size() == 1) {
        result.width = sizes.front().width;
        result.height = sizes.front().height;
        return result;
    }
    std::ranges::stable_sort(result.order, std::ranges::less{}, [&](auto index) { return areas[index]; });
    // SimpleRowGraphPlacer sums after sorting; retain its floating-point order
    // because the threshold can decide whether the next component wraps.
    for (const auto index : result.order)
        total_area += areas[index];
    if (!std::isfinite(total_area))
        return std::unexpected("ELK component area overflow");
    maximum_width = std::max(maximum_width, std::sqrt(total_area) * aspect_ratio);
    if (!std::isfinite(maximum_width))
        return std::unexpected("ELK component row width overflow");
    double x = 0, y = 0, row_height = 0;
    result.width = spacing;
    for (const auto index : result.order) {
        const auto &size = sizes[index];
        if (x + size.width > maximum_width) {
            x = 0;
            y += row_height + spacing;
            row_height = 0;
        }
        result.offsets[index] = {x, y};
        result.width = std::max(result.width, x + size.width);
        row_height = std::max(row_height, size.height);
        x += size.width + spacing;
    }
    result.height = y + row_height;
    if (!std::isfinite(result.width) || !std::isfinite(result.height))
        return std::unexpected("ELK component packing overflow");
    return result;
}

std::expected<ElkCompoundLayoutResult, std::string>
elk_layout_components(const ElkCompoundLayoutInput &input, const ElkFlatLayoutOptions &options) {
    if (input.nodes.size() != input.hierarchy.nodes.size())
        return std::unexpected("ELK component layout requires measured original nodes");
    if (!input.node_model_order.empty() && input.node_model_order.size() != input.nodes.size())
        return std::unexpected("ELK component model order does not match original nodes");
    if (std::ranges::any_of(input.hierarchy.nodes,
                            [](const auto &node) { return node.group || node.parent; }))
        return elk_layout_compound(input, options);
    // GraphConfigurator selects processors before ComponentsProcessor splits
    // the scope. Freeze that decision so a small component cannot reactivate
    // the switch when the complete scope reached the configured threshold.
    auto component_options = options;
    component_options.two_sided_greedy_switch = elk_uses_greedy_switch(options, input.nodes.size());
    component_options.greedy_switch_activation_threshold = 0;
    auto components = elk_split_components(input.nodes.size(), input.hierarchy.edges);
    if (!components)
        return std::unexpected(components.error());
    if (components->size() <= 1)
        return elk_layout_compound(input, component_options);
    if (!std::isfinite(options.padding) || options.padding < 0)
        return std::unexpected("Invalid ELK component graph padding");

    std::vector<ElkCompoundLayoutInput> graphs(components->size());
    std::vector<std::size_t> local_node(input.nodes.size()), local_edge(input.hierarchy.edges.size()),
        edge_owner(input.hierarchy.edges.size());
    for (std::size_t index = 0; index < components->size(); ++index) {
        const auto &component = (*components)[index];
        auto &graph = graphs[index];
        graph.hierarchy.direction = input.hierarchy.direction;
        graph.hierarchy.edge_spacing = input.hierarchy.edge_spacing;
        for (const auto node : component.nodes) {
            local_node[node] = graph.nodes.size();
            graph.nodes.push_back(input.nodes[node]);
            graph.hierarchy.nodes.push_back(input.hierarchy.nodes[node]);
            // ComponentsProcessor moves existing LNodes in DFS order without
            // changing the MODEL_ORDER assigned before the split.
            graph.node_model_order.push_back(input.node_model_order.empty() ? node
                                                                            : input.node_model_order[node]);
        }
        for (const auto edge : component.edges) {
            local_edge[edge] = graph.hierarchy.edges.size();
            edge_owner[edge] = index;
            const auto &ends = input.hierarchy.edges[edge];
            graph.hierarchy.edges.push_back({local_node[ends.source], local_node[ends.target]});
        }
    }
    for (auto label : input.center_labels) {
        if (label.original_edge >= local_edge.size())
            return std::unexpected("ELK component center label references a missing edge");
        auto &graph = graphs[edge_owner[label.original_edge]];
        label.original_edge = local_edge[label.original_edge];
        graph.center_labels.push_back(label);
    }
    for (auto label : input.end_labels) {
        if (label.original_edge >= local_edge.size())
            return std::unexpected("ELK component end label references a missing edge");
        auto &graph = graphs[edge_owner[label.original_edge]];
        label.original_edge = local_edge[label.original_edge];
        graph.end_labels.push_back(label);
    }

    component_options.padding = 0;
    ElkRandom random(static_cast<std::uint64_t>(options.seed));
    std::vector<ElkCompoundLayoutResult> layouts;
    std::vector<ElkComponentSize> sizes;
    for (const auto &graph : graphs) {
        // GraphConfigurator creates RANDOM before ComponentsProcessor copies
        // the properties; all components consume the same random object.
        auto placed = elk_layout_compound(graph, component_options, random);
        if (!placed)
            return std::unexpected(placed.error());
        sizes.push_back({placed->width, placed->height});
        layouts.push_back(std::move(*placed));
    }
    auto packed = elk_pack_components(sizes);
    if (!packed)
        return std::unexpected(packed.error());
    ElkCompoundLayoutResult result;
    result.nodes.resize(input.nodes.size());
    result.group_titles.resize(input.nodes.size());
    result.width = packed->width + 2 * options.padding;
    result.height = packed->height + 2 * options.padding;
    for (const auto index : packed->order) {
        const auto &component = (*components)[index];
        auto &layout = layouts[index];
        const auto dx = packed->offsets[index].x + options.padding;
        const auto dy = packed->offsets[index].y + options.padding;
        const auto shift = [&](ElkRoutingPoint &point) {
            point.x += dx;
            point.y += dy;
        };
        for (std::size_t n = 0; n < component.nodes.size(); ++n) {
            auto node = layout.nodes[n];
            node.x += dx;
            node.y += dy;
            result.nodes[component.nodes[n]] = node;
        }
        for (auto &route : layout.routes) {
            route.original_edge = component.edges[route.original_edge];
            const auto &edge = input.hierarchy.edges[route.original_edge];
            route.source_node = edge.source;
            route.target_node = edge.target;
            shift(route.start);
            shift(route.end);
            for (auto &point : route.bends)
                shift(point);
            result.routes.push_back(std::move(route));
        }
        const auto append_labels = [&](auto &source, auto &target) {
            for (auto label : source) {
                label.original_edge = component.edges[label.original_edge];
                label.x += dx;
                label.y += dy;
                target.push_back(label);
            }
        };
        append_labels(layout.center_labels, result.center_labels);
        append_labels(layout.end_labels, result.end_labels);
    }
    std::ranges::sort(result.routes, {}, &ElkRestoredRoute::original_edge);
    std::ranges::sort(result.center_labels, {}, &ElkLabelPosition::id);
    std::ranges::sort(result.end_labels, {}, &ElkLabelPosition::id);
    return result;
}
} // namespace folia::mermaid
