// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_greedy_switch;
import std;
export import folia.mermaid.elk_long_edges;
export import folia.mermaid.elk_random;

export namespace folia::mermaid {
// Original TWO_SIDED_GREEDY_SWITCH stage on a prepared independent graph.
// Runs after BARYCENTER and before self-loop port restoration. Node and port
// identities remain stable; layers and clockwise port lists carry the result.
// Accepts the same unshared E/W port boundary as the ordinary layer sweep,
// including label/long-edge dummies and same-side in-layer edges.
[[nodiscard]] std::expected<std::size_t,std::string> elk_greedy_switch(
    ElkLayeredGraph& graph,ElkRandom& random);
}
