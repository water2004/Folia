// Mermaid's directional containers use ELK SEPARATE_CHILDREN. Each isolated
// graph runs the existing layered pipeline in its own coordinate system;
// INCLUDE_CHILDREN graphs retain the shared hierarchy/port-ordering pipeline.
// Included in flowchart_elk's private namespace.
// SPDX-License-Identifier: MIT
std::vector<bool> separate_group_scopes(const FlowchartInput& source,
    const ElkCompoundInput& hierarchy) {
    const auto root = hierarchy.nodes.size();
    const auto parent = [&](std::size_t node) { return hierarchy.nodes[node].parent.value_or(root); };
    std::vector<bool> separate(root);
    for (std::size_t i = 0; i < root; ++i)
        separate[i] = hierarchy.nodes[i].group && !source.nodes[i].direction.empty();
    // configureCrossHierarchyEdges/setIncludeChildrenPolicy walks both ends
    // through their common ancestor, including that ancestor. A boundary
    // endpoint is different from an edge to a descendant inside the boundary.
    for (const auto& edge : hierarchy.edges) {
        if (parent(edge.source) == parent(edge.target)) continue;
        std::unordered_set<std::size_t> ancestors;
        for (auto node = edge.source; node != root; node = parent(node)) ancestors.insert(node);
        auto common = edge.target;
        while (common != root && !ancestors.contains(common)) common = parent(common);
        for (auto endpoint : {edge.source, edge.target}) {
            for (auto node = endpoint; node != root; node = parent(node)) {
                separate[node] = false;
                if (node == common) break;
            }
        }
    }
    return separate;
}

std::expected<ElkCompoundLayoutResult, std::string> layout_directed_scopes(
    const ElkCompoundLayoutInput& input, const ElkFlatLayoutOptions& root_options,
    const std::vector<bool>& separate) {
    const auto root = input.nodes.size();
    std::vector<std::size_t> owners(root, root), local_node(root), local_edge(input.hierarchy.edges.size());
    std::vector<std::vector<std::size_t>> nodes(root + 1), edges(root + 1);
    for (std::size_t n = 0; n < root; ++n) {
        for (auto parent = input.hierarchy.nodes[n].parent; parent;
             parent = input.hierarchy.nodes[*parent].parent) {
            if (separate[*parent]) { owners[n] = *parent; break; }
        }
        local_node[n] = nodes[owners[n]].size();
        nodes[owners[n]].push_back(n);
    }
    for (std::size_t e = 0; e < input.hierarchy.edges.size(); ++e) {
        const auto& edge = input.hierarchy.edges[e];
        const auto owner = owners[edge.source];
        if (owner != owners[edge.target])
            return std::unexpected("An isolated ELK subgraph has a cross-boundary edge");
        local_edge[e] = edges[owner].size();
        edges[owner].push_back(e);
    }

    ElkCompoundLayoutResult result;
    result.nodes.resize(root);
    std::vector<ElkMeasuredNode> scope_sizes(root + 1);
    const auto solve = [&](auto&& self, std::size_t owner) -> std::expected<void, std::string> {
        auto options = owner == root ? root_options : input.group_options[owner].value_or(root_options);
        if (owner != root) {
            options.direction = input.hierarchy.nodes[owner].direction;
            // The owning container's asymmetric padding/title strip is applied
            // below, after its independent directional layout has been solved.
            options.padding = 0;
        }
        ElkCompoundLayoutInput local;
        local.hierarchy.direction = options.direction;
        local.hierarchy.edge_spacing = input.hierarchy.edge_spacing;
        local.group_boxes.resize(nodes[owner].size());
        local.group_options.resize(nodes[owner].size());
        for (const auto n : nodes[owner]) {
            auto node = input.hierarchy.nodes[n];
            if (node.parent && *node.parent != owner) node.parent = local_node[*node.parent];
            else node.parent.reset();
            auto measured = input.nodes[n];
            if (separate[n]) {
                if (auto nested = self(self, n); !nested) return std::unexpected(nested.error());
                node.group = false;
                measured = scope_sizes[n];
            } else {
                local.group_boxes[local_node[n]] = input.group_boxes[n];
                local.group_options[local_node[n]] = input.group_options[n];
            }
            local.hierarchy.nodes.push_back(node);
            local.nodes.push_back(measured);
        }
        for (const auto e : edges[owner]) {
            const auto& edge = input.hierarchy.edges[e];
            local.hierarchy.edges.push_back({local_node[edge.source], local_node[edge.target]});
        }
        for (auto label : input.center_labels) {
            if (owners[input.hierarchy.edges[label.original_edge].source] != owner) continue;
            label.original_edge = local_edge[label.original_edge];
            local.center_labels.push_back(label);
        }
        for (auto label : input.end_labels) {
            if (owners[input.hierarchy.edges[label.original_edge].source] != owner) continue;
            label.original_edge = local_edge[label.original_edge];
            local.end_labels.push_back(label);
        }
        // The original non-hierarchical wrapper enables greedy switch below
        // its default 40-node threshold. Keep included child graphs on their
        // own hierarchical pipeline; components freeze the decision before
        // their DFS split, so it is never recalculated from a smaller piece.
        if(owner!=root&&!std::ranges::any_of(local.hierarchy.nodes,&ElkCompoundInputNode::group))
            options.hierarchical_layout=false;
        auto placed = owner == root ? elk_layout_compound(local, options)
                                    : elk_layout_components(local, options);
        if (!placed) return std::unexpected(placed.error());
        ElkRoutingPoint offset;
        auto width = placed->width, height = placed->height;
        if (owner != root) {
            const auto& box = input.group_boxes[owner];
            const auto padding = elk_group_content_padding(box);
            ElkCompoundContent content{.width = width, .height = height,
                .padding_left = padding.left, .padding_right = padding.right,
                .padding_top = padding.top, .padding_bottom = padding.bottom};
            auto minimum_width = input.nodes[owner].width;
            if (box.title) minimum_width = std::max(minimum_width, box.title->width + 2 * box.title->padding);
            // SEPARATE_CHILDREN uses ELK's default top-left content alignment;
            // extra width required by a title does not recenter its children.
            elk_resize_compound_content(content, {}, minimum_width, input.nodes[owner].height, false);
            offset = {padding.left + content.offset.x, padding.top + content.offset.y};
            width = content.width + padding.left + padding.right;
            height = content.height + padding.top + padding.bottom;
        }
        scope_sizes[owner] = {width, height};
        for (std::size_t i = 0; i < nodes[owner].size(); ++i) {
            auto box = placed->nodes[i]; box.x += offset.x; box.y += offset.y;
            result.nodes[nodes[owner][i]] = box;
        }
        const auto shift = [&](ElkRoutingPoint& point) { point.x += offset.x; point.y += offset.y; };
        for (auto& route : placed->routes) {
            route.original_edge = edges[owner][route.original_edge];
            const auto& endpoints = input.hierarchy.edges[route.original_edge];
            route.source_node = endpoints.source; route.target_node = endpoints.target;
            shift(route.start); shift(route.end);
            for (auto& point : route.bends) shift(point);
            result.routes.push_back(std::move(route));
        }
        const auto append_labels = [&](auto& from, auto& to) {
            for (auto label : from) {
                label.original_edge = edges[owner][label.original_edge];
                label.x += offset.x; label.y += offset.y; to.push_back(label);
            }
        };
        append_labels(placed->center_labels, result.center_labels);
        append_labels(placed->end_labels, result.end_labels);
        return {};
    };
    if (auto placed = solve(solve, root); !placed) return std::unexpected(placed.error());
    result.width = scope_sizes[root].width; result.height = scope_sizes[root].height;

    // Resolve every origin before translating the stored boxes; otherwise a
    // child could observe a parent that was already moved to root coordinates.
    std::vector<std::optional<ElkRoutingPoint>> origins(root + 1);
    origins[root] = ElkRoutingPoint{};
    const auto origin = [&](auto&& self, std::size_t owner) -> ElkRoutingPoint {
        if (origins[owner]) return *origins[owner];
        const auto parent = self(self, owners[owner]);
        const auto& box = result.nodes[owner];
        return *(origins[owner] = ElkRoutingPoint{parent.x + box.x, parent.y + box.y});
    };
    for (const auto owner : owners) origin(origin, owner);
    for (std::size_t n = 0; n < root; ++n) {
        const auto offset = *origins[owners[n]];
        result.nodes[n].x += offset.x; result.nodes[n].y += offset.y;
    }
    for (auto& route : result.routes) {
        const auto offset = *origins[owners[route.source_node]];
        const auto shift = [&](ElkRoutingPoint& point) { point.x += offset.x; point.y += offset.y; };
        shift(route.start); shift(route.end);
        for (auto& point : route.bends) shift(point);
    }
    for (auto* labels : {&result.center_labels, &result.end_labels}) {
        for (auto& label : *labels) {
            const auto offset = *origins[owners[input.hierarchy.edges[label.original_edge].source]];
            label.x += offset.x; label.y += offset.y;
        }
        std::ranges::sort(*labels, {}, &ElkLabelPosition::id);
    }
    std::ranges::sort(result.routes, {}, &ElkRestoredRoute::original_edge);
    result.group_titles.resize(root);
    for (std::size_t n = 0; n < root; ++n)
        if (input.hierarchy.nodes[n].group && input.group_boxes[n].title)
            result.group_titles[n] = elk_group_title_box(result.nodes[n], *input.group_boxes[n].title);
    return result;
}
