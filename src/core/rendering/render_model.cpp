module folia.core.render_model;
import std;

namespace folia {

// Define these after InlineRenderItem is complete: constructing/destroying
// the semantic payload instantiates its vector<InlineRenderItem> on libc++.
InlineRenderSemanticPayload const& InlineRenderPayload::semantic() const {
    static const InlineRenderSemanticPayload empty{};
    return semantic_payload ? *semantic_payload : empty;
}

InlineRenderSemanticPayload& InlineRenderPayload::ensure_semantic() {
    if (!semantic_payload) semantic_payload = std::make_shared<InlineRenderSemanticPayload>();
    return *semantic_payload;
}

} // namespace folia
